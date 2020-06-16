enable_language(CXX)
set(BENCHMARK_SRC
  BenchmarkSender.h
  BenchmarkReceiver.h
  BenchmarkDriver.h
  BenchmarkDriver.cc
  ) # Adding headers required for portability reasons http://voices.canonical.com/jussi.pakkanen/2013/03/26/a-list-of-common-cmake-antipatterns/
add_library(benchmark ${BENCHMARK_SRC})
target_include_directories(benchmark PUBLIC ./)
target_link_libraries(benchmark rdma_lib utils gflags)
