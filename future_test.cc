#include <thread>
#include <utility>

#include <gtest/gtest.h>

#include "future.h"

namespace futures {

TEST(LazyFutureTest, Callback) {
  bool callback_ran = false;
  {
    ThreadPerTaskExecutor executor;
    Supplier<int> supplier = []() -> Result<int> { return 5; };
    LazyFuture<int> fut(std::move(supplier), &executor);

    std::thread::id main_thread_id = std::this_thread::get_id();
    std::move(fut).ConsumeAsync([&](Result<int> val) {
      callback_ran = true;
      ASSERT_EQ(5, *val);
      ASSERT_NE(main_thread_id, std::this_thread::get_id());
    });
  }
  ASSERT_TRUE(callback_ran);
}

TEST(LazyFutureTest, CallbackError) {
  bool callback_ran = false;
  {
    ThreadPerTaskExecutor executor;
    Supplier<int> supplier = []() -> Result<int> {
      return Status::Invalid("XYZ");
    };
    LazyFuture<int> fut(std::move(supplier), &executor);

    std::move(fut).ConsumeAsync([&](Result<int> val) {
      callback_ran = true;
      ASSERT_FALSE(val.ok());
    });
  }
  ASSERT_TRUE(callback_ran);
}

TEST(LazyFutureTest, MoveToDifferentScope) {
  bool callback_ran = false;
  {
    ThreadPerTaskExecutor executor;
    Supplier<int> supplier = []() -> Result<int> { return 5; };
    LazyFuture<int> fut(std::move(supplier), &executor);
    {
      LazyFuture<int> new_fut = std::move(fut);
      std::move(new_fut).ConsumeAsync([&](Result<int> val) {
        callback_ran = true;
        ASSERT_EQ(5, *val);
      });
    }
  }
  ASSERT_TRUE(callback_ran);
}

} // namespace futures