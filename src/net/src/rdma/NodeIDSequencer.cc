#include "NodeIDSequencer.h"

using namespace rdma;

NodeIDSequencer::NodeIDSequencer(/* args */) : ProtoServer("NodeIDSequencer", Config::SEQUENCER_PORT)
{
  if (!ProtoServer::isRunning())
  {
    ProtoServer::startServer();
  }
}

NodeIDSequencer::~NodeIDSequencer()
{
}

NodeID NodeIDSequencer::getNextNodeID()
{
  return m_nextNodeID++; //indexing entries vector relies on nodeIDs being incremented by 1!
}

void NodeIDSequencer::handle(Any *anyReq, Any *anyResp)
{
  if (anyReq->Is<NodeIDRequest>())
  {
    NodeIDResponse connResp;
    NodeIDRequest connReq;
    anyReq->UnpackTo(&connReq);
    std::string IP = connReq.ip();
    std::string name = connReq.name();
    NodeType::Enum nodeType = (NodeType::Enum)connReq.node_type_enum();
    NodeID newNodeID = getNextNodeID();

    NodeEntry_t entry{IP, name, newNodeID, nodeType};
    m_entries.emplace_back(entry);

    if (nodeType == NodeType::Enum::SERVER)
    {
      m_ipPortToNodeIDMapping[IP] = newNodeID;
    }

    std::cout << "newNodeID: " << newNodeID << " type: " << nodeType << " IP: " << IP << " name: " << name << std::endl;

    connResp.set_nodeid(newNodeID);
    connResp.set_return_(MessageErrors::NO_ERROR);

    anyResp->PackFrom(connResp);
  }
  else if (anyReq->Is<GetAllNodeIDsRequest>())
  {
    GetAllNodeIDsResponse connResp;
    GetAllNodeIDsRequest connReq;
    anyReq->UnpackTo(&connReq);

    for (auto &entry : m_entries)
    {
      auto nodeidEntry = connResp.add_nodeid_entries();
      nodeidEntry->set_name(entry.name);
      nodeidEntry->set_ip(entry.IP);
      nodeidEntry->set_node_id(entry.nodeID);
      nodeidEntry->set_node_type_enum(entry.nodeType);
    }
    connResp.set_return_(MessageErrors::NO_ERROR);

    anyResp->PackFrom(connResp);
  }
  else if (anyReq->Is<GetNodeIDForIpPortRequest>())
  {
    GetNodeIDForIpPortResponse connResp;
    GetNodeIDForIpPortRequest connReq;
    anyReq->UnpackTo(&connReq);

    std::string ipPort = connReq.ipport();
    // std::cout << "NodeID request - IP: " << ipPort << std::endl;

    if (m_ipPortToNodeIDMapping.find(ipPort) != m_ipPortToNodeIDMapping.end())
    {
      NodeID nodeId = m_ipPortToNodeIDMapping[ipPort];
      auto entry = m_entries[nodeId];
      connResp.set_ip(entry.IP);
      connResp.set_name(entry.name);
      connResp.set_node_id(entry.nodeID);
      connResp.set_node_type_enum(entry.nodeType);
      connResp.set_return_(MessageErrors::NO_ERROR);
    }
    else
    {
      Logging::info("NodeIDSequencer handling message GetNodeIDForIpPortRequest: could not find nodeid for IP: " + ipPort);
      connResp.set_return_(MessageErrors::NODEID_NOT_FOUND);
    }
    anyResp->PackFrom(connResp);
  }
  else
  {
    Logging::error(__FILE__, __LINE__, "NodeIDSequencer got unknown message!");
  }
}
