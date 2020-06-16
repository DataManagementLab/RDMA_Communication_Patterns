#pragma once

#include <gflags/gflags.h>

#include "../net/src/utils/RdmaCounter.h"
#include "../utils/PerfEvent.hpp"
#include "BenchmarkSender.h"
#include "BenchmarkReceiver.h"

DECLARE_string(mode);
DECLARE_uint64(localSender);
DECLARE_uint64(localReceiver);
DECLARE_uint64(totalSender);
DECLARE_uint64(totalReceiver);
DECLARE_uint64(requests);
DECLARE_string(verbs);
DECLARE_uint64(payloadSize);
DECLARE_string(expName);
namespace benchhelper{

struct ExpStatsReceiver{
   size_t runtimeInMicroseconds {0};
   size_t pageRequests {0};
};


void logExperimentLatencies(size_t*& latencies, size_t payloadSize);
void logExperimentLatencies(size_t*& latencies, size_t payloadSize, PerfEvent& pEvent);
void logExperimentResults(size_t requests, size_t runtimeInMicrosec, size_t payloadSize, rdma::RdmaCounter& counter, PerfEvent& pEvent);
void logExperimentResults(size_t requests, size_t runtimeInMicrosec, size_t payloadSize, rdma::RdmaCounter& counter, PerfEvent& pEvent, size_t percentEmptyMailboxes);
// template<RdmaPoll ReceiverP,RdmaTransfer ReceiverT, RdmaPoll SenderP,RdmaTransfer SenderT , size_t PL_SIZE>
// int benchmarkDriver(){

   
//    std::unique_ptr<rdma::NodeIDSequencer> sequencer;
//    if(FLAGS_mode.compare( "master") == 0){
//       sequencer = std::make_unique<rdma::NodeIDSequencer>();
//    }  

//    std::vector<ExpStatsReceiver> expStatsRec;
//    expStatsRec.resize(FLAGS_localReceiver);

//    size_t* senderLatencies = nullptr;
//    if(FLAGS_localSender != 0){
//       senderLatencies =  new size_t[FLAGS_requests * FLAGS_localSender];
//    }
//    WorkerGroup rGroup(FLAGS_localReceiver);
//    Barrier rBarrier(rGroup.size() + 1 ); // including main thread

//    PerfEvent pEvent;
//    rdma::RdmaCounter counter;

//    rGroup.run([&rBarrier, &expStatsRec, &pEvent, &counter](int workerId){
//                  BenchmarkReceiver<ReceiverP, ReceiverT, PL_SIZE> recv(FLAGS_totalSender, FLAGS_totalReceiver ,workerId);
//                  rBarrier.wait();
//                  recv.initPhase();
//                  counter.start();
//                  pEvent.startCounters();
//                  auto start = std::chrono::high_resolution_clock::now();
//                  auto requests = recv.benchmarkPhase();
//                  auto elapsed = std::chrono::high_resolution_clock::now() - start;
//                  auto runtime = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
//                  counter.stop();
//                  pEvent.stopCounters(); 
//                  expStatsRec[workerId].runtimeInMicroseconds = runtime;
//                  expStatsRec[workerId].pageRequests = requests;
//               });

//    rBarrier.wait();
  

//    WorkerGroup group(FLAGS_localSender);
//    Barrier barrier(group.size());
   
//    group.run([&barrier, &senderLatencies ](int workerId){
//                 std::cout << "WorkerId" << workerId << std::endl;
//                 BenchmarkSender<SenderP, SenderT, PL_SIZE> sender(FLAGS_totalReceiver, FLAGS_requests);
//                 auto recvIps = sender.queryReceiverInformation();
//                 sender.initPhase(recvIps,barrier);
//                 sender.benchmarkPhase(senderLatencies, workerId *FLAGS_requests);
//                 barrier.wait([](){std::cout << "Last client finished" << std::endl; return true;});
//                 sender.finalizePhase();
//              });
   
//    rGroup.wait();   
//    group.wait();

//    if(expStatsRec.size() != 0){
//       logExperimentResults(expStatsRec[0].pageRequests, expStatsRec[0].runtimeInMicroseconds, PL_SIZE, counter, pEvent);
//    }

//    if(FLAGS_localSender != 0){
//       logExperimentLatencies(senderLatencies, PL_SIZE);
//    }
   
//    return 0;

// }


// template <size_t ...> struct PayLoadSizeList {};

// // default ca
// template<RdmaPoll ReceiverP,RdmaTransfer ReceiverT, RdmaPoll SenderP,RdmaTransfer SenderT>
// int handle_cases(size_t, PayLoadSizeList<>) {
//    std::cerr << "Page size not supported! " << std::endl;
//    std::cerr << "SUPPORTED_PAYLOAD_SIZES are supported " << std::endl;
//    return 1; }

// template<RdmaPoll ReceiverP,RdmaTransfer ReceiverT, RdmaPoll SenderP,RdmaTransfer SenderT , size_t PS, size_t ...N>
// int handle_cases(size_t i, PayLoadSizeList<PS, N...>)
// {
//    std::cout << PS << "\n";
   
//    if (PS != i) { return handle_cases<ReceiverP, ReceiverT, SenderP, SenderT>(i, PayLoadSizeList<N...>()); }

//    std::cout << "Instantiating Experiment with PL SIZE " << PS  << "\n";

//    return benchmarkDriver<ReceiverP, ReceiverT, SenderP, SenderT, PS >();
    
// }





}
