/**
 * @file ProtoServer.h
 * @author cbinnig, tziegler
 * @date 2018-08-17
 */

#ifndef NET_PROTOSERVER_H
#define NET_PROTOSERVER_H

#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#include "../message/ProtoMessageFactory.h"
#include "../thread/Thread.h"
#include "../utils/Config.h"
#include "../utils/Network.h"
#include "ProtoSocket.h"
#include "zmq.hpp"

using google::protobuf::Any;

namespace rdma {

class ProtoServer : public Thread {
 public:
  ProtoServer(string name, int port);
  virtual ~ProtoServer();
  virtual bool startServer();
  virtual void stopServer();
  void run();
  bool isRunning();
  virtual void handle(Any* sendMsg, Any* respMsg) = 0;

  int getPort() { return m_port; }

 protected:
  string m_name;
  
 private:
  int m_port;

  std::atomic<bool> m_running {false};
  ProtoSocket* m_pSocket;
  mutex m_handleLock;
};

}  // end namespace rdma

#endif /* NET_PROTOSERVER_H_ */
