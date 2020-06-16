/**
 * @file ReliableRDMA.h
 * @author cbinnig, tziegler
 * @date 2018-08-17
 */

#ifndef ReliableRDMA_H_
#define ReliableRDMA_H_

#include "../utils/Config.h"
#include <atomic>
#include "BaseRDMA.h"

#define MAX_POSTLIST 128

namespace rdma {

struct sharedrq_t {
  ibv_srq* shared_rq;
  ibv_cq* recv_cq;
};

class ReliableRDMA : public BaseRDMA {
 public:
  ReliableRDMA();
  ReliableRDMA(size_t mem_size);
  ~ReliableRDMA();

  void initQPWithSuppliedID(const rdmaConnID suppliedID) override;
  void initQPWithSuppliedID( struct ib_qp_t** qp ,struct ib_conn_t ** localConn) override ;

  void initQP(rdmaConnID& retRdmaConnID) override;
  void connectQP(const rdmaConnID rdmaConnID) override;

  void write(const rdmaConnID rdmaConnID, size_t offset, const void* memAddr,
             size_t size, bool signaled);

  void writeImm(const rdmaConnID rdmaConnID, size_t offset, const void* memAddr,
             size_t size, uint32_t , bool signaled);
  void read(const rdmaConnID rdmaConnID, size_t offset, const void* memAddr,
            size_t size, bool signaled);
  void requestRead(const rdmaConnID rdmaConnID, size_t offset,
                   const void* memAddr, size_t size);
  void fetchAndAdd(const rdmaConnID rdmaConnID, size_t offset,
                   const void* memAddr, size_t size, bool signaled);
  void fetchAndAdd(const rdmaConnID rdmaConnID, size_t offset,
                   const void* memAddr, size_t value_to_add, size_t size,
                   bool signaled);

  void compareAndSwap(const rdmaConnID rdmaConnID, size_t offset,
                      const void* memAddr, int toCompare, int toSwap,
                      size_t size, bool signaled);

  void send(const rdmaConnID rdmaConnID, const void* memAddr, size_t size,
            bool signaled) override;

  void sendBatch(const rdmaConnID rdmaConnID, const std::vector<void*>& vecMemAddr,
                               std::vector<size_t>& vecSizes, bool signaled);
  void receive(const rdmaConnID rdmaConnID, const void* memAddr,
               size_t size) override;
  int pollReceive(const rdmaConnID rdmaConnID, bool doPoll = true,uint32_t* = nullptr) override;
  void pollReceiveBatch(size_t srq_id, size_t& num_completed, bool& doPoll);
  void pollSend(const rdmaConnID rdmaConnID, bool doPoll) override;

  void* localAlloc(const size_t& size) override;
  void localFree(const void* ptr) override;
  void localFree(const size_t& offset) override;
  
  // Shared Receive Queue
  void initQPForSRQWithSuppliedID(size_t srq_id, const rdmaConnID rdmaConnID);
  void initQPForSRQ(size_t srq_id, rdmaConnID& retRdmaConnID);

  void receiveSRQ(size_t srq_id, const void* memAddr, size_t size);
  void receiveSRQ(size_t srq_id, size_t memoryIndex ,const void* memAddr, size_t size);
  void pollReceiveSRQ(size_t srq_id, rdmaConnID& retrdmaConnID, bool& doPoll);
  void pollReceiveSRQ(size_t srq_id, rdmaConnID& retrdmaConnID, size_t& retMemoryIdx, bool& doPoll);
  int pollReceiveSRQ(size_t srq_id, rdmaConnID& retrdmaConnID, size_t& retMemoryIdx, std::atomic<bool>& doPoll);
  int pollReceiveSRQBatch(size_t srq_id,size_t* nodeIDs,  size_t* retMemoryIdx, std::atomic<bool>& doPoll);
  int pollReceiveSRQ(size_t srq_id, rdmaConnID& retrdmaConnID, std::atomic<bool> & doPoll);
  int pollReceiveSRQ(size_t srq_id, rdmaConnID &retRdmaConnID, uint32_t *imm, atomic<bool> &doPoll);
  void createSharedReceiveQueue(size_t& ret_srq_id);

   
   
 protected:
  // RDMA operations
  inline void __attribute__((always_inline))
  remoteAccess(const rdmaConnID rdmaConnID, size_t offset, const void* memAddr,
               size_t size, bool signaled, bool wait, enum ibv_wr_opcode verb,uint32_t * imm = nullptr) {
    DebugCode(
      if (memAddr < m_res.buffer || (char*)memAddr + size > (char*)m_res.buffer + m_res.mr->length) {
        Logging::error(__FILE__, __LINE__,
                        "Passed memAddr falls out of buffer addr space");
    })

    checkSignaled(signaled, rdmaConnID);

    struct ib_qp_t localQP = m_qps[rdmaConnID];
    struct ib_conn_t remoteConn = m_rconns[rdmaConnID];

    int ne;

    struct ibv_send_wr sr;
    struct ibv_sge sge;
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)memAddr;
    sge.lkey = m_res.mr->lkey;
    sge.length = size;
    memset(&sr, 0, sizeof(sr));
    sr.sg_list = &sge;
    sr.num_sge = 1;
    sr.opcode = verb;
    sr.next = nullptr;
    sr.send_flags = ((signaled) ? IBV_SEND_SIGNALED : 0) | (size < 220 ? IBV_SEND_INLINE: 0);
    // sr.send_flags = (signaled) ? IBV_SEND_SIGNALED : 0;
    

    // calculate remote address using offset in local buffer
    sr.wr.rdma.remote_addr = remoteConn.buffer + offset;
    sr.wr.rdma.rkey = remoteConn.rc.rkey;

    if(imm!= nullptr)
        sr.imm_data = *imm;

    struct ibv_send_wr* bad_wr = nullptr;
    if ((errno = ibv_post_send(localQP.qp, &sr, &bad_wr))) {
      throw runtime_error("RDMA OP not successful! error: " + to_string(errno));
    }

    if (signaled && wait) {
      struct ibv_wc wc;

      do {
        wc.status = IBV_WC_SUCCESS;
        ne = ibv_poll_cq(localQP.send_cq, 1, &wc);
        if (wc.status != IBV_WC_SUCCESS) {
          throw runtime_error("RDMA completion event in CQ with error! " +
                             to_string(wc.status));
        }

#ifdef BACKOFF
        if (ne == 0) {
          __asm__("pause");
        }
#endif
      } while (ne == 0);

      if (ne < 0) {
        throw runtime_error("RDMA polling from CQ failed!");
      }
    }
  }
   
  virtual void destroyQPs() override;
  void createQP(struct ib_qp_t* qp) override;
  void createQP(size_t srq_id, struct ib_qp_t& qp);
  void modifyQPToInit(struct ibv_qp* qp);
  void modifyQPToRTR(struct ibv_qp* qp, uint32_t remote_qpn, uint16_t dlid,
                     uint8_t* dgid);
  void modifyQPToRTS(struct ibv_qp* qp);

  // shared receive queues
  map<size_t, sharedrq_t> m_srqs;
  size_t m_srqCounter = 0;
  map<size_t, vector<rdmaConnID>> m_connectedQPs;


   size_t batches [MAX_POSTLIST];

};

}  // namespace rdma

#endif /* ReliableRDMA_H_ */
