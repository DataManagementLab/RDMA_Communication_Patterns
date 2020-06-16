find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS})
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")
PROTOBUF_GENERATE_CPP(PROTO_SRC PROTO_HDRS ${PROTO_FILES})
set(NET_MESSAGE_SRC
  ProtoMessageFactory.h
  ) # Adding headers required for portability reasons http://voices.canonical.com/jussi.pakkanen/2013/03/26/a-list-of-common-cmake-antipatterns/


set(ProtobufIncludePath ${CMAKE_CURRENT_BINARY_DIR}
  CACHE INTERNAL "Path to generated protobuf files.")

add_library(net_message ${NET_MESSAGE_SRC} ${PROTO_SRC} ${PROTO_HDRS})
target_include_directories(net_message PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(net_message
  ${PROTOBUF_LIBRARIES}
)
