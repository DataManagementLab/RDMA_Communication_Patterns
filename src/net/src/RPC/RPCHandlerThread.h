

#ifndef SRC_DB_UTILS_RPCHANDLER_H_
#define SRC_DB_UTILS_RPCHANDLER_H_



#include "../utils/Config.h"
#include "RPCMemory.h"
#include "../thread/Thread.h"
#include "RPCVoidHandlerThread.h"



#include "../rdma/RDMAServer.h"



namespace rdma
{
    //templated RPCHandler Class
    template <class MessageType,typename RDMA_API_T>
    class RPCHandlerThread : public RPCVoidHandlerThread<RDMA_API_T>
    {

    public:
        RPCHandlerThread(RDMAServer<ReliableRDMA> *rdmaServer, size_t srqID,
                         size_t maxNumberMsgs,char* rpcbuffer
                          )
                :RPCVoidHandlerThread<RDMA_API_T>(rdmaServer,srqID,sizeof(MessageType),maxNumberMsgs,rpcbuffer)

        {
            m_intermediateRspBuffer = static_cast<MessageType*>(RPCVoidHandlerThread<RDMA_API_T>::m_intermediateRspBufferVoid);


        };

        //constructor without rpcbuffer
        RPCHandlerThread(RDMAServer<RDMA_API_T> *rdmaServer, size_t srqID,
                         size_t maxNumberMsgs
        )
                :RPCVoidHandlerThread<RDMA_API_T>(rdmaServer,srqID,sizeof(MessageType),maxNumberMsgs)

        {

            m_intermediateRspBuffer = static_cast<MessageType*>(RPCVoidHandlerThread<RDMA_API_T>::m_intermediateRspBufferVoid);


        };

        //constructor without rpcbuffer and without srqID
        RPCHandlerThread(RDMAServer<RDMA_API_T> *rdmaServer,
                         size_t maxNumberMsgs
        )
                :RPCVoidHandlerThread<RDMA_API_T>(rdmaServer,sizeof(MessageType),maxNumberMsgs)

        {
            m_intermediateRspBuffer = static_cast<MessageType*>(RPCVoidHandlerThread<RDMA_API_T>::m_intermediateRspBufferVoid);

        };


        ~RPCHandlerThread(){


        };





        void  handleRDMARPCVoid(void *message, NodeID &returnAdd){
            handleRDMARPC(static_cast<MessageType*>(message),returnAdd);
        }

        //This Message needs to be implemented in subclass to handle the messages
        void virtual handleRDMARPC(MessageType* message,NodeID & returnAdd) =0;


        protected:

        MessageType *m_intermediateRspBuffer;



    };




   

} /* namespace rdma */

#endif /* SRC_DB_UTILS_RPCHANDLER_H_ */