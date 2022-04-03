#include <benchmark/benchmark.h>

#include <thread>

#include "future.h"

namespace futures {

void Callback(const Status &status) {
  benchmark::DoNotOptimize(status.ok());
  benchmark::ClobberMemory();
}

void DirectCallMarkFinished(Result<internal::Empty> result) {
  Callback(result.status());
}

static void BM_FutureThenEmpty(benchmark::State &state) {
  for (auto _ : state) {
    auto future = Future<>::Make();
    auto fut2 = future.Then([]() { return Status::OK(); });
    fut2.AddCallback([](const Status &status) { Callback(status); });
    future.MarkFinished(Status::OK());
  }
}
BENCHMARK(BM_FutureThenEmpty)->Threads(16);

static void BM_FutureCallbackEmpty(benchmark::State &state) {
  for (auto _ : state) {
    auto future = Future<>::Make();
    future.AddCallback([](const Status &status) { Callback(status); });
    future.MarkFinished(Status::OK());
  }
}
BENCHMARK(BM_FutureCallbackEmpty)->Threads(16);

static void BM_LazyFutureCallbackEmpty(benchmark::State &state) {
  InlineExecutor executor;
  for (auto _ : state) {
    LazyFuture<void> future([] { return Status::OK(); }, &executor);
    std::move(future).ConsumeAsync([](Status status) { Callback(status); });
  }
}
BENCHMARK(BM_LazyFutureCallbackEmpty)->Threads(16);

static void BM_FutureAlreadyFinishedEmpty(benchmark::State &state) {
  for (auto _ : state) {
    auto future = Future<>::Make();
    future.MarkFinished();
    future.AddCallback([](const Status &status) { Callback(status); });
  }
}
BENCHMARK(BM_FutureAlreadyFinishedEmpty)->Threads(16);

static void BM_DirectCallEmpty(benchmark::State &state) {
  for (auto _ : state) {
    auto res = internal::Empty::ToResult(Status::OK());
    DirectCallMarkFinished(std::move(res));
  }
}
BENCHMARK(BM_DirectCallEmpty)->Threads(16);

void CallbackSharedPtr(std::shared_ptr<int> foo) {
  benchmark::DoNotOptimize(foo.get());
  benchmark::ClobberMemory();
}

static void BM_FutureCallbackSharedPtr(benchmark::State &state) {
  for (auto _ : state) {
    auto future = Future<std::shared_ptr<int>>::Make();
    future.AddCallback([](const Result<std::shared_ptr<int>> &res) {
      CallbackSharedPtr(*res);
    });
    future.MarkFinished(std::make_shared<int>(0));
  }
}
BENCHMARK(BM_FutureCallbackSharedPtr)->Threads(16);

static void BM_LazyFutureCallbackSharedPtr(benchmark::State &state) {
  InlineExecutor executor;
  for (auto _ : state) {
    Supplier<std::shared_ptr<int>> supplier =
        []() -> Result<std::shared_ptr<int>> {
      return std::make_shared<int>(0);
    };
    LazyFuture<std::shared_ptr<int>> future(std::move(supplier), &executor);
    std::move(future).ConsumeAsync(
        [](Result<std::shared_ptr<int>> res) { CallbackSharedPtr(*res); });
  }
}
BENCHMARK(BM_LazyFutureCallbackSharedPtr)->Threads(16);

static void BM_FutureAlreadyFinishedSharedPtr(benchmark::State &state) {
  for (auto _ : state) {
    auto future = Future<std::shared_ptr<int>>::Make();
    future.MarkFinished(std::make_shared<int>(0));
    future.AddCallback([](const Result<std::shared_ptr<int>> &res) {
      CallbackSharedPtr(*res);
    });
  }
}
BENCHMARK(BM_FutureAlreadyFinishedSharedPtr)->Threads(16);

static void BM_DirectCallSharedPtr(benchmark::State &state) {
  for (auto _ : state) {
    Result<std::shared_ptr<int>> res = std::make_shared<int>(0);
    CallbackSharedPtr(std::move(res).MoveValueUnsafe());
  }
}
BENCHMARK(BM_DirectCallSharedPtr)->Threads(16);

static void BM_FutureThenSharedPtr(benchmark::State &state) {
  for (auto _ : state) {
    auto future = Future<std::shared_ptr<int>>::Make();
    auto fut2 =
        future.Then([](const std::shared_ptr<int> &val) { return val; });
    fut2.AddCallback([](const Result<std::shared_ptr<int>> &res) {
      CallbackSharedPtr(*res);
    });
    future.MarkFinished(std::make_shared<int>(0));
  }
}
BENCHMARK(BM_FutureThenSharedPtr)->Threads(16);

static void BM_LazyFutureThenSharedPtr(benchmark::State &state) {
  InlineExecutor executor;
  for (auto _ : state) {
    Supplier<std::shared_ptr<int>> supplier =
        []() -> Result<std::shared_ptr<int>> {
      return std::make_shared<int>(0);
    };
    LazyFuture<std::shared_ptr<int>> future(std::move(supplier), &executor);
    auto fut2 = std::move(future).Then<std::shared_ptr<int>>(
        [](Result<std::shared_ptr<int>> val) { return val; });
    std::move(fut2).ConsumeAsync(
        [](Result<std::shared_ptr<int>> res) { CallbackSharedPtr(*res); });
  }
}
BENCHMARK(BM_LazyFutureThenSharedPtr)->Threads(16);

void ReceiveFuture(Future<> fut) {
  benchmark::DoNotOptimize(fut.is_finished());
  benchmark::ClobberMemory();
}

static void BM_CreateMove(benchmark::State &state) {
  for (auto _ : state) {
    Future<> fut = Future<>::Make();
    ReceiveFuture(std::move(fut));
  }
}
BENCHMARK(BM_CreateMove)->Threads(16);

static void BM_CreateOnly(benchmark::State &state) {
  for (auto _ : state) {
    Future<> fut = Future<>::Make();
    benchmark::DoNotOptimize(fut);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_CreateOnly)->Threads(16);

} // namespace futures

BENCHMARK_MAIN();
