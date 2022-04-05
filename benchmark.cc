#include <benchmark/benchmark.h>

#include <thread>

#include "future.h"

constexpr int kNumThreads = 16;

namespace futures {

void Callback(const Status &status) {
  benchmark::DoNotOptimize(status.ok());
  benchmark::ClobberMemory();
}

void DirectCallMarkFinished(Result<internal::Empty> result) {
  Callback(result.status());
}

static void BM_LazyFutureCallbackEmpty(benchmark::State &state) {
  InlineExecutor executor;
  for (auto _ : state) {
    LazyFuture<void> future([] { return Status::OK(); }, &executor);
    std::move(future).ConsumeAsync([](Status status) { Callback(status); });
  }
}
BENCHMARK(BM_LazyFutureCallbackEmpty)->Threads(kNumThreads);

static void BM_DirectCallEmpty(benchmark::State &state) {
  for (auto _ : state) {
    auto res = internal::Empty::ToResult(Status::OK());
    DirectCallMarkFinished(std::move(res));
  }
}
BENCHMARK(BM_DirectCallEmpty)->Threads(kNumThreads);

void CallbackSharedPtr(std::shared_ptr<int> foo) {
  benchmark::DoNotOptimize(foo.get());
  benchmark::ClobberMemory();
}

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
BENCHMARK(BM_LazyFutureCallbackSharedPtr)->Threads(kNumThreads);

static void BM_DirectCallSharedPtr(benchmark::State &state) {
  for (auto _ : state) {
    Result<std::shared_ptr<int>> res = std::make_shared<int>(0);
    CallbackSharedPtr(std::move(res).MoveValueUnsafe());
  }
}
BENCHMARK(BM_DirectCallSharedPtr)->Threads(kNumThreads);

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
BENCHMARK(BM_LazyFutureThenSharedPtr)->Threads(kNumThreads);

} // namespace futures

BENCHMARK_MAIN();
