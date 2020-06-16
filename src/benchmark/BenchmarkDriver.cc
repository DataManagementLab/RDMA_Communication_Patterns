#include "BenchmarkDriver.h"

DEFINE_string(mode, "peer", "master or peer - master starts node id sequencer");
DEFINE_uint64(localSender, 1, "Number sender threads started on this machine");
DEFINE_uint64(localReceiver, 1, "Number receiver threads started  on this machine");
DEFINE_uint64(totalSender, 1, "Number total sender in experiment");
DEFINE_uint64(totalReceiver, 1, "Number total receiver in experiment");
DEFINE_uint64(requests, 1, "Number requests each sender sends");
DEFINE_string(verbs, "SendSend", " SenderReceiver {Send,Write}");
DEFINE_uint64(payloadSize, 64, "Payload size in bytes ");
DEFINE_string(expName, "Nto1", "Experiment name e.g Nto1 or NtoN");


namespace benchhelper{


void logExperimentResults(size_t requests, size_t runtimeInMicrosec, size_t payloadSize, rdma::RdmaCounter& counter, PerfEvent& pEvent){
   std::string expLogName = "./experimentLog"+ FLAGS_expName + ".csv";
   bool fileExists = true;
   ifstream f(expLogName.c_str());
   if(!f.good()){
      fileExists = false;
   }
      
   ofstream expLog;
   expLog.open(expLogName, std::ios_base::app );

   std::stringstream perfHeader;
   std::stringstream perfData;
   pEvent.printCounter(perfHeader,perfData, "time sec", pEvent.getDuration());
   pEvent.printReport(perfHeader, perfData , requests); 
   // std::cout << perfHeader.str()  << "\n";
   // std::cout << perfData.str()  << "\n";
   
   if(!fileExists){
      expLog << "expname,verbs,pagesize,time,localsender,localreceiver,totalsender,totalreceiver,pageRequests,pageRequestsPerSec,BWPerSec ," << perfHeader.str() <<  "\n";
   }

   // bigger than 1 sec otherwise error
   if(runtimeInMicrosec > 1000000)
      expLog << FLAGS_expName << "," << FLAGS_verbs << "," << payloadSize   << "," << runtimeInMicrosec  << "," << FLAGS_localSender  << "," << FLAGS_localReceiver  << "," << FLAGS_totalSender  << "," << FLAGS_totalReceiver  << "," << requests  << "," << (requests / (runtimeInMicrosec  / 1000000 ))  << "," <<  (counter.getSentBytes()/1024/1024) / (runtimeInMicrosec / 1000000 )  << "," << perfData.str() << "\n";
   
}


void logExperimentResults(size_t requests, size_t runtimeInMicrosec, size_t payloadSize, rdma::RdmaCounter& counter, PerfEvent& pEvent, size_t percentEmptyMailboxes){
   std::string expLogName = "./experimentLog"+ FLAGS_expName + ".csv";
   bool fileExists = true;
   ifstream f(expLogName.c_str());
   if(!f.good()){
      fileExists = false;
   }
      
   ofstream expLog;
   expLog.open(expLogName, std::ios_base::app );

   std::stringstream perfHeader;
   std::stringstream perfData;
   pEvent.printCounter(perfHeader,perfData, "time sec", pEvent.getDuration());
   pEvent.printReport(perfHeader, perfData , requests); 
   // std::cout << perfHeader.str()  << "\n";
   // std::cout << perfData.str()  << "\n";
   
   if(!fileExists){
      expLog << "expname,verbs,pagesize,time,localsender,localreceiver,totalsender,totalreceiver,percEmptyMailbox,pageRequests,pageRequestsPerSec,BWPerSec ," << perfHeader.str() <<  "\n";
   }

   // bigger than 1 sec otherwise error
   if(runtimeInMicrosec > 1000000)
      expLog << FLAGS_expName << "," << FLAGS_verbs << "," << payloadSize   << "," << runtimeInMicrosec  << "," << FLAGS_localSender  << "," << FLAGS_localReceiver  << "," << FLAGS_totalSender  << "," << FLAGS_totalReceiver  << "," <<  percentEmptyMailboxes  << "," <<requests  << "," << (requests / (runtimeInMicrosec  / 1000000 ))  << "," <<  (counter.getSentBytes()/1024/1024) / (runtimeInMicrosec / 1000000 )  << "," << perfData.str() << "\n";
   
}


// Only for one to one measurements

void logExperimentLatencies(size_t*& latencies, size_t payloadSize, PerfEvent& pEvent){
   std::string expLogName = "./experimentLogLatencies"+ FLAGS_expName + ".csv";
   bool fileExists = true;
   ifstream f(expLogName.c_str());
   if(!f.good()){
      fileExists = false;
   }
      
   ofstream expLog;
   expLog.open(expLogName, std::ios_base::app );

   std::stringstream perfHeader;
   std::stringstream perfData;
   pEvent.printCounter(perfHeader,perfData, "time sec", pEvent.getDuration());
   pEvent.printReport(perfHeader, perfData , FLAGS_requests);
   
   
   if(!fileExists){
      expLog << "expname,verbs,pagesize,localsender,localreceiver,totalsender,totalreceiver,avgLatencieNanoseconds,rquestsPerClient," <<  perfHeader.str() <<  "\n";
   }

   size_t avgLatencieInMicroseconds {0};
   size_t sumLatencies {0};


   
   for(size_t i = 0; i < FLAGS_requests * FLAGS_localSender; i++){
      if(latencies[i] == 0){
         std::cout << "[ATTENTION] Latency measurement " << i << " is 0"  << "\n";
      }
      sumLatencies += latencies[i];
   }

   avgLatencieInMicroseconds = sumLatencies / (FLAGS_requests * FLAGS_localSender);
   
   expLog << FLAGS_expName << "," << FLAGS_verbs << "," << payloadSize  << "," << FLAGS_localSender  << "," << FLAGS_localReceiver  << "," << FLAGS_totalSender  << "," << FLAGS_totalReceiver  << "," << avgLatencieInMicroseconds << "," << FLAGS_requests  << "," << perfData.str() <<  "\n";
   
}



void logExperimentLatencies(size_t*& latencies, size_t payloadSize){
   std::string expLogName = "./experimentLogLatencies"+ FLAGS_expName + ".csv";
   bool fileExists = true;
   ifstream f(expLogName.c_str());
   if(!f.good()){
      fileExists = false;
   }
      
   ofstream expLog;
   expLog.open(expLogName, std::ios_base::app );
   
   if(!fileExists){
      expLog << "expname,verbs,pagesize,localsender,localreceiver,totalsender,totalreceiver,avgLatencieNanoseconds,rquestsPerClient" <<  "\n";
   }

   double  avgLatencieInMicroseconds {0};
   double sumLatencies {0};


   
   for(size_t i = 0; i < FLAGS_requests * FLAGS_localSender; i++){
      if(latencies[i] == 0){
         std::cout << "[ATTENTION] Latency measurement " << i << " is 0"  << "\n";
      }
      sumLatencies += latencies[i];
   }

   avgLatencieInMicroseconds = sumLatencies / (FLAGS_requests * FLAGS_localSender);
   
   expLog << FLAGS_expName << "," << FLAGS_verbs << "," << payloadSize  << "," << FLAGS_localSender  << "," << FLAGS_localReceiver  << "," << FLAGS_totalSender  << "," << FLAGS_totalReceiver  << "," << avgLatencieInMicroseconds << "," << FLAGS_requests << "\n";
   
}


}
