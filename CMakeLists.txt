cmake_minimum_required(VERSION 3.10)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

# set the project name
project(FutureBenchmarks VERSION 1.0 LANGUAGES CXX)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    message("Windows build: CMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}")
else()
    message("Linux build")
    set(CMAKE_CXX_FLAGS_DEBUG "-g")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
endif()

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(benchmark REQUIRED)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(futures status.cc result.cc future.cc benchmark.cc)
target_link_libraries(futures benchmark::benchmark)

enable_testing()

add_executable(
  future_test
  future.cc
  result.cc
  status.cc
  future_test.cc
)
target_link_libraries(
  future_test
  gtest_main
)

add_executable(
  noalloc
  noalloc.cc
  future.cc
  status.cc
  result.cc
)

include(GoogleTest)
gtest_discover_tests(future_test)
