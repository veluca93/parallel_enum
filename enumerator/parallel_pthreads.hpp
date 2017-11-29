#ifndef ENUMERATOR_PARALLEL_PTHREADS_H
#define ENUMERATOR_PARALLEL_PTHREADS_H

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
class ParallelPthreads : public Enumerator<Node, Item> {
private:
    int _nthreads;
public:
    ParallelPthreads(int nthreads) : _nthreads(nthreads){
        std::cout << "Parallel enumerator (Pthreads): running with " <<
                     _nthreads << " threads."<< std::endl;
    }
protected:
  void RunInternal(Enumerable<Node, Item>* system) override {
    moodycamel::ConcurrentQueue<Node> gnodes; // Global nodes
    std::atomic<size_t> nextRoot{0};

      // Thread code
      auto worker_thread = [&](int id) {
          moodycamel::ProducerToken ptok(gnodes);
          moodycamel::ConsumerToken ctok(gnodes);
          bool rootsAvailable = true;
          system->SetUp();
          char padding[64];

          auto solution_cb = [this, &gnodes, &ptok, &ctok, system](const Node& node) {
            gnodes.enqueue(ptok, node);
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

                if(!gnodes.try_dequeue_from_producer(ptok, node) &&
                   !gnodes.try_dequeue(ctok, node)){
                    terminate = true;
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
#endif // ENUMERATOR_PARALLEL_PTHREADS_H
