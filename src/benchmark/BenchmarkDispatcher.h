#pragma once

#include "RDMAClientSRQ.h"


#include <stdexcept>

/* 
   T must implement operator=, copy ctor 
*/




unsigned long upper_power_of_two(unsigned long v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;

}



template<typename T> class CircBuf {
   // don't use default ctor
   const int size;
   T *data;
   int front;
   int count;
public:
   CircBuf(int);
   CircBuf() = delete;
   ~CircBuf();

   bool empty() { return count == 0; }
   bool full() { return count == size; }
   size_t getSize() { return count; }
   bool add(const T&);
   void remove(T*);
   int getCurrentIndex(){
      return (front + count) & (size - 1);
   }
};

template<typename T> CircBuf<T>::CircBuf(int sz): size(sz) {
   if (sz==0) throw std::invalid_argument("size cannot be zero");
   data = new T[sz];
   front = 0;
   count = 0;
}
template<typename T> CircBuf<T>::~CircBuf() {
   delete[] data;
}


// returns true if add was successful, false if the buffer is already full
template<typename T> bool CircBuf<T>::add(const T &t) {
      int end = (front + count) & (size - 1);
      data[end] = t;
      count++;
      return true;
}

// returns true if there is something to remove, false otherwise
template<typename T> void CircBuf<T>::remove(T *t) {
      *t = data[front];
      front =  (front + 1) & (size -1);
      count--;
 }


template<RdmaPoll P,RdmaTransfer T, size_t PL_SIZE>
class BenchmarkDispatcher : public rdma::RDMAClientSRQ<rdma::ReliableRDMA>
{
  protected:
   
   using ResponseT = Response<PL_SIZE>;

   ResponseT* respBuffer_ = nullptr;
   ExperimentMessage* reqBuffer_ = nullptr;
   IncomingSendMailbox* mailboxes_ = nullptr;
   std::atomic<size_t>* received_ = nullptr;
   size_t expectedReceiver_{0};
   size_t localSender_ {0};
   std::string nodeIdSequencerIp_ = rdma::Config::SEQUENCER_IP + ":" + std::to_string(rdma::Config::SEQUENCER_PORT);

   std::atomic<bool> poll_ {false};

   std::size_t reqDispatched = 0 ;
   std::size_t msgReceived = 0;
   
   
  public:

   std::atomic<bool> running{true};
   
   BenchmarkDispatcher(size_t expectedReceiver, size_t localSender) : expectedReceiver_(expectedReceiver), localSender_(localSender){};
   virtual ~BenchmarkDispatcher() = default;
   
   ResponseT* createSharedRDMAMemoryResponses(){
      respBuffer_ = static_cast<ResponseT*>(localAlloc(sizeof(ResponseT) * localSender_));
      return respBuffer_;
   };
   
   ExperimentMessage* createSharedRDMAMemoryRequests(){
      reqBuffer_ =static_cast<ExperimentMessage*>(localAlloc(sizeof(ExperimentMessage) * localSender_));
      return reqBuffer_;
   };
   
   // IncomingSendMailbox* createMailboxes(){
   //    mailboxes_ = new IncomingSendMailbox[localSender_];
   //    return mailboxes_;
   // };

   std::atomic<size_t>*& createMailboxes(){
      received_ = new std::atomic<size_t>[localSender_];

      for(size_t i = 0; i < localSender_ ; i++){
         received_[i] = 0;
      }
      
      return received_;
   };
   
   void receiveBatchSRQ(ResponseT* buffer, size_t numberReceives){
      size_t msgSize = sizeof(ResponseT);
      for (size_t i = 0; i < numberReceives; ++i)
      {
         receiveSRQ(srqId_, i , (void*) &buffer[i], msgSize);
      }
   }
   
   auto queryReceiverInformation(){

      if (!ProtoClient::isConnected(m_sequencerIpPort)) {
         m_ownNodeID = requestNodeID(nodeIdSequencerIp_, m_ownIpPort, m_nodeType);
      }

      std::vector<std::string> receiverConnectionStrings;
      receiverConnectionStrings.reserve(expectedReceiver_);

      while(receiverConnectionStrings.size() != expectedReceiver_){
         auto getAllIDsReq = rdma::ProtoMessageFactory::createGetAllNodeIDsRequest(m_ownNodeID);
         Any rcvAny;
         rdma::ProtoClient::exchangeProtoMsg(nodeIdSequencerIp_, &getAllIDsReq, &rcvAny);
         if (rcvAny.Is<rdma::GetAllNodeIDsResponse>()) {
            rdma::GetAllNodeIDsResponse connResp;
            rcvAny.UnpackTo(&connResp);

            for (int i = 0; i < connResp.nodeid_entries_size(); ++i) {
               auto nodeidEntry = connResp.nodeid_entries(i);
               if(nodeidEntry.name() == "receiver"){
                  
                  if (std::find(receiverConnectionStrings.begin(), receiverConnectionStrings.end(),nodeidEntry.ip()) ==  receiverConnectionStrings.end()){
                     receiverConnectionStrings.push_back(nodeidEntry.ip());
                  }
               }
            }
    
         }else{
            std::cout << "False response"  << "\n";
         }
      }
      
      return receiverConnectionStrings;
   }


   int pollBatch(size_t* retMemoryIdx){
      if constexpr (P == RdmaPoll::BUF){
            int ne = 0;
            for(size_t respId = 0; respId < localSender_; respId++){
               if(respBuffer_[respId].transferFlag != 1)
                  continue;
               retMemoryIdx[ne] = respId;
               ne++;
               respBuffer_[respId].transferFlag = 0;
               return ne;
            }
            
         }
      
      if constexpr (P == RdmaPoll::SRQ){
            int ne = 0;
            size_t nodeId      [16];
            size_t failedPolls {0};
            do{
               ne = pollReceiveSRQBatch(srqId_, nodeId ,retMemoryIdx, poll_);
               failedPolls++;
            }while(ne == 0 && failedPolls < 150);            
            
            
            if(ne > 0)
               return ne;

         }
      return 0;

   }

   bool poll(size_t& retMemoryIdx){


      if constexpr (P == RdmaPoll::BUF){

            for(size_t respId = 0; respId < localSender_; respId++){
               if(respBuffer_[respId].transferFlag != 1)
                  continue;
               retMemoryIdx = respId;
               respBuffer_[respId].transferFlag = 0;
               return true;
            }
            
         }
      
      if constexpr (P == RdmaPoll::SRQ){
      
            size_t nodeId       {0};
    
            size_t failedPolls {0};
            int ne {0};
            do{
               ne = pollReceiveSRQ(srqId_, nodeId ,retMemoryIdx, poll_);
               failedPolls++;
            }while(ne == 0 && failedPolls < 150);

            if(ne > 0)
               return true;

         }
      return false;
   }


   inline void transfer(size_t reqId){
      
      auto* req =  ((ExperimentMessage*)&reqBuffer_[reqId]);
      
      if(T == RdmaTransfer::WRITE){
         write(req->padding_destNodeID, req->padding_destRDMA, (void*) req, sizeof(ExperimentMessage), false);

      }
      if(T == RdmaTransfer::SEND){
         send(req->padding_destNodeID,  (void*) req , sizeof(ExperimentMessage), false);
      }
   }


   inline void finalize(size_t id){
      if(P == RdmaPoll::BUF){
         // reqBuffer_[id].type = MessageType::EMPTY;
      }
      if(P == RdmaPoll::SRQ){
         receiveSRQ(srqId_, id ,(void*) &respBuffer_[id], sizeof(ResponseT));
      }
   }


   void run(){
     
      if constexpr (P == RdmaPoll::SRQ){
            receiveBatchSRQ(respBuffer_, localSender_);
         }

      bool* transfered = new bool[localSender_];

     
      for(size_t i = 0; i < localSender_ ; i++){
         transfered[i] = false;
      }

      // std::queue<size_t> workerIds; 

      // CircBuf<size_t> workerIds(upper_power_of_two(localSender_));
      
      while(running){
         
         // if constexpr (P == RdmaPoll::SRQ){

               
         // }

         for(size_t reqId = 0; reqId < localSender_; reqId++){
            if((reqBuffer_[reqId].type == MessageType::EMPTY) || transfered[reqId])
               continue;
            // send away a
            transfer(reqId);
            reqDispatched++;
            transfered[reqId] = true;

         }
      

      
         size_t responseIdx [16];
         int ne = pollBatch(responseIdx);
         for (int i = 0; i < ne; ++i)
         {

            if(P == RdmaPoll::BUF){
         
               auto rIdx = responseIdx[i];
                       
               size_t expected = 0;
               size_t desired = 1;
               msgReceived++;
               if(transfered[rIdx]){
                  transfered[rIdx] = false;
                  reqBuffer_[rIdx].type = MessageType::EMPTY;
            
                  while(! received_[rIdx].compare_exchange_strong(expected, desired)){
                     std::cout << "Could not compare exchange dispatcher " <<  rIdx <<  " " <<  i  << "\n";
                     for (int j = 0; j < ne; j++){
                        std::cout << "array  dispatcher " <<  responseIdx[j]  << "\n";
                     }
                     std::cout << " " << std::endl;
                     expected = 0;

                  }
                  finalize(rIdx);
            
               }else{
                  std::cout << "Has not been transfered" << std::endl;
               }

            }
            if(P == RdmaPoll::SRQ){

               // transform memIdx in worker Ids
               
               
               // map to worker id
               
               auto diff =respBuffer_[responseIdx[i]].senderNodeId - convertPointerToOffset((void*)respBuffer_);
               size_t workerId = (diff/sizeof(ResponseT));
               // std::cout << "" << workerId << ")  "; 

               size_t expected = 0;
               size_t desired = 1;
               msgReceived++;
               if(transfered[workerId]){
                  transfered[workerId] = false;
                  reqBuffer_[workerId].type = MessageType::EMPTY;
            
                  while(! received_[workerId].compare_exchange_strong(expected, desired)){
                     std::cout << "Could not compare exchange dispatcher " <<  workerId <<  " " <<  i  << "\n";

               
                     for (int j = 0; j < ne; j++){
                        std::cout << "array  dispatcher " <<  responseIdx[j]  << "\n";
                     }
                     std::cout << " " << std::endl;
                     expected = 0;

                  }
                  finalize(responseIdx[i]);
            
               }else{
                  std::cout << "Has not been transfered" << std::endl;
               }
               

               
            }
         }
      }
   }
   };
