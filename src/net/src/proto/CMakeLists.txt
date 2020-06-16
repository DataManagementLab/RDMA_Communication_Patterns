find_package(ZMQ REQUIRED)
set(NET_PROTO_SRC
  ProtoSocket.h
  ProtoClient.h
  ProtoServer.h
  ProtoSocket.cc
  ProtoClient.cc
  ProtoServer.cc
  ProtoSendSocket.h
  ProtoSendSocket.cc
  ) # Adding headers required for portability reasons http://voices.canonical.com/jussi.pakkanen/2013/03/26/a-list-of-common-cmake-antipatterns/
add_library(rdma_proto ${NET_PROTO_SRC})
target_include_directories(rdma_proto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rdma_proto
  net_message
  thread
  ${ZMQ_LIBRARIES}
  )
