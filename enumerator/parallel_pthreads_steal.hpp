#ifndef ENUMERATOR_PARALLEL_PTHREADS_STEAL_H
#define ENUMERATOR_PARALLEL_PTHREADS_STEAL_H

#include <iostream>
#include <pthread.h>
#include <vector>

#include "enumerator/enumerator.hpp"
#include "util/concurrentqueue.hpp"


#undef DEBUG
#ifdef DEBUG_PARALLEL_PTHREADS
#define DEBUG(x) do { std::cerr << "[ParallelPthreads] " << x << std::endl; } while (0)
#else
#define DEBUG(x)
#endif

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

using NewRange = std::function<std::pair<size_t, size_t>()>;

template <typename Node, typename Item>
class ParallelPthreadsSteal : public Enumerator<Node, Item> {
private:
    int _nthreads;
    size_t _minRootId;
    size_t _maxRootId;
    NewRange* _newRangeCb;
public:
    /**
     * Roots will be explored in the range [minRootId, maxRootId[
     * @brief ParallelPthreadsSteal
     * @param nthreads Number of threads.
     * @param minRootId Minimum id for roots search.
     * @param maxRootId Maximum id for roots search.
     */
    ParallelPthreadsSteal(int nthreads,
                          size_t minRootId = 0,
                          size_t maxRootId = 0):
        _nthreads(nthreads), _minRootId(minRootId), _maxRootId(maxRootId),
        _newRangeCb(NULL){
        std::cout << "Parallel enumerator (Pthreads): running with " <<
                     _nthreads << " threads."<< std::endl;
    }

    void SetMoreRootsCallback(NewRange* moreRoots){
        _newRangeCb = moreRoots;
    }

protected:
  void RunInternal(Enumerable<Node, Item>* system) override {
    moodycamel::ConcurrentQueue<Node> gnodes(_nthreads*2); // Global nodes
    std::atomic<uint_fast32_t> waiting{0}; // Waiting nodes
    std::atomic<uint_fast32_t> stolen{0}; // Stolen log
    std::atomic<uint_fast32_t> qSize{0}; // Precise size of global queue
    std::atomic<bool> terminate{false};
    std::atomic<size_t> nextRoot{_minRootId};
    if(!_maxRootId){
        _maxRootId = system->MaxRoots();
    }
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, _nthreads);

      // Thread code
      auto worker_thread = [&](int id) {
          bool rootsAvailable = true;
          system->SetUp();
          std::vector<Node> lnodes; // Local nodes
          char padding[64];

          auto solution_cb = [this, &lnodes, &gnodes, &waiting, &stolen, &qSize, system](const Node& node) {
            if(qSize < waiting){
                ++qSize;
                gnodes.enqueue(node);
#ifdef PRINT_STOLEN
                ++stolen;
#endif
            }else{
                lnodes.push_back(node);
            }
            Enumerator<Node, Item>::ReportSolution(system, node);
            return true;
          };

          while(!terminate){
              rootsAvailable = true;
              while (true) {
                  Node node;
                  bool nodeSet = false;
                  if(rootsAvailable){
                      size_t tmp = nextRoot++;
                      if(tmp < _maxRootId){
                        system->GetRoot(tmp, solution_cb);
                      }else{
                        rootsAvailable = false;
                      }
                  }

                  if (!lnodes.empty()) {
                    // Pick from local nodes if available
                    node = std::move(lnodes.back());
                    lnodes.pop_back();
                    nodeSet = true;
                  }else{
                    // Otherwise pick from global nodes
                    nodeSet = gnodes.try_dequeue(node);
                    if(!nodeSet){
                      // If no nodes on the global queue, try to steal.
                      ++waiting;
                      do{
                        //std::this_thread::yield();
                        nodeSet = gnodes.try_dequeue(node);
                      }while(!nodeSet && waiting < (uint_fast32_t) _nthreads);

                      if(nodeSet){
                        --waiting;
                        --qSize;
                      }
                    }else{
                        --qSize;
                    }
                  }

                  if(nodeSet){
                    system->ListChildren(node, solution_cb);
                  }else if(nextRoot >= _maxRootId){
                      break; // No more local roots, try to grab from other computing nodes (distributed)
                  }
              }
              if(_newRangeCb){
                  if(id == 0){
                    pthread_barrier_wait(&barrier);
                    DEBUG("Invoking callback.");
                    std::pair<size_t, size_t> range = (*_newRangeCb)();
                    DEBUG("Callback invoked.");
                    nextRoot = range.first;
                    _maxRootId = range.second;
                    waiting = 0;
                    qSize = 0;
                    if(nextRoot >= system->MaxRoots()){
                        DEBUG("Terminating....");
                        terminate = true;
                    }
                    DEBUG("Waiting on barrier.");
                    pthread_barrier_wait(&barrier);
                    DEBUG("Exiting barrier.");
                    // Cleanup
                  }else{
                    // Two barriers are needed: the first one to be sure
                    // everyone is out of the first loop, the second one to be
                    // sure that the new range has been set.
                    pthread_barrier_wait(&barrier);
                    // Wait for zero to do its stuff
                    pthread_barrier_wait(&barrier);
                  }
              }else{
                  terminate = true;
              }
          }
      };
      // Start and wait threads
      std::vector<std::thread> threads;
      for (int i = 0; i < _nthreads; i++){
          threads.emplace_back(std::bind(worker_thread, i));
#ifndef PARALLEL_NOPIN
          pin(threads.back(), i);
#endif
      }
      for (int i = 0; i < _nthreads; i++)
          threads[i].join();
      pthread_barrier_destroy(&barrier);

#ifdef PRINT_STOLEN
    std::cout << "Stolen " << stolen << std::endl;
#endif
    system->CleanUp();
  }
};
#endif // ENUMERATOR_PARALLEL_PTHREADS_STEAL_H
