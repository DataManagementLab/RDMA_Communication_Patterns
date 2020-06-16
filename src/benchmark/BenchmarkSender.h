#pragma once

#include <algorithm>
#include <thread>
#include <chrono>

#include "RDMAClient.h"
#include "Concurrency.h"
#include "Messages.h"
#include "Defs.h"

// Needed for dispatcher only
struct alignas(64) IncomingSendMailbox{
   
   std::atomic<size_t> mailReceived {0}; // 0 no and flipped to 1 if 
};

template<RdmaPoll P,RdmaTransfer T, size_t PL_SIZE>
class BenchmarkSender : public rdma::RDMAClient<rdma::ReliableRDMA>
{
  protected:

   using ResponseT = Response<PL_SIZE>;

   struct recvInfo {
      NodeID nodeId {0};
      size_t msgOffset {0};
      size_t from{0};
      size_t to{0};

      recvInfo(NodeID nodeId): nodeId(nodeId){};
   };
   
   std::string nodeIdSequencerIp_ = rdma::Config::SEQUENCER_IP + ":" + std::to_string(rdma::Config::SEQUENCER_PORT);
   
   std::vector<recvInfo> recvIds;
   size_t expectedReceiver_ {0};
   size_t requests_ {0};
   
  public:
   BenchmarkSender(size_t expectedReceiver, size_t requests) : expectedReceiver_(expectedReceiver), requests_(requests) {}

   void initPhase(std::vector<std::string>& recvIps, Barrier& barrier){

      // conncet to all recv
      for(auto& ip : recvIps){
         size_t recvId {0};
         if (!connect(ip, recvId)) {
            throw invalid_argument(
               "RPCPerf connection failed");
         }
         recvIds.emplace_back(recvId);
      }

      barrier.wait([](){std::cout << "Connected to all recv " << std::endl; return true;});

      for(auto& ri : recvIds){

         InitExperimentMsg* recvMsg = (InitExperimentMsg*) localAlloc(sizeof(InitExperimentMsg));
      
         InitExperimentMsg* initMsg = (InitExperimentMsg*) localAlloc(sizeof(InitExperimentMsg));
         initMsg->type = MessageType::InitExp;

         receive(ri.nodeId, (void*)(recvMsg), sizeof(InitExperimentMsg));
         send(ri.nodeId, (void*) initMsg, sizeof(InitExperimentMsg), true); 
         pollReceive(ri.nodeId, true);
         ri.msgOffset = recvMsg->RDMAptr;
         ri.from = recvMsg->msgId;
         ri.to = recvMsg->id;
      }
      barrier.wait([](){
            std::cout << "Last worker recv last receive" << std::endl;
            return true;});
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

   
   void benchmarkPhase(size_t*& latencies, size_t startIndex){
      
      ResponseT* responseArea = static_cast<ResponseT*>(localAlloc(sizeof(ResponseT)));
      auto* expMsg = static_cast<ExperimentMessage*>(localAlloc(sizeof(ExperimentMessage)));
      size_t respRDMAOffset = convertPointerToOffset((void*) responseArea);

      size_t respBufferSize = ( RESP_BUFFER_BYTES / sizeof(ResponseT));

      auto startOuter = std::chrono::high_resolution_clock::now();

      unsigned int seed = startIndex * 41 * getOwnNodeID();
      
      for (size_t i = 0; i < requests_ ; ++i)
      {

         auto randNumber = (rand_r(&seed));
         
         expMsg->type = MessageType::ExpMsg;   
         expMsg->msgId = i;
         expMsg->id = randNumber % respBufferSize;
         expMsg->RDMAptr = respRDMAOffset;
         expMsg->destinationNodeID = getOwnNodeID();
         auto pickedReceiverID = recvIds[ randNumber % recvIds.size()];
         expMsg->padding_destNodeID = pickedReceiverID.nodeId;
         expMsg->padding_destRDMA = pickedReceiverID.msgOffset;

         responseArea->transferFlag = 0;
         
         // auto start = std::chrono::high_resolution_clock::now();

         if constexpr(P == RdmaPoll::RQ){
               receive(pickedReceiverID.nodeId, (void*)(responseArea), sizeof(ResponseT));
            }

         auto start = std::chrono::high_resolution_clock::now();         
         if constexpr(T == RdmaTransfer::WRITE){
               write(pickedReceiverID.nodeId, pickedReceiverID.msgOffset  ,(void*) expMsg, sizeof(ExperimentMessage), false);
            }
             
         if constexpr(T == RdmaTransfer::SEND){
               send(pickedReceiverID.nodeId,  (void*) expMsg, sizeof(ExperimentMessage), false);
            }
         // std::cout << "Send Request to " << pickedReceiverID.nodeId  << "\n";

         if constexpr(P == RdmaPoll::RQ){
               pollReceive(pickedReceiverID.nodeId, true);
            }
         if constexpr(P == RdmaPoll::BUF){
               while(responseArea->transferFlag != 1){
               };
               responseArea->transferFlag = 0;
            }

         auto elapsed = std::chrono::high_resolution_clock::now() - start;

     
      
         latencies[startIndex + i] = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();

      }
      auto elapsedOuter = std::chrono::high_resolution_clock::now() - startOuter;
      std::cout << "Loop avg latency took " << std::chrono::duration_cast<std::chrono::microseconds>(elapsedOuter).count() / requests_ << " microseconds" << std::endl;
      
   };


   void benchmarkPhase(size_t*& latencies, size_t startIndex, double percEmptyMailbox){
      
      ResponseT* responseArea = static_cast<ResponseT*>(localAlloc(sizeof(ResponseT)));
      auto* expMsg = static_cast<ExperimentMessage*>(localAlloc(sizeof(ExperimentMessage)));
      size_t respRDMAOffset = convertPointerToOffset((void*) responseArea);

      size_t respBufferSize = ( RESP_BUFFER_BYTES / sizeof(ResponseT));

      auto startOuter = std::chrono::high_resolution_clock::now();

      unsigned int seed = startIndex * 41 * getOwnNodeID();
      size_t numberNoOps {0};
      for (size_t i = 0; i < requests_ ; ++i)
      {

         auto noOp = (rand_r(&seed));
         // Experiment for polling hypothesis
         if(noOp % 100 < percEmptyMailbox){
            
            auto start = std::chrono::high_resolution_clock::now();       
            auto elapsed = std::chrono::high_resolution_clock::now() - start;
            numberNoOps++;
            while(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() < 8){
               elapsed = std::chrono::high_resolution_clock::now() - start;
            }
            
            latencies[startIndex + i] = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
            continue;

         }
         
         auto randNumber = (rand_r(&seed));
         
         expMsg->type = MessageType::ExpMsg;   
         expMsg->msgId = i;
         expMsg->id = randNumber % respBufferSize;
         expMsg->RDMAptr = respRDMAOffset;
         expMsg->destinationNodeID = getOwnNodeID();
         auto pickedReceiverID = recvIds[ randNumber % recvIds.size()];
         expMsg->padding_destNodeID = pickedReceiverID.nodeId;
         expMsg->padding_destRDMA = pickedReceiverID.msgOffset;

         responseArea->transferFlag = 0;
         
         // auto start = std::chrono::high_resolution_clock::now();

         if constexpr(P == RdmaPoll::RQ){
               receive(pickedReceiverID.nodeId, (void*)(responseArea), sizeof(ResponseT));
            }

         auto start = std::chrono::high_resolution_clock::now();         
         if constexpr(T == RdmaTransfer::WRITE){
               write(pickedReceiverID.nodeId, pickedReceiverID.msgOffset  ,(void*) expMsg, sizeof(ExperimentMessage), false);
            }
             
         if constexpr(T == RdmaTransfer::SEND){
               send(pickedReceiverID.nodeId,  (void*) expMsg, sizeof(ExperimentMessage), false);
            }
         // std::cout << "Send Request to " << pickedReceiverID.nodeId  << "\n";

         if constexpr(P == RdmaPoll::RQ){
               pollReceive(pickedReceiverID.nodeId, true);
            }
         if constexpr(P == RdmaPoll::BUF){
               while(responseArea->transferFlag != 1){
               };
               responseArea->transferFlag = 0;
            }

         auto elapsed = std::chrono::high_resolution_clock::now() - start;

     
      
         latencies[startIndex + i] = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();

      }
      auto elapsedOuter = std::chrono::high_resolution_clock::now() - startOuter;
      std::cout << "Loop avg latency took " << std::chrono::duration_cast<std::chrono::microseconds>(elapsedOuter).count() / requests_ << " microseconds" << std::endl;
      std::cout << "Number no ops: " <<numberNoOps << "\n";
      std::cout << "Number requests: " <<requests_  << "\n";
      std::cout << "Perc. Empty Mailboxes: " << percEmptyMailbox  << "\n";

   };


   // overload for shared memory
   void benchmarkPhase(size_t*& latencies, size_t startIndex, ResponseT* sharedResponseArea, size_t offsetForSharedArea, NodeID sharedNodeId){

      (void) sharedResponseArea;
      (void) offsetForSharedArea;
      // ResponseT* responseArea = static_cast<ResponseT*>(localAlloc(sizeof(ResponseT)));
      auto* expMsg = static_cast<ExperimentMessage*>(localAlloc(sizeof(ExperimentMessage)));
      // size_t respRDMAOffset = convertPointerToOffset((void*) responseArea);

      size_t respBufferSize = ( RESP_BUFFER_BYTES / sizeof(ResponseT));
      unsigned int seed = startIndex * 42;
      
      for (size_t i = 0; i < requests_ ; ++i)
      {

         auto randNumber = (rand_r(&seed));
         
         expMsg->type = MessageType::ExpMsg;   
         expMsg->msgId = i;
         expMsg->id = randNumber % respBufferSize;
         expMsg->RDMAptr = offsetForSharedArea;
         expMsg->destinationNodeID = sharedNodeId;
         auto pickedReceiverID = recvIds[ (randNumber) % recvIds.size()];
         expMsg->padding_destNodeID = pickedReceiverID.nodeId;
         expMsg->padding_destRDMA = pickedReceiverID.msgOffset;

         auto start = std::chrono::high_resolution_clock::now();

         // if constexpr(P == RdmaPoll::RQ){
         //       receive(pickedReceiverID.nodeId, (void*)(responseArea), sizeof(ResponseT));
         //    }

         if constexpr(T == RdmaTransfer::WRITE){
               write(pickedReceiverID.nodeId, pickedReceiverID.msgOffset  ,(void*) expMsg, sizeof(ExperimentMessage), false);
            }
             
         if constexpr(T == RdmaTransfer::SEND){
               send(pickedReceiverID.nodeId,  (void*) expMsg, sizeof(ExperimentMessage), false);
            }
         // std::cout << "Send Request to " << pickedReceiverID.nodeId  << "\n";

         // if constexpr(P == RdmaPoll::RQ){
         //       pollReceive(pickedReceiverID.nodeId, true);
         //    }
         if constexpr(P == RdmaPoll::BUF){
               while(sharedResponseArea->transferFlag != 1){
               };
               sharedResponseArea->transferFlag = 0;
            }

         auto elapsed = std::chrono::high_resolution_clock::now() - start;

     
      
         latencies[startIndex + i] = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();

      }
   };


      // overload for dispatcher
   void benchmarkPhase(size_t*& latencies, size_t startIndex, ResponseT* sharedResponseArea, size_t offsetForSharedArea , ExperimentMessage* sharedReqArea, std::atomic<size_t>* mailbox, NodeID sharedNodeId){


      

      (void) sharedResponseArea;
      // ResponseT* responseArea = static_cast<ResponseT*>(localAlloc(sizeof(ResponseT)));
      auto* expMsg = sharedReqArea;
      // size_t respRDMAOffset = convertPointerToOffset((void*) responseArea);

      // size_t respBufferSize = ( RESP_BUFFER_BYTES / sizeof(ResponseT));
      unsigned int seed = startIndex * 42;
      size_t expected = 1 ;
      size_t desired = 0;
      for (size_t i = 0; i < requests_ ; ++i)
      {

         
         auto randNumber = (rand_r(&seed));
         auto pickedReceiverID = recvIds[ randNumber % recvIds.size()];
         

         expMsg->msgId = i;
         expMsg->id = (randNumber % (pickedReceiverID.to-pickedReceiverID.from)) + pickedReceiverID.from ;
         expMsg->RDMAptr = offsetForSharedArea;
         expMsg->destinationNodeID = sharedNodeId;

         expMsg->padding_destNodeID = pickedReceiverID.nodeId;
         expMsg->padding_destRDMA = pickedReceiverID.msgOffset;

         auto start = std::chrono::high_resolution_clock::now();

         
         // signal dispatcher
         expMsg->type = MessageType::ExpMsg;
         
         
         while(! mailbox->compare_exchange_strong(expected, desired))
         {
            expected = 1;
         }
         
         auto elapsed = std::chrono::high_resolution_clock::now() - start;

         latencies[startIndex + i] = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();

      }
   };

   
   
   // depending on method
   
   void finalizePhase(){
      for(auto& ri : recvIds){      
         FinishedExperimentMsg* initMsg = (FinishedExperimentMsg*) localAlloc(sizeof(FinishedExperimentMsg));
         initMsg->type = MessageType::FinishedExp;
         if constexpr(T == RdmaTransfer::WRITE)
         write(ri.nodeId, ri.msgOffset,(void*) initMsg, sizeof(FinishedExperimentMsg), true); 
         if constexpr(T == RdmaTransfer::SEND)
                                 send(ri.nodeId,(void*) initMsg, sizeof(FinishedExperimentMsg), true); 
      }
   }

   
   
   virtual ~BenchmarkSender(){}
};
