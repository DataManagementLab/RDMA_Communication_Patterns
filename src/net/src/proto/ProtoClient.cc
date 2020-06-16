#include "ProtoClient.h"
#include "../utils/Network.h"
#include "ProtoSendSocket.h"

using namespace rdma;

bool ProtoClient::connectProto(const std::string& connection) {
  if (isConnected(connection)) {
    return true;
  }

  // exchange QP info
  string ipAddr = Network::getAddressOfConnection(connection);
  size_t ipPort = Network::getPortOfConnection(connection);
  ProtoSendSocket* sendSocket = new ProtoSendSocket(ipAddr, ipPort);
  sendSocket->connect();
  m_connections[connection] = sendSocket;
  return true;
}

//------------------------------------------------------------------------------------//

void ProtoClient::exchangeProtoMsg(std::string ipAndPortString, Any* sendMsg,
                                   Any* recMsg) {
  auto* sendSocket = m_connections[ipAndPortString];
  sendSocket->send(sendMsg, recMsg);
}

//------------------------------------------------------------------------------------//
