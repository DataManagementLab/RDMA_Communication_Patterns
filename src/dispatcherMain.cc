#include <gflags/gflags.h>
#include <iostream>

#include "BenchmarkDriver.h"
#include "BenchmarkDispatcher.h"

// DISPATCHER BENCHMARK 

#define SUPPORTED_PAYLOAD_SIZES 64,128,256,512,1024,2048,4096,8192,16384


template<RdmaPoll ReceiverP,RdmaTransfer ReceiverT, RdmaPoll SenderP,RdmaTransfer SenderT , size_t PL_SIZE>
int benchmarkDriver(){

   
   std::unique_ptr<rdma::NodeIDSequencer> sequencer;
   if(FLAGS_mode.compare( "master") == 0){
      sequencer = std::make_unique<rdma::NodeIDSequencer>();
   }  

   std::vector<benchhelper::ExpStatsReceiver> expStatsRec;
   expStatsRec.resize(FLAGS_localReceiver);

   size_t* senderLatencies = nullptr;
   if(FLAGS_localSender != 0){
      senderLatencies =  new size_t[FLAGS_requests * FLAGS_localSender];
   }
   WorkerGroup rGroup(FLAGS_localReceiver);
   Barrier rBarrier(rGroup.size() + 1 ); // including main thread

   PerfEvent pEvent;
   rdma::RdmaCounter counter;

   rGroup.run([&rBarrier, &expStatsRec, &pEvent, &counter](int workerId){
                 BenchmarkReceiver<ReceiverP, ReceiverT, PL_SIZE> recv(FLAGS_totalSender, FLAGS_totalReceiver ,workerId);
                 rBarrier.wait();
                 recv.initPhase();
                 if(workerId == 0){
                    counter.start();
                    pEvent.startCounters();
                 }
                 auto start = std::chrono::high_resolution_clock::now();
                 auto requests = recv.benchmarkPhase();
                 auto elapsed = std::chrono::high_resolution_clock::now() - start;
                 auto runtime = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
                 if(workerId == 0){
                    counter.stop();
                    pEvent.stopCounters();
                 }
                 expStatsRec[workerId].runtimeInMicroseconds = runtime;
                 expStatsRec[workerId].pageRequests = requests;
              });

   rBarrier.wait();
  

   // create shared storage client and connect to receiver and create bitlist needed for (incoming sends) 

   if(FLAGS_localSender != 0){
   BenchmarkDispatcher<SenderP, SenderT, PL_SIZE> dispatcher(FLAGS_totalReceiver, FLAGS_localSender);
   auto recvIps = dispatcher.queryReceiverInformation(); 

   for(auto& receiverIP : recvIps ){
      size_t nodeId {0};
      dispatcher.connect(receiverIP,nodeId);
   }

   // allocate shared memory for responses both incoming send and write
   Response<PL_SIZE>* sharedResponseArea = dispatcher.createSharedRDMAMemoryResponses();

   ExperimentMessage* sharedReqArea = dispatcher.createSharedRDMAMemoryRequests();
 
   // only used for incoming sends, writes can be written directly to the destination
   std::atomic<size_t>* mailboxes = dispatcher.createMailboxes();

  
   Worker dispatcherThread(0);

   dispatcherThread.start([&dispatcher](int workerId){
                             std::cout << "test" << workerId << "\n";
                             dispatcher.run();
                          });
   
   
   WorkerGroup group(FLAGS_localSender);
   Barrier barrier(group.size());
   
   group.run([&barrier, &senderLatencies , &sharedResponseArea, &sharedReqArea, &mailboxes , &dispatcher](int workerId){
                std::cout << "WorkerId" << workerId << std::endl;
                BenchmarkSender<SenderP, SenderT, PL_SIZE> sender(FLAGS_totalReceiver, FLAGS_requests);
                auto recvIps = sender.queryReceiverInformation();
                sender.initPhase(recvIps,barrier);
                sender.benchmarkPhase(senderLatencies, workerId *FLAGS_requests, &sharedResponseArea[workerId], dispatcher.convertPointerToOffset((void*) &sharedResponseArea[workerId]), &sharedReqArea[workerId] ,&mailboxes[workerId] ,dispatcher.getOwnNodeID());
                barrier.wait([](){std::cout << "Last client finished" << std::endl; return true;});
                sender.finalizePhase();
             });
   
    
   group.wait();
   dispatcher.running = false;
   dispatcherThread.join();
   }
   rGroup.wait();
   
   if(expStatsRec.size() != 0){
      benchhelper::logExperimentResults(expStatsRec[0].pageRequests, expStatsRec[0].runtimeInMicroseconds, PL_SIZE, counter, pEvent);
   }

   if(FLAGS_localSender != 0){
      benchhelper::logExperimentLatencies(senderLatencies, PL_SIZE);
      delete[] senderLatencies;
   }
   
   return 0;

}


template <size_t ...> struct PayLoadSizeList {};

// default ca
template<RdmaPoll ReceiverP,RdmaTransfer ReceiverT, RdmaPoll SenderP,RdmaTransfer SenderT>
int handle_cases(size_t, PayLoadSizeList<>) {
   std::cerr << "Page size not supported! " << std::endl;
   std::cerr << "SUPPORTED_PAYLOAD_SIZES are supported " << std::endl;
   return 1; }

template<RdmaPoll ReceiverP,RdmaTransfer ReceiverT, RdmaPoll SenderP,RdmaTransfer SenderT , size_t PS, size_t ...N>
int handle_cases(size_t i, PayLoadSizeList<PS, N...>)
{
   std::cout << PS << "\n";
   
   if (PS != i) { return handle_cases<ReceiverP, ReceiverT, SenderP, SenderT>(i, PayLoadSizeList<N...>()); }

   std::cout << "Instantiating Experiment with PL SIZE " << PS  << "\n";

   return benchmarkDriver<ReceiverP, ReceiverT, SenderP, SenderT, PS >();
    
}



int main(int argc, char *argv[])
{
   
   gflags::ParseCommandLineFlags(&argc, &argv, true);

   if(FLAGS_verbs.compare( "WriteWrite") == 0){
      return handle_cases<RdmaPoll::BUF, RdmaTransfer::WRITE, RdmaPoll::BUF, RdmaTransfer::WRITE>(FLAGS_payloadSize,PayLoadSizeList<SUPPORTED_PAYLOAD_SIZES> ());
   }
   if(FLAGS_verbs.compare( "SendWrite") == 0){
      return handle_cases<RdmaPoll::SRQ, RdmaTransfer::WRITE, RdmaPoll::BUF, RdmaTransfer::SEND >(FLAGS_payloadSize ,PayLoadSizeList<SUPPORTED_PAYLOAD_SIZES> ());
   }

   if(FLAGS_verbs.compare( "SendSend") == 0){
      return handle_cases<RdmaPoll::SRQ, RdmaTransfer::SEND, RdmaPoll::SRQ, RdmaTransfer::SEND >( FLAGS_payloadSize, PayLoadSizeList<SUPPORTED_PAYLOAD_SIZES> ());
   }
   if(FLAGS_verbs.compare( "WriteSend") == 0){
      return handle_cases<RdmaPoll::BUF, RdmaTransfer::SEND, RdmaPoll::SRQ, RdmaTransfer::WRITE >( FLAGS_payloadSize,PayLoadSizeList<SUPPORTED_PAYLOAD_SIZES> ());
   }

   std::cerr << "Wrong Verb specification " << FLAGS_verbs << std::endl;
   
   return 0;
}
