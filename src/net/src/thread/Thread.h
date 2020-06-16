/**
 * @file Thread.h
 * @author cbinnig, tziegler
 * @date 2018-08-17
 */



#ifndef THREAD_HPP_
#define THREAD_HPP_

#include "../utils/Config.h"
#include <thread>
#include <atomic>

namespace rdma {

class Thread {
 public:
  Thread();

  virtual ~Thread();

  void start(int threadid = -1);

  void join();

  void stop();

  bool running();

  bool killed();

  virtual void run() = 0;

  uint128_t time() {
    return m_endTime - m_startTime;
  }

  void startTimer();
  void endTimer();

  void waitForUser() {
    //wait for user input
    cout << "Press Enter to run Benchmark!" << flush << endl;
    char temp;
    cin.get(temp);
  }
 protected:
  static void execute(void* arg, int threadid);

  uint128_t m_startTime;
  uint128_t m_endTime;
  thread* m_thread;

  std::atomic<bool> m_kill {false};

  std::atomic<bool> m_running {false};
};

}  // end namespace rdma

#endif /* THREAD_HPP_ */
