/**
 * @file RDMAClient.h
 * @author cbinnig, tziegler
 * @date 2018-08-17
 */

#ifndef RDMAClientSRQ_H_
#define RDMAClientSRQ_H_

#include "../proto/ProtoClient.h"
#include "../utils/Config.h"
#include "BaseRDMA.h"
#include "ReliableRDMA.h"
#include "RDMAClient.h"
#include "UnreliableRDMA.h"
#include "NodeIDSequencer.h"

#include <list>
#include <unordered_map>

namespace rdma {

template <typename RDMA_API_T>
class RDMAClientSRQ : public RDMAClient<RDMA_API_T> {


 public:
  size_t srqId_ {0};

 RDMAClientSRQ() : RDMAClient<RDMA_API_T>::RDMAClient(Config::RDMA_MEMSIZE) {
    RDMA_API_T::createSharedReceiveQueue(srqId_);
  }
  
  
  ~RDMAClientSRQ() {}


  /**
   * @brief Connects to an RDMAServer
   * 
   * @param ipPort Ip : port string
   * @param retServerNodeID nodeId of the server connected to
   * @return true success
   * @return false fail
   */
  bool connect(const string& ipPort, NodeID &retServerNodeID) {

    if (!ProtoClient::isConnected(RDMAClient<RDMA_API_T>::m_sequencerIpPort)) {
      RDMAClient<RDMA_API_T>::m_ownNodeID = RDMAClient<RDMA_API_T>::requestNodeID(RDMAClient<RDMA_API_T>::m_sequencerIpPort, RDMAClient<RDMA_API_T>::m_ownIpPort, RDMAClient<RDMA_API_T>::m_nodeType);
    }

    // check if client is connected to data node
    if (!ProtoClient::isConnected(ipPort)) {
      
      ProtoClient::connectProto(ipPort);

      {
        //Request nodeID from Sequencer
        auto getNodeIdReq = ProtoMessageFactory::createGetNodeIDForIpPortRequest(ipPort);

        Any rcvAny;
        ProtoClient::exchangeProtoMsg(RDMAClient<RDMA_API_T>::m_sequencerIpPort, &getNodeIdReq, &rcvAny);

        if (rcvAny.Is<GetNodeIDForIpPortResponse>()) {

          GetNodeIDForIpPortResponse connResponse;
          rcvAny.UnpackTo(&connResponse);

          size_t retries = 50;
          size_t i = 0;
          while (i < retries && connResponse.return_() != MessageErrors::NO_ERROR)
          {
            ProtoClient::exchangeProtoMsg(RDMAClient<RDMA_API_T>::m_sequencerIpPort, &getNodeIdReq, &rcvAny);
            rcvAny.UnpackTo(&connResponse);
            Logging::debug(__FILE__, __LINE__, "GetNodeIDForIpPortResponse returned an error: " + to_string(connResponse.return_()) + " retry " + to_string(i) + "/" + to_string(retries));
            usleep(Config::RDMA_SLEEP_INTERVAL * i);
            ++i;
          }

          if (connResponse.return_() != MessageErrors::NO_ERROR)
          {
            Logging::error(__FILE__, __LINE__, RDMAClient<RDMA_API_T>::m_name + " could not fetch node id of server on connect! Address: " + ipPort);
            return false;
          }

          retServerNodeID = connResponse.node_id();

          if (connResponse.ip() != ipPort)
          {
            std::cout << "name: " << RDMAClient<RDMA_API_T>::m_name << " returned nodeid: " << retServerNodeID << std::endl;
            throw runtime_error("Fetched IP (" + connResponse.ip() + ") from Sequencer did not match requested IP ("+ipPort+")");
          }
        }
        else
        {
          throw runtime_error("An Error occurred while fetching NodeID for ip: " + ipPort);
        }
      }

        if (retServerNodeID >= RDMAClient<RDMA_API_T>::m_nodeIDsConnection.size()) {
            RDMAClient<RDMA_API_T>::m_nodeIDsConnection.resize(retServerNodeID + 1);
            RDMAClient<RDMA_API_T>::m_nodeIDsConnection[retServerNodeID] = ipPort;

        } else {
            RDMAClient<RDMA_API_T>::m_nodeIDsConnection[retServerNodeID] = ipPort;
        }
        RDMAClient<RDMA_API_T>::m_connections[ipPort] = retServerNodeID;




        // check if other Server tried to connect
        //only relevant if this is a Server too
        unique_lock<mutex> lck(RDMAClient<RDMA_API_T>::m_connLock);
	if (retServerNodeID >= RDMAClient<RDMA_API_T>::m_NodeIDsQPs.size()) {
	  RDMAClient<RDMA_API_T>::m_NodeIDsQPs.resize(retServerNodeID + 1);
	}

	if(RDMAClient<RDMA_API_T>::m_NodeIDsQPs.at(retServerNodeID) == false){
	  
	  RDMAClient<RDMA_API_T>::m_NodeIDsQPs[retServerNodeID] = true;
	}else{
	  //other Server already called connect exit
	  lck.unlock();
	  return true;
	}
	lck.unlock();
	
	
	/* RDMA_API_T::initQPWithSuppliedID(retServerNodeID); */
	    
	RDMA_API_T::initQPForSRQWithSuppliedID(srqId_, retServerNodeID);
  

      RDMAConnRequest connRequest;
      ib_conn_t localConn = RDMA_API_T::getLocalConnData(retServerNodeID);
      
      connRequest.set_buffer(localConn.buffer);
      connRequest.set_rkey(localConn.rc.rkey);
      connRequest.set_qp_num(localConn.qp_num);
      connRequest.set_lid(localConn.lid);
      for (int i = 0; i < 16; ++i) {
        connRequest.add_gid(localConn.gid[i]);
      }
      connRequest.set_psn(localConn.ud.psn);
      connRequest.set_nodeid(RDMAClient<RDMA_API_T>::m_ownNodeID);

      Any sendAny;
      sendAny.PackFrom(connRequest);
      Any rcvAny;

      ProtoClient::exchangeProtoMsg(ipPort, &sendAny, &rcvAny);


      if (rcvAny.Is<RDMAConnResponse>()) {
          // connect request was successful
        RDMAConnResponse connResponse;
        rcvAny.UnpackTo(&connResponse);

        struct ib_conn_t remoteConn;
        remoteConn.buffer = connResponse.buffer();
        remoteConn.rc.rkey = connResponse.rkey();
        remoteConn.qp_num = connResponse.qp_num();
        remoteConn.lid = connResponse.lid();
        remoteConn.ud.psn = connResponse.psn();
        for (int i = 0; i < 16; ++i) {
          remoteConn.gid[i] = connResponse.gid(i);
        }
        RDMA_API_T::setRemoteConnData(retServerNodeID, remoteConn);
      }
     
      // connect QPs
      RDMA_API_T::connectQP(retServerNodeID);

      Logging::debug(__FILE__, __LINE__, "RDMAClient: connected to server!");


      return true;
    }
    else
    {
      retServerNodeID = RDMAClient<RDMA_API_T>::m_connections[ipPort];
      return true;
    }

  }
  
protected:


  using ProtoClient::connectProto; //Make private
  using ProtoClient::exchangeProtoMsg; //Make private


};
}  // namespace rdma

#endif /* RDMAClientSRQ_H_ */
