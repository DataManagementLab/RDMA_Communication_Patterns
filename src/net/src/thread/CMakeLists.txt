set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(THREAD_SRCS
  Thread.h
  Thread.cc
  ) # Adding headers required for portability reasons http://voices.canonical.com/jussi.pakkanen/2013/03/26/a-list-of-common-cmake-antipatterns/

add_library(thread ${THREAD_SRCS})
target_include_directories(thread PUBLIC ${CMAKE_CURRENT_BINARY_PATH})
target_link_libraries(thread
  ${CMAKE_THREAD_LIBS_INIT}
  rdma_utils
  )




