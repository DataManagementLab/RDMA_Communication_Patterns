

#include "BaseRDMA.h"
#include "../message/ProtoMessageFactory.h"
#include "../utils/Logging.h"
#include "ReliableRDMA.h"
#include "UnreliableRDMA.h"

#ifdef LINUX
#include <numa.h>
#include <numaif.h>
#endif

#define HUGEPAGE
using namespace rdma;

rdma_mem_t BaseRDMA::s_nillmem;

//------------------------------------------------------------------------------------//

BaseRDMA::BaseRDMA(size_t mem_size) {
  m_memSize = mem_size;
  m_ibPort = Config::RDMA_IBPORT;
  m_gidIdx = -1;
  m_rdmaMem.push_back(rdma_mem_t(m_memSize, true, 0));

  createBuffer();
}

//------------------------------------------------------------------------------------//

BaseRDMA::~BaseRDMA() {
  // de-register memory region
  if (m_res.mr != nullptr) {
    ibv_dereg_mr(m_res.mr);
    m_res.mr = nullptr;
  }

  // free memory
  if (m_res.buffer != nullptr) {
#ifdef HUGEPAGE
    munmap(m_res.buffer, m_memSize);
#else
    free(m_res.buffer);
#endif
    m_res.buffer = nullptr;
  }

  // de-allocate protection domain
  if (m_res.pd != nullptr) {
    ibv_dealloc_pd(m_res.pd);
    m_res.pd = nullptr;
  }

  // close device
  if (m_res.ib_ctx != nullptr) {
    ibv_close_device(m_res.ib_ctx);
    m_res.ib_ctx = nullptr;
  }
}

//------------------------------------------------------------------------------------//
void BaseRDMA::createBuffer() {
  // Logging::debug(__FILE__, __LINE__, "Create memory region");

  struct ibv_device **dev_list = nullptr;
  struct ibv_device *ib_dev = nullptr;
  int num_devices = 0;

  // get devices
  if ((dev_list = ibv_get_device_list(&num_devices)) == nullptr) {
    throw runtime_error("Get device list failed!");
  }

  bool found = false;
  //Choose rdma device on the correct numa node
  for (int i = 0; i < num_devices; i++)
  {
    ifstream numa_node_file;
    numa_node_file.open(std::string(dev_list[i]->ibdev_path)+"/device/numa_node");
    size_t numa_node = -1;
    numa_node_file >> numa_node;
    if (numa_node == Config::RDMA_NUMAREGION)
    {
      ib_dev = dev_list[i];
      found = true;
      break;
    }
  }
  
  if (!found)
  {
    ibv_free_device_list(dev_list);
    throw runtime_error("Did not find a device connected to specified numa node (Config::RDMA_NUMAREGION)");
  }

  Config::RDMA_DEVICE_FILE_PATH = ib_dev->ibdev_path;

  ibv_free_device_list(dev_list);

  // open device
  if (!(m_res.ib_ctx = ibv_open_device(ib_dev))) {
    throw runtime_error("Open device failed!");
  }

  // get port properties
  if ((errno = ibv_query_port(m_res.ib_ctx, m_ibPort, &m_res.port_attr)) != 0) {
    throw runtime_error("Query port failed");
  }

// allocate memory
#ifdef HUGEPAGE
  m_res.buffer = malloc_huge(m_memSize);
#else
  m_res.buffer = malloc(m_memSize);
#endif
#ifdef LINUX
   numa_tonode_memory(m_res.buffer, m_memSize, Config::RDMA_NUMAREGION);
#endif
  memset(m_res.buffer, 0, m_memSize);
  if (m_res.buffer == 0) {
    throw runtime_error("Cannot allocate memory! Requested size: " + to_string(m_memSize));
  }

  // create protected domain
  m_res.pd = ibv_alloc_pd(m_res.ib_ctx);
  if (m_res.pd == 0) {
    throw runtime_error("Cannot create protected domain!");
  }

  // register memory
  int mr_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                 IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC;

  m_res.mr = ibv_reg_mr(m_res.pd, m_res.buffer, m_memSize, mr_flags);
  if (m_res.mr == 0) {
    throw runtime_error("Cannot register memory!");
  }
}

//------------------------------------------------------------------------------------//

void BaseRDMA::createCQ(ibv_cq *&send_cq, ibv_cq *&rcv_cq) {
  // send queue
  
   if (!(send_cq = ibv_create_cq(m_res.ib_ctx, Config::RDMA_MAX_WR + 1, nullptr, nullptr, 0))) {
    throw runtime_error("Cannot create send CQ!");
  }

  // receive queue
  if (!(rcv_cq = ibv_create_cq(m_res.ib_ctx, Config::RDMA_MAX_WR + 1, nullptr, nullptr, 0))) {
    throw runtime_error("Cannot create receive CQ!");
  }

  Logging::debug(__FILE__, __LINE__, "Created send and receive CQs!");
}

//------------------------------------------------------------------------------------//

void BaseRDMA::destroyCQ(ibv_cq *&send_cq, ibv_cq *&rcv_cq) {
  auto err = ibv_destroy_cq(send_cq);
  if (err != 0) {
    throw runtime_error("Cannot delete send CQ. errno: " + to_string(err));
  }

  err = ibv_destroy_cq(rcv_cq);
  if (err == EBUSY) {
    Logging::info(
        "Could not destroy receive queue in destroyCQ(): One or more Work "
        "Queues is still associated with the CQ");
  } else if (err != 0) {
    throw runtime_error("Cannot delete receive CQ. errno: " + to_string(err));
  }
}

//------------------------------------------------------------------------------------//

void BaseRDMA::setQP(const rdmaConnID rdmaConnID, ib_qp_t &qp) {
  if (m_qps.size() < rdmaConnID + 1) {
    m_qps.resize(rdmaConnID + 1);
    m_countWR.resize(rdmaConnID + 1);
  }
  m_qps[rdmaConnID] = qp;
  m_qpNum2connID[qp.qp->qp_num] = rdmaConnID;
}

//------------------------------------------------------------------------------------//

void BaseRDMA::setLocalConnData(const rdmaConnID rdmaConnID, ib_conn_t &conn) {
  if (m_lconns.size() < rdmaConnID + 1) {
    m_lconns.resize(rdmaConnID + 1);
  }
  m_lconns[rdmaConnID] = conn;
}

//------------------------------------------------------------------------------------//

void BaseRDMA::internalFree(const size_t &offset) {
  size_t lastOffset = 0;
  rdma_mem_t memResFree = m_usedRdmaMem[offset];
  m_usedRdmaMem.erase(offset);

  // lookup the memory region that was assigned to this pointer
  auto listIter = m_rdmaMem.begin();
  if (listIter != m_rdmaMem.end()) {
    for (; listIter != m_rdmaMem.end(); listIter++) {
      rdma_mem_t &memRes = *(listIter);
      if (lastOffset <= offset && offset < memRes.offset) {
        memResFree.free = true;
        m_rdmaMem.insert(listIter, memResFree);
        listIter--;
        Logging::debug(__FILE__, __LINE__, "Freed reserved local memory");
        // printMem();
        mergeFreeMem(listIter);
        // printMem();

        return;
      }
      lastOffset += memRes.offset;
    }
  } else {
    memResFree.free = true;
    m_rdmaMem.insert(listIter, memResFree);
    Logging::debug(__FILE__, __LINE__, "Freed reserved local memory");
    // printMem();
    return;
  }
  // printMem();
  throw runtime_error("Did not free any internal memory!");
}

//------------------------------------------------------------------------------------//

rdma_mem_t BaseRDMA::internalAlloc(const size_t &size) {
  auto listIter = m_rdmaMem.begin();
  for (; listIter != m_rdmaMem.end(); ++listIter) {
    rdma_mem_t memRes = *listIter;
    if (memRes.free && memRes.size >= size) {
      rdma_mem_t memResUsed(size, false, memRes.offset);
      m_usedRdmaMem[memRes.offset] = memResUsed;

      if (memRes.size > size) {
        rdma_mem_t memResFree(memRes.size - size, true, memRes.offset + size);
        m_rdmaMem.insert(listIter, memResFree);
      }
      m_rdmaMem.erase(listIter);
      // printMem();
      return memResUsed;
    }
  }
  // printMem();
  return rdma_mem_t();  // nullptr
}

//------------------------------------------------------------------------------------//

void BaseRDMA::mergeFreeMem(list<rdma_mem_t>::iterator &iter) {
  size_t freeSpace = (*iter).size;
  size_t offset = (*iter).offset;
  size_t size = (*iter).size;

  // start with the prev
  if (iter != m_rdmaMem.begin()) {
    --iter;
    if (iter->offset + iter->size == offset) {
      // increase mem of prev
      freeSpace += iter->size;
      (*iter).size = freeSpace;

      // delete hand-in el
      iter++;
      iter = m_rdmaMem.erase(iter);
      iter--;
    } else {
      // adjust iter to point to hand-in el
      iter++;
    }
  }
  // now check following
  ++iter;
  if (iter != m_rdmaMem.end()) {
    if (offset + size == iter->offset) {
      freeSpace += iter->size;

      // delete following
      iter = m_rdmaMem.erase(iter);

      // go to previous and extend
      --iter;
      (*iter).size = freeSpace;
    }
  }
  Logging::debug(
      __FILE__, __LINE__,
      "Merged consecutive free RDMA memory regions, total free space: " +
          to_string(freeSpace));
}

//------------------------------------------------------------------------------------//

void BaseRDMA::printBuffer() {
  auto listIter = m_rdmaMem.begin();
  for (; listIter != m_rdmaMem.end(); ++listIter) {
    Logging::debug(__FILE__, __LINE__,
                   "offset=" + to_string((*listIter).offset) + "," +
                       "size=" + to_string((*listIter).size) + "," +
                       "free=" + to_string((*listIter).free));
  }
  Logging::debug(__FILE__, __LINE__, "---------");
}

//------------------------------------------------------------------------------------//

void BaseRDMA::setRemoteConnData(const rdmaConnID rdmaConnID, ib_conn_t &conn) {
  if (m_rconns.size() < rdmaConnID + 1) {
    m_rconns.resize(rdmaConnID + 1);
  }
  m_rconns[rdmaConnID] = conn;
}
