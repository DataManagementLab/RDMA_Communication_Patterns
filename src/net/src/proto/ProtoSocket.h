/**
 * @file ProtoSocket.h
 * @author cbinnig, tziegler
 * @date 2018-08-17
 */

#ifndef ZMQ_ProtoSocket_H
#define ZMQ_ProtoSocket_H

#include "../utils/Config.h"

#include <unistd.h>
#include <cstring>
#include <string>

#include "../message/ProtoMessageFactory.h"
#include "../utils/Logging.h"
#include "zmq.hpp"

using google::protobuf::Any;

namespace rdma {

class ProtoSocket {
 public:
  ProtoSocket(string addr, int port, int sockType);

  ~ProtoSocket();

  bool bind();

  bool connect();

  bool isOpen();

  bool sendMore(Any* msg);

  bool send(Any* msg);

  bool receive(Any* msg);

  bool close();

  bool closeContext();

 private:
  zmq::context_t* m_pCtx;

  string m_conn;
  int m_sockType;
  bool m_isOpen;
  zmq::socket_t* m_pSock;
};

}  // end namespace rdma

#endif
