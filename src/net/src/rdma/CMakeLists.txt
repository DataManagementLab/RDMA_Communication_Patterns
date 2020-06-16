set(NET_RDMA_SRC
  BaseRDMA.h
  BaseRDMA.cc
  ReliableRDMA.h
  ReliableRDMA.cc
  UnreliableRDMA.h
  UnreliableRDMA.cc
  RDMAServer.h
  RDMAClient.h
  NodeIDSequencer.h
  NodeIDSequencer.cc
) # Adding headers required for portability reasons http://voices.canonical.com/jussi.pakkanen/2013/03/26/a-list-of-common-cmake-antipatterns/
add_library(rdma_lib ${NET_RDMA_SRC})
target_include_directories(rdma_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(IBVerbs)
target_link_libraries(rdma_lib ${IBVERBS_LIBRARY})
find_package(RdmaCm)
target_link_libraries(rdma_lib ${RDMACM_LIBRARY})
find_package(Numa)
target_link_libraries(rdma_lib ${NUMA_LIBRARY})

target_link_libraries(rdma_lib
  ${RDMA_LINKER_FLAGS}
  rdma_utils
  net_message
  rdma_proto
)