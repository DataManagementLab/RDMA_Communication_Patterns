set(NET_RPC_SRC
        # RPCHandlerThread.h
        RPCMemory.h
        RPCVoidHandlerThread.h
        ) # Adding headers required for portability reasons http://voices.canonical.com/jussi.pakkanen/2013/03/26/a-list-of-common-cmake-antipatterns/
add_library(net_rpc ${NET_RPC_SRC})
target_include_directories(net_rpc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

#what else
target_link_libraries(net_rpc thread rdma_lib)



