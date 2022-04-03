// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "future.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <numeric>

namespace futures {

// Shared mutex for all FutureWaiter instances.
// This simplifies lock management compared to a per-waiter mutex.
// The locking order is: global waiter mutex, then per-future mutex.
//
// It is unlikely that many waiter instances are alive at once, so this
// should ideally not limit scalability.
static std::mutex global_waiter_mutex;

class ConcreteFutureImpl : public FutureImpl {
public:
  void DoMarkFinished() { DoMarkFinishedOrFailed(FutureState::SUCCESS); }

  void DoMarkFailed() { DoMarkFinishedOrFailed(FutureState::FAILURE); }

  void AddCallback(Callback callback) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (IsFutureFinished(state_)) {
      lock.unlock();
      std::move(callback)(*this);
    } else {
      callbacks_.push_back(std::move(callback));
    }
  }

  bool TryAddCallback(const std::function<Callback()>& callback_factory) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (IsFutureFinished(state_)) {
      return false;
    } else {
      callbacks_.push_back(callback_factory());
      return true;
    }
  }

  void DoMarkFinishedOrFailed(FutureState state) {
    {
      // Lock the hypothetical waiter first, and the future after.
      // This matches the locking order done in FutureWaiter constructor.
      std::unique_lock<std::mutex> waiter_lock(global_waiter_mutex);
      std::unique_lock<std::mutex> lock(mutex_);

      state_ = state;
    }
    cv_.notify_all();

    auto callbacks = std::move(callbacks_);
    auto self = shared_from_this();

    // run callbacks, lock not needed since the future is finished by this
    // point so nothing else can modify the callbacks list and it is safe
    // to iterate.
    //
    // In fact, it is important not to hold the locks because the callback
    // may be slow or do its own locking on other resources
    for (auto& callback : callbacks) {
      std::move(callback)(*self);
    }
  }

  void DoWait() {
    std::unique_lock<std::mutex> lock(mutex_);

    cv_.wait(lock, [this] { return IsFutureFinished(state_); });
  }

  bool DoWait(double seconds) {
    std::unique_lock<std::mutex> lock(mutex_);

    cv_.wait_for(lock, std::chrono::duration<double>(seconds),
                 [this] { return IsFutureFinished(state_); });
    return IsFutureFinished(state_);
  }

  std::mutex mutex_;
  std::condition_variable cv_;
};

namespace {

ConcreteFutureImpl* GetConcreteFuture(FutureImpl* future) {
  return dynamic_cast<ConcreteFutureImpl*>(future);
}

}  // namespace

std::unique_ptr<FutureImpl> FutureImpl::Make() {
  return std::unique_ptr<FutureImpl>(new ConcreteFutureImpl());
}

std::unique_ptr<FutureImpl> FutureImpl::MakeFinished(FutureState state) {
  std::unique_ptr<ConcreteFutureImpl> ptr(new ConcreteFutureImpl());
  ptr->state_ = state;
  return ptr;
}

FutureImpl::FutureImpl() : state_(FutureState::PENDING) {}

void FutureImpl::Wait() { GetConcreteFuture(this)->DoWait(); }

bool FutureImpl::Wait(double seconds) { return GetConcreteFuture(this)->DoWait(seconds); }

void FutureImpl::MarkFinished() { GetConcreteFuture(this)->DoMarkFinished(); }

void FutureImpl::MarkFailed() { GetConcreteFuture(this)->DoMarkFailed(); }

void FutureImpl::AddCallback(Callback callback) {
  GetConcreteFuture(this)->AddCallback(std::move(callback));
}

bool FutureImpl::TryAddCallback(const std::function<Callback()>& callback_factory) {
  return GetConcreteFuture(this)->TryAddCallback(callback_factory);
}

Future<> AllComplete(const std::vector<Future<>>& futures) {
  struct State {
    explicit State(int64_t n_futures) : mutex(), n_remaining(n_futures) {}

    std::mutex mutex;
    std::atomic<size_t> n_remaining;
  };

  if (futures.empty()) {
    return Future<>::MakeFinished();
  }

  auto state = std::make_shared<State>(futures.size());
  auto out = Future<>::Make();
  for (const auto& future : futures) {
    future.AddCallback([state, out](const Status& status) mutable {
      if (!status.ok()) {
        std::unique_lock<std::mutex> lock(state->mutex);
        if (!out.is_finished()) {
          out.MarkFinished(status);
        }
        return;
      }
      if (state->n_remaining.fetch_sub(1) != 1) return;
      out.MarkFinished();
    });
  }
  return out;
}

Future<> AllFinished(const std::vector<Future<>>& futures) {
  return All(futures).Then([](const std::vector<Result<internal::Empty>>& results) {
    for (const auto& res : results) {
      if (!res.ok()) {
        return res.status();
      }
    }
    return Status::OK();
  });
}

}  // namespace arrow
