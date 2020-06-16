


#include <thread>
#include <vector>

class WorkerGroup;

class Barrier {
 private:
   const std::size_t threadCount;
   alignas(64) std::atomic<std::size_t> cntr;
   alignas(64) std::atomic<uint8_t> round;

 public:
   explicit Barrier(std::size_t threadCount)
       : threadCount(threadCount), cntr(threadCount), round(0) {}

   template <typename F> bool wait(F finalizer) {
      auto prevRound = round.load(); // Must happen before fetch_sub
      auto prev = cntr.fetch_sub(1);
      if (prev == 1) {
         // last thread arrived
         cntr = threadCount;
         auto r = finalizer();
         round++;
         return r;
      } else {
         while (round == prevRound) {
            // wait until barrier is ready for re-use
            asm("pause");
            asm("pause");
            asm("pause");
         }
         return false;
      }
   }
   inline bool wait() {
      return wait([]() { return true; });
   }
};



class Worker
{
  protected:
   
   std::thread* t_ = nullptr;
   WorkerGroup* workerGroup;
   bool isJoined = false;
   int workerId = 0;  

  public:
   
   template <class... Args>
   void start(Args&&... args){
      t_ = new std::thread(args..., (workerId));
   }

   Worker(int id):workerId(id){}
   
   Worker(){
   };
   
   ~Worker(){
      
      delete t_;
   };

   void join(){
      if(t_ && t_->joinable()){
         t_->join();
      }
      isJoined = true;
   };
};



class WorkerGroup
{
   private:
   //barrier
   std::size_t size_ = std::thread::hardware_concurrency();
   std::vector<Worker*> workers;
  
   public:
   WorkerGroup(){};
   WorkerGroup(size_t numberThreads) : size_(numberThreads){};

   // template<typename Func>
   template< class... Args >
   inline void run(Args&&... args){
      workers.resize(size_);
      for (size_t i = 0; i < size_; ++i)
      {
         workers[i] = new Worker(i);
         workers[i]->start(std::forward<Args>(args)...);
      }
   };

   inline void wait(){
      for (size_t i = 0; i < size_; ++i)
      {
         workers[i]->join();
      }
   }

   size_t size(){return size_;};
   virtual ~WorkerGroup(){ for(auto* w : workers) delete w;};
};



// class TaskGroup
// {
//    private:
//    //barrier
//    std::size_t size_ = std::thread::hardware_concurrency();
//    std::vector<Worker*> workers;
  
//    public:
//    WorkerGroup(){};
//    WorkerGroup(size_t numberThreads) : size_(numberThreads){};

//    // template<typename Func>
//    template< class... Args >
//    inline void run(Args&&... args){
//       workers.resize(size_);
//       for (size_t i = 0; i < size_; ++i)
//       {
//          workers[i] = new Worker(i);
//          workers[i]->start(std::forward<Args>(args)...);
//       }
//    };

//    inline void wait(){
//       for (size_t i = 0; i < size_; ++i)
//       {
//          workers[i]->join();
//       }
//    }

//    size_t size(){return size_;};
//    virtual ~WorkerGroup(){ for(auto* w : workers) delete w;};
// };