#ifndef SRC_DB_UTILS_RPCMEMORY_H_
#define SRC_DB_UTILS_RPCMEMORY_H_

#include "../utils/Config.h"

namespace rdma {

/*
 * Class managing the buffer for Send Recv calls
 */
    class RPCMemory {
    public:

        RPCMemory(char* buffer, size_t msgSize, uint32_t maxMsgs)
                : m_localBuffer(buffer),
                  m_msgSize(msgSize),
                  m_maxNumberMsgs(maxMsgs) {};

        ~RPCMemory(){};


        char* getNext() {
            auto retPtr = (m_localBuffer + ((m_msgSize * (m_index % m_maxNumberMsgs))));
            m_index++;
            return retPtr;
        };

        char* bufferAdd(){
            return m_localBuffer;
        }

    private:
        char* m_localBuffer;
        size_t m_msgSize;
        uint32_t m_maxNumberMsgs;

        uint32_t m_index = 0;
    };

} /* namespace rdma */

#endif /* SRC_DB_UTILS_RPCMEMORY_H_ */