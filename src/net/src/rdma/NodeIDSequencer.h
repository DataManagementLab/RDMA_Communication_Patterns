#pragma once

#include "../message/ProtoMessageFactory.h"
#include "../message/MessageErrors.h"
#include "../proto/ProtoServer.h"
#include "../utils/Config.h"

namespace rdma {

namespace NodeType{
enum Enum : int
{ 
    SERVER,
    CLIENT
};
};

class NodeIDSequencer : public ProtoServer
{
private:
    struct NodeEntry_t
    {
        std::string IP;
        std::string name;
        NodeID nodeID;
        NodeType::Enum nodeType;
    };

protected:
    std::vector<NodeEntry_t> m_entries; //Indexed with nodeID
    std::unordered_map<std::string, NodeID> m_ipPortToNodeIDMapping;
    NodeID m_nextNodeID = 0;
    void handle(Any *anyReq, Any *anyResp) override;
    NodeID getNextNodeID();
public:
    NodeIDSequencer(/* args */);
    ~NodeIDSequencer();

};

}