/**
 * @file UnreliableRDMA.h
 * @author cbinnig, tziegler
 * @date 2018-08-17
 */

#ifndef UnreliableRDMA_H_
#define UnreliableRDMA_H_

#include "../utils/Config.h"

#include "BaseRDMA.h"

#include <arpa/inet.h>
#include <rdma/rdma_verbs.h>

namespace rdma {

struct rdma_mcast_conn_t {
  char *mcast_addr;
  struct sockaddr mcast_sockaddr;
  struct rdma_cm_id *id;
  struct rdma_event_channel *channel;
  struct ibv_cq *scq;
  struct ibv_cq *rcq;
  struct ibv_ah *ah;
  struct ibv_pd *pd;
  struct ibv_mr *mr;
  uint32_t remote_qpn;
  uint32_t remote_qkey;
  pthread_t cm_thread;
  bool active;
};

class UnreliableRDMA : public BaseRDMA {
 public:
  UnreliableRDMA(size_t mem_size = Config::RDMA_MEMSIZE);
  ~UnreliableRDMA();

  void initQPWithSuppliedID(const rdmaConnID suppliedID) override;
  void initQPWithSuppliedID( ib_qp_t **, ib_conn_t **) override;
  void initQP(rdmaConnID &retRdmaConnID) override;
  void connectQP(const rdmaConnID rdmaConnID) override;

  void send(const rdmaConnID rdmaConnID, const void *memAddr, size_t size,
            bool signaled) override;
  void receive(const rdmaConnID rdmaConnID, const void *memAddr,
               size_t size) override;
  int pollReceive(const rdmaConnID rdmaConnID,  bool doPoll = true,uint32_t* = nullptr) override;
  void pollSend(const rdmaConnID rdmaConnID, bool doPoll) override;

  void *localAlloc(const size_t &size) override;
  void localFree(const void *ptr) override;
  void localFree(const size_t &offset) override;

  void joinMCastGroup(string mCastAddress, rdmaConnID &retRdmaConnID);
  void leaveMCastGroup(const rdmaConnID rdmaConnID);
  void sendMCast(const rdmaConnID rdmaConnID, const void *memAddr, size_t size,
                 bool signaled);
  void receiveMCast(const rdmaConnID rdmaConnID, const void *memAddr,
                    size_t size);
  int pollReceiveMCast(const rdmaConnID rdmaConnID, bool doPoll);

 private:
  void createQP(struct ib_qp_t *qp) override;
  void destroyQPs() override;
  void modifyQPToInit(struct ibv_qp *qp);
  void modifyQPToRTR(struct ibv_qp *qp);
  void modifyQPToRTS(struct ibv_qp *qp, const uint32_t psn);

  inline uint64_t nextMCastConnKey() { return m_lastMCastConnKey++; }

  void setMCastConn(const rdmaConnID rdmaConnID, rdma_mcast_conn_t &conn);

  void getCmEvent(struct rdma_event_channel *channel,
                  enum rdma_cm_event_type type, struct rdma_cm_event **out_ev);
  
  void checkSignaledMCast(bool &signaled, rdmaConnID rdmaConnID);
  
  // only one QP needed for all connections
  ib_qp_t m_udqp;
  ib_conn_t m_udqpConn;
  ib_qp_t m_udqpMgmt;

  // maps mcastConnkey to MCast connections
  size_t m_lastMCastConnKey;
  std::vector<rdma_mcast_conn_t> m_udpMcastConns;
  std::vector<size_t> m_sendMCastCount;

  mutex m_cqCreateLock;

};

}  // namespace rdma

#endif /* UnreliableRDMA_H_ */
