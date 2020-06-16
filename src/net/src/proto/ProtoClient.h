#pragma once

#include "../message/ProtoMessageFactory.h"
#include "../utils/Config.h"
#include "ProtoSendSocket.h"

namespace rdma {

class ProtoClient {
 public:
  ProtoClient() = default;
  ~ProtoClient() {
    for (auto kv : m_connections) {
      delete kv.second;
    }
    m_connections.clear();
  }
  void exchangeProtoMsg(std::string ipAndPortString, Any* sendMsg, Any* recMsg);
  bool connectProto(const string& connection);

  bool isConnected(std::string ipAndPortString) {
    return m_connections.find(ipAndPortString) != m_connections.end();
  }

 protected:
  unordered_map<string, ProtoSendSocket*> m_connections;
};

}  // namespace rdma