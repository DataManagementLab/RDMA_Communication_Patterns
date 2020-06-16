#pragma once

#include <algorithm>

#include "RDMAServer.h"
#include "Messages.h"
#include "Defs.h"

using sharedRQId = size_t;

// Selective Signaling
constexpr size_t WS_SERVER  = 2048;			// Outstanding responses by a server
constexpr size_t WS_SERVER_ = 2047;

template<RdmaPoll POLL, RdmaTransfer TRANSFER, size_t PL_SIZE>
class BenchmarkReceiver : public rdma::RDMAServer<rdma::ReliableRDMA>
{

  private:
   
   using ResponseT = Response<PL_SIZE>;
   
   sharedRQId sid_ {0};  
   size_t expectedClients_ {0};
   size_t expectedReceiver_ {0}; // needs to be known in order to estimate node IDs
   ExperimentMessage* reqBuffer_ = nullptr;
   ResponseT* respBuffer_ = nullptr;
   size_t reqBufferLength_ {0};
   std::vector<NodeID> connectedSenderIds_;
   std::atomic<bool> poll_{true};
   
  public:
   BenchmarkReceiver(size_t expectedClients, size_t expectedReceiver, size_t localReceiverId) : rdma::RDMAServer<rdma::ReliableRDMA>("receiver", rdma::Config::RDMA_PORT + localReceiverId)  , expectedClients_(expectedClients), expectedReceiver_(expectedReceiver), reqBufferLength_(expectedClients_ +  expectedReceiver) {
      createSharedReceiveQueue(sid_);
      activateSRQ(sid_);
      startServer();
   }

   void receiveBatchSRQ(MessageBase* buffer, size_t numberReceives){
      size_t msgSize = sizeof(MessageBase) ;
      for (size_t i = 0; i < numberReceives; ++i)
      {
         receiveSRQ(sid_, i , (void*) &buffer[i], msgSize);
      }
   }


   
   size_t poll(){
      if(POLL == RdmaPoll::BUF){
         volatile ExperimentMessage* tempReqBuffer = reqBuffer_;

         while(true){
            for(size_t pos = 0; pos < reqBufferLength_; pos++){
               if(tempReqBuffer[pos].type == MessageType::EMPTY)
                  continue;
               return pos;
            }
         
         }
         
         
      }
      if(POLL == RdmaPoll::SRQ){
         size_t nodeId       {0};
         size_t retMemoryIdx {0};
         pollReceiveSRQ(sid_, nodeId ,retMemoryIdx, poll_);
         return retMemoryIdx;
      }
      return 0; 
   }


   inline void finalize(size_t id){
      if(POLL == RdmaPoll::BUF){
         reqBuffer_[id].type = MessageType::EMPTY;
      }
      if(POLL == RdmaPoll::SRQ){
         receiveSRQ(sid_, id ,(void*) &reqBuffer_[id], sizeof(MessageBase));
      }

        
   }


   inline void transfer(size_t reqId, size_t respId){
      ResponseT* resp =  &respBuffer_[respId];
      auto* req =  ((ExperimentMessage*)&reqBuffer_[reqId]);
      
      if(TRANSFER == RdmaTransfer::WRITE){
         write(req->destinationNodeID, req->RDMAptr, (void*) resp, sizeof(ResponseT), false);
      }
      if(TRANSFER == RdmaTransfer::SEND){
         send(req->destinationNodeID, (void*) resp, sizeof(ResponseT), false);
      }
   }
  
 
   size_t benchmarkPhase(){
      size_t clientsFinished {0};
      size_t handledRequests {0};
      while(true ){

         auto memIdx = poll();
         
         if(reqBuffer_[memIdx].type == MessageType::ExpMsg){

               
            auto* req =  ((ExperimentMessage*)&reqBuffer_[memIdx]);
                       

            ResponseT* resp =  &respBuffer_[req->id];
            resp->responseId = req->id; 
            resp->transferFlag = 1;
            resp->senderNodeId = req->RDMAptr;

            // oreder is important for buffer poll and RQ -  SRQ can be scheduled afterwards but needs testing
            if constexpr (POLL == RdmaPoll::BUF){
                  finalize(memIdx);
               }
            transfer(memIdx, req->id);
            
            if constexpr (POLL == RdmaPoll::SRQ){
                  finalize(memIdx);
               }
            ++handledRequests;
         }else if(reqBuffer_[memIdx].type == MessageType::FinishedExp){
            clientsFinished++;
            if(expectedClients_ == clientsFinished){
               return handledRequests;
            }
            // important otherwise one msg can be counted multiple times
            if constexpr (POLL == RdmaPoll::BUF){
                  finalize(memIdx);
               }
         }
      }
      return 0;
   }
   
   inline void initReqBuffer(){
      if(POLL == RdmaPoll::BUF){
         // placement new
         for (size_t i = 0; i < reqBufferLength_; i++){
            reqBuffer_[i].type = MessageType::EMPTY;
         }
      }else if(POLL == RdmaPoll::SRQ){
         receiveBatchSRQ(reqBuffer_, reqBufferLength_);
      }else if(POLL == RdmaPoll::RQ){
         for(auto& id : connectedSenderIds_){
            receive(id, (void*) &reqBuffer_[id], sizeof(ExperimentMessage));
         }
      }
   }


   bool partition(size_t currentNodeId, size_t maxNodeId, int& from, int& to){
      size_t numberPreparedResponses = ( RESP_BUFFER_BYTES / sizeof(ResponseT));
      from = currentNodeId * (numberPreparedResponses / maxNodeId);
      to = (currentNodeId + 1) * (numberPreparedResponses / maxNodeId) - 1;
      if (currentNodeId == maxNodeId-1)
      {
         to = numberPreparedResponses - 1;
      }
      
      return true;
   }

   
   inline void initPhase(){

      // create init buffer
      auto* initBuffer = static_cast<MessageBase*>(localAlloc(sizeof(InitExperimentMsg) * expectedClients_));
      // // prepare req buffer!
      // reqBuffer_ =  static_cast<ExperimentMessage*>(localAlloc(sizeof(InitExperimentMsg) * ( reqBufferLength_)));
      // prepare req buffer!
      respBuffer_ =  static_cast<ResponseT*>(localAlloc(sizeof(ResponseT) * ( RESP_BUFFER_BYTES / sizeof(ResponseT) )));

      receiveBatchSRQ(initBuffer, expectedClients_);
      connectedSenderIds_.reserve(expectedClients_);
      size_t crntConnections {0};
      
      while(true){

         std::atomic<bool> poll {true};
         size_t nodeId {0};
         size_t memoryIdx {0};
         pollReceiveSRQ(sid_, nodeId ,memoryIdx, poll);
           
         auto* recvMessage = &initBuffer[memoryIdx];
         auto* tempResponseBuffer = localAlloc(sizeof(InitExperimentMsg));
         
         if (((MessageBase*)recvMessage)->type == MessageType::InitExp){

            // handle connection
            crntConnections++;
            connectedSenderIds_.push_back(nodeId);
            if(crntConnections == expectedClients_){
               size_t maxNodeID = *(std::max_element(std::begin(connectedSenderIds_), std::end(connectedSenderIds_)));

               std::cout << "Found max Node ID " << maxNodeID << "\n";
               
               // update reqBufferLenght according to maxNodeID seen
               reqBufferLength_ = maxNodeID + 1;
               // prepare req buffer!
               reqBuffer_ =  static_cast<ExperimentMessage*>(localAlloc(sizeof(ExperimentMessage) * ( reqBufferLength_)));
               // prepare reqBuffer according to receive method
               initReqBuffer();

               // partition req region based on max nodeIDs


               
               for(size_t i = 0; i < connectedSenderIds_.size(); i++){
                  NodeID id = connectedSenderIds_[i];

                  int from, to;
                  if(!partition(id, reqBufferLength_, from,to)){
                     std::cout << "Failed to partition" << "\n";

                  }
                  auto resp = ((InitExperimentMsg*) tempResponseBuffer);
                  resp->type = MessageType::InitExp;
                  resp->msgId = from; // hack todo fix this 
                  resp->id = to;
                  resp->RDMAptr = convertPointerToOffset((void*) &(reqBuffer_[id]));
                  send(id, (void*) resp, sizeof(InitExperimentMsg), true );
               }
               return;
            }
            
            // receiveSRQ(sid_, memoryIdx ,(void*) recvMessage, sizeof(MessageBase));
         }
      }
     
   }
  
   
   ~BenchmarkReceiver(){}
};
