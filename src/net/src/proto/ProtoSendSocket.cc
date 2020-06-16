

#include "ProtoSendSocket.h"
#include "../message/MessageErrors.h"

using namespace rdma;

ProtoSendSocket::ProtoSendSocket(string address, int port) {
  m_address = address;
  m_port = port;
  m_pSocket = NULL;
  m_isConnected = false;
}

ProtoSendSocket::~ProtoSendSocket() {
  if (m_pSocket != nullptr) {
    delete m_pSocket;
    m_pSocket = nullptr;
  }
}

void ProtoSendSocket::connect() {
  m_pSocket = new ProtoSocket(m_address, m_port, ZMQ_REQ);
  if (!m_pSocket->connect()) {
    throw runtime_error("Cannot connect to server");
  }
  m_isConnected = true;
}

void ProtoSendSocket::send(Any* sendMsg, Any* recMsg) {
  if (!m_isConnected) {
    throw runtime_error("Not connected to server");
  }
  if (sendMsg == NULL || !m_pSocket->send(sendMsg)) {
    throw runtime_error("Cannot send message");
  }
  if (recMsg == NULL || !m_pSocket->receive(recMsg)) {
    throw runtime_error("Cannot receive message");
  }
  if (recMsg->Is<ErrorMessage>()) {
    ErrorMessage errMsg;
    recMsg->UnpackTo(&errMsg);
    if (errMsg.return_() != MessageErrors::NO_ERROR) {
      throw runtime_error("Error " + to_string(errMsg.return_()) + " returned from server");
    }
  }
}
