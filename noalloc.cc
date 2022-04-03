#include "future.h"

int main() {
  futures::InlineExecutor executor;
  futures::Supplier<int> supplier = []() -> futures::Result<int> { return 5; };
  futures::LazyFuture<int> fut(std::move(supplier), &executor);
  std::move(fut).ConsumeAsync(
      [&](futures::Result<int> val) { val.ValueOrDie(); });
  return 0;
}
