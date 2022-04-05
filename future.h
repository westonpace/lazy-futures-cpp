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

#pragma once

#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "result.h"
#include "status.h"

namespace futures {

// template <typename R, typename... A> using FuncType = internal::FnOnce<R, A...>;
template <typename Sig> using FuncType = std::function<Sig>;

using Task = FuncType<void()>;
class Executor {
public:
  virtual void Spawn(Task task) = 0;
};

class ThreadPerTaskExecutor : public Executor {
public:
  virtual ~ThreadPerTaskExecutor() {
    for (auto *thread : threads) {
      thread->join();
      delete thread;
    }
  }
  void Spawn(Task task) override {
    threads.push_back(new std::thread(std::move(task)));
  }
  std::vector<std::thread *> threads;
};

class InlineExecutor : public Executor {
  void Spawn(Task task) override { std::move(task)(); }
};

template <typename T> using Consumer = FuncType<void(Result<T>)>;

template <typename T>
class Promise {
    Promise(Consumer<T> callback) : callback_(std::move(callback)) {}
    Promise(const Consumer<T>&) = delete;
    Promise& operator=(const Consumer<T>&) = delete;
    Promise(Consumer<T>&&) = default;
    Promise& operator=(Consumer<T>&&) = default;
    ~Promise() {
        if (callback_) {
            std::move(callback_)(Status::Invalid("Abandoned promise"));
        }
    }
    void Fulfill(Result<T> val) {
        std::move(callback_)(std::move(val));
        callback_ = {};
    }
private:
    Consumer<T> callback_;
};

template <typename T> using Supplier = FuncType<void(Promise<T>)>;
template <typename T, typename V>
using MapTask = FuncType<Result<V>(Result<T>)>;
template <typename T> using MapTaskVoid = FuncType<Status(Result<T>)>;

using VoidSupplier = FuncType<Status()>;
using VoidConsumer = FuncType<void(Status)>;
template <typename V> using VoidMapTask = FuncType<Result<V>(Status)>;
using VoidMapTaskVoid = FuncType<Status(Status)>;

template <typename T, typename V>
Supplier<V> Compose(Supplier<T> supplier, MapTask<T, V> continuation) {
  struct Composition {
    Result<V> operator()() {
      return std::move(continuation)(std::move(supplier)());
    }
    Supplier<T> supplier;
    MapTask<T, V> continuation;
  };
  return Supplier<V>(Composition{std::move(supplier), std::move(continuation)});
}

template <typename T>
VoidSupplier ComposeVoid(Supplier<T> supplier,
                         MapTaskVoid<T> continuation_void) {
  struct Composition {
    Status operator()() {
      return std::move(continuation_void)(std::move(supplier)());
    }
    Supplier<T> supplier;
    MapTaskVoid<T> continuation_void;
  };
  return VoidSupplier(
      Composition{std::move(supplier), std::move(continuation_void)});
}

template <typename V>
Supplier<V> VoidCompose(VoidSupplier supplier, VoidMapTask<V> continuation) {
  struct Composition {
    Result<V> operator()() {
      return std::move(continuation)(std::move(supplier)());
    }
    VoidSupplier supplier;
    VoidMapTask<V> continuation;
  };
  return Supplier<V>(Composition{std::move(supplier), std::move(continuation)});
}

inline VoidSupplier VoidComposeVoid(VoidSupplier supplier,
                                    VoidMapTaskVoid continuation) {
  struct Composition {
    Status operator()() {
      return std::move(continuation)(std::move(supplier)());
    }
    VoidSupplier supplier;
    VoidMapTaskVoid continuation;
  };
  return VoidSupplier(
      Composition{std::move(supplier), std::move(continuation)});
}

template <typename T> class LazyFuture;

template <> class LazyFuture<void> {
public:
  LazyFuture(VoidSupplier supplier, Executor *executor)
      : supplier_(std::move(supplier)), executor_(std::move(executor)) {}

  void ConsumeAsync(VoidConsumer consumer) && {
    struct VoidFutureRunningTask {
      void operator()() { std::move(consumer)(std::move(supplier)()); }
      VoidSupplier supplier;
      VoidConsumer consumer;
    };
    Executor *executor = executor_;
    VoidFutureRunningTask task{std::move(supplier_), std::move(consumer)};
    executor->Spawn(std::move(task));
  }

  template <typename V> LazyFuture<V> Then(VoidMapTask<V> map_func) && {
    Supplier<V> continued =
        VoidCompose<V>(std::move(supplier_), std::move(map_func));
    return LazyFuture<V>(std::move(continued), executor_);
  }

  LazyFuture<void> ThenVoid(VoidMapTaskVoid map_func) && {
    VoidSupplier continued =
        VoidComposeVoid(std::move(supplier_), std::move(map_func));
    return LazyFuture<void>(std::move(continued), executor_);
  }

private:
  VoidSupplier supplier_;
  Executor *executor_;
};

template <typename T> class LazyFuture {
public:
  LazyFuture(Supplier<T> supplier, Executor *executor)
      : supplier_(std::move(supplier)), executor_(executor) {}

  void ConsumeAsync(Consumer<T> consumer) && {
    struct FutureRunningTask {
      void operator()() { std::move(consumer)(std::move(supplier)()); }
      Supplier<T> supplier;
      Consumer<T> consumer;
    };
    Executor *executor = executor_;
    FutureRunningTask task{std::move(supplier_), std::move(consumer)};
    executor->Spawn(std::move(task));
  }

  template <typename V> LazyFuture<V> Then(MapTask<T, V> map_func) && {
    Supplier<V> continued =
        Compose<T, V>(std::move(supplier_), std::move(map_func));
    return LazyFuture<V>(std::move(continued), executor_);
  }

  template <typename V>
  LazyFuture<V> ThenFuture(MapTask<T, LazyFuture<V>> map_func) && {}

  LazyFuture<void> ThenVoid(MapTaskVoid<T> map_func) && {
    VoidSupplier continued =
        ComposeVoid<T>(std::move(supplier_), std::move(map_func));
    return LazyFuture<void>(std::move(continued), executor_);
  }

private:
  Supplier<T> supplier_;
  Executor *executor_;
};

// template <typename T>
// LazyFuture<std::vector<Result<T>>>
// All(const std::vector<LazyFuture<T>> &futures) {
//   struct Supplier {
//     std::vector<Result<T>> operator()() {
//       std::vector<T> results;
//     }
//     std::vector<LazyFuture<T>> futures;
//   };
// }

} // namespace futures
