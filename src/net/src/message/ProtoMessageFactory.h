

#ifndef ProtoMessageFactory_H_
#define ProtoMessageFactory_H_

#include "../utils/Config.h"

#include "HelloMessage.pb.h"
#include "RDMAConnRequest.pb.h"
#include "RDMAConnResponse.pb.h"
#include "RDMAConnRequestMgmt.pb.h"
#include "RDMAConnResponseMgmt.pb.h"
#include "MemoryResourceRequest.pb.h"
#include "MemoryResourceResponse.pb.h"
#include "GetAllNodeIDsRequest.pb.h"
#include "GetAllNodeIDsResponse.pb.h"
#include "NodeIDRequest.pb.h"
#include "NodeIDResponse.pb.h"
#include "GetNodeIDForIpPortRequest.pb.h"
#include "GetNodeIDForIpPortResponse.pb.h"

#include "ErrorMessage.pb.h"

#include <google/protobuf/any.pb.h>
#include <google/protobuf/message.h>
using google::protobuf::Any;

namespace rdma
{

namespace MemoryResourceType
{
enum Enum : int
{
  MEMORY_RESOURCE_REQUEST,
  MEMORY_RESOURCE_RELEASE,
};
}

class ProtoMessageFactory
{
public:

  static Any createMemoryResourceRequest(size_t size)
  {
    MemoryResourceRequest resReq;
    resReq.set_size(size);
    resReq.set_type(MemoryResourceType::Enum::MEMORY_RESOURCE_REQUEST);
    Any anyMessage;
    anyMessage.PackFrom(resReq);
    return anyMessage;
  }

  static Any createMemoryResourceRequest(size_t size, std::string &name,
                                         bool persistent)
  {
    MemoryResourceRequest resReq;
    resReq.set_size(size);
    resReq.set_type(MemoryResourceType::Enum::MEMORY_RESOURCE_REQUEST);
    resReq.set_name(name);
    resReq.set_persistent(persistent);

    Any anyMessage;
    anyMessage.PackFrom(resReq);
    return anyMessage;
  }

  static Any createMemoryResourceRelease(size_t size, size_t offset)
  {
    MemoryResourceRequest resReq;
    resReq.set_size(size);
    resReq.set_offset(offset);
    resReq.set_type(MemoryResourceType::Enum::MEMORY_RESOURCE_RELEASE);
    Any anyMessage;
    anyMessage.PackFrom(resReq);
    return anyMessage;
  }

  static Any createGetAllNodeIDsRequest(NodeID nodeID)
  {
    GetAllNodeIDsRequest resReq;
    resReq.set_node_id(nodeID);
    Any anyMessage;
    anyMessage.PackFrom(resReq);
    return anyMessage;
  }

  static Any createGetNodeIDForIpPortRequest(std::string ipPort)
  {
    GetNodeIDForIpPortRequest resReq;
    resReq.set_ipport(ipPort);
    Any anyMessage;
    anyMessage.PackFrom(resReq);
    return anyMessage;
  }

  static Any createNodeIDRequest(std::string ip, std::string name, int nodeTypeEnum)
  {
    NodeIDRequest resReq;
    resReq.set_ip(ip);
    resReq.set_name(name);
    resReq.set_node_type_enum(nodeTypeEnum);
    Any anyMessage;
    anyMessage.PackFrom(resReq);
    return anyMessage;
  }
};
// end class
} // end namespace rdma

#endif /* ProtoMessageFactory_H_ */
