#ifndef ENUMERATOR_PARALLEL_PTHREADS_STEAL_H
#define ENUMERATOR_PARALLEL_PTHREADS_STEAL_H

#include <vector>

#include "enumerator/enumerator.hpp"
#include "util/concurrentqueue.hpp"

#define STEAL

static void pin(std::thread& t, int i){
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(i, &cpuset);
  int rc = pthread_setaffinity_np(t.native_handle(),
                                  sizeof(cpu_set_t), &cpuset);
  if (rc != 0) {
    throw std::runtime_error("Error calling pthread_setaffinity_np: " + rc);
  }
}

template <typename Node, typename Item>
class ParallelPthreadsSteal : public Enumerator<Node, Item> {
private:
    int _nthreads;
public:
    ParallelPthreadsSteal(int nthreads) : _nthreads(nthreads){
        std::cout << "Parallel enumerator (Pthreads): running with " <<
                     _nthreads << " threads."<< std::endl;
    }
protected:
  void RunInternal(Enumerable<Node, Item>* system) override {
    moodycamel::ConcurrentQueue<Node> gnodes(_nthreads*2); // Global nodes
    std::atomic<uint_fast32_t> waiting{0}; // Waiting nodes
    std::atomic<uint_fast32_t> stolen{0}; // Stolen log
    std::atomic<size_t> nextRoot{0};
    std::atomic<uint_fast32_t> qSize{0}; // Precise size of global queue

      // Thread code
      auto worker_thread = [&](int id) {
          bool rootsAvailable = true;
          system->SetUp();
          std::vector<Node> lnodes; // Local nodes
          char padding[64];

          auto solution_cb = [this, &lnodes, &gnodes, &waiting, &stolen, &qSize, system](const Node& node) {
#ifdef STEAL
            //if(waiting){
            if(qSize < waiting){
                ++qSize;
                gnodes.enqueue(node);
#ifdef PRINT_STOLEN
                ++stolen;
#endif
            }else{
                lnodes.push_back(node);
            }
#else
            lnodes.push_back(node);
#endif
            Enumerator<Node, Item>::ReportSolution(system, node);
            return true;
          };

          while (true) {              
              Node node;
              bool terminate = false;
              if(rootsAvailable){
                  size_t tmp = nextRoot++;
                  if(tmp < system->MaxRoots()){
                    system->GetRoot(tmp, solution_cb);
                  }else{
                    rootsAvailable = false;
                  }
              }

              if (!lnodes.empty()) {
                // Pick from local nodes if available
                node = std::move(lnodes.back());
                lnodes.pop_back();
              }else{
#ifdef STEAL
                // Otherwise pick from global nodes
                bool got = gnodes.try_dequeue(node);
                if(!got){
                  // If no nodes on the global queue, try to steal.
                  ++waiting;
                  do{
                    //std::this_thread::yield();
                    got = gnodes.try_dequeue(node);
                  }while(!got && waiting < (uint_fast32_t) _nthreads);

                  if(got){
                    --waiting;
                    --qSize;
                  }else{
                    terminate = true;
                  }
                }else{
                    --qSize;
                }
#else
                terminate = true;
#endif
              }

              if (terminate){break;}
              system->ListChildren(node, solution_cb);
          }
      };
      // Start and wait threads
      std::vector<std::thread> threads;
      for (int i = 0; i < _nthreads; i++){
          threads.emplace_back(std::bind(worker_thread, i));
          pin(threads.back(), i);
      }
      for (int i = 0; i < _nthreads; i++)
          threads[i].join();

#ifdef PRINT_STOLEN
    std::cout << "Stolen " << stolen << std::endl;
#endif
    system->CleanUp();
  }
};
#endif // ENUMERATOR_PARALLEL_PTHREADS_STEAL_H
