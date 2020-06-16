enable_testing()
include(GNUInstallDirs)

add_subdirectory(utils)
add_subdirectory(net/src)
add_subdirectory(benchmark)


# set(CMAKE_CXX_CLANG_TIDY
#   clang-tidy;
#   -header-filter=.;
#   -checks=*;
#   -warnings-as-errors=*;)


add_executable(bench main.cc)
add_executable(benchSharedMemory sharedMemMain.cc)
add_executable(benchDispatcher dispatcherMain.cc)

set_target_properties(rdma_lib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO   
)



set_target_properties(bench PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

set_target_properties(benchSharedMemory PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

set_target_properties(benchDispatcher PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_compile_options(rdma_lib PRIVATE -O3 -g)

target_compile_options(bench PRIVATE -O3 -Wall -Wextra -Werror -pthread -g)

target_compile_options(benchSharedMemory PRIVATE -O3 -Wall -Wextra -Werror -pthread -g)

target_compile_options(benchDispatcher PRIVATE -O3 -Wall -Wextra -Werror -pthread -g)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g" )

message("rdma proto msg path: " ${ProtobufIncludePath})


target_link_libraries(bench
  rdma_lib
  benchmark
  gflags
  # asan
)

target_link_libraries(benchSharedMemory
  rdma_lib
  benchmark
  gflags
  
  # asan
)


target_link_libraries(benchDispatcher
  rdma_lib
  benchmark
  gflags
  # asan
)


find_package(GoogleBenchmark REQUIRED)

find_package(gflags REQUIRED)

# Include libraries we depend on
find_package(IBVerbs REQUIRED)
find_package(RdmaCm REQUIRED)
find_package(Numa REQUIRED)
find_package(Protobuf REQUIRED)
find_package(ZMQ REQUIRED)
find_package(Threads REQUIRED)



message("CMAKE INSTALL INCLUDEDIR" ${CMAKE_INSTALL_INCLUDEDIR} )

if(USE_VTUNE)
  add_definitions(-DUSE_VTUNE)
  
  if(${INTELAMPLIFIER_FOUND})
    target_include_directories(bench PUBLIC ${INTELAMPLIFIER_INCLUDE_DIRS})
    target_link_libraries(bench ${INTELAMPLIFIER_LIBITTNOTIFY} -ldl)
    
  else()
    message(WARNING "Require VTUNE package")
  endif()
endif()


