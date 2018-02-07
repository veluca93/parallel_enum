#ifndef ENUMERATOR_PARALLEL_PTHREADS_STEAL_H
#define ENUMERATOR_PARALLEL_PTHREADS_STEAL_H

#include <pthread.h>
#include <iostream>
#include <vector>

#include "enumerator/enumerator.hpp"
#include "util/concurrentqueue.hpp"
#include "util/serialize.hpp"

#undef DEBUG
#ifdef DEBUG_PARALLEL_PTHREADS
#define DEBUG(x)                                          \
  do {                                                    \
    std::cerr << "[ParallelPthreads] " << x << std::endl; \
  } while (0)
#else
#define DEBUG(x)
#endif

static void pin(std::thread& t, int i) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(i, &cpuset);
  int rc =
      pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpuset);
  if (rc != 0) {
    throw std::runtime_error("Error calling pthread_setaffinity_np: " + rc);
  }
}

typedef enum{
  MORE_WORK_RANGE = 0,  // Assigned roots
  MORE_WORK_ROOTS,      // Stolen roots
  MORE_WORK_SUBTREE,    // Stolen subtree
  MORE_WORK_NOTHING
}MoreWorkInfo;

typedef struct{
  MoreWorkInfo info;
  std::pair<size_t, size_t> range;
  size_t* subtree;
  size_t subtreeLength;
}MoreWorkData;

using MoreWork = std::function<MoreWorkData()>;
using CheckSteal = std::function<bool()>;
using SendSteal = std::function<void(std::vector<size_t>&, bool areRoots)>;

template <typename Node, typename Item>
class ParallelPthreadsSteal : public Enumerator<Node, Item> {
 private:
  int _nthreads;
  size_t _minRootId;
  size_t _maxRootId;
  MoreWork* _moreWorkCb;
  CheckSteal* _checkStealCb;
  SendSteal* _sendStealCb;
 public:
  /**
   * Roots will be explored in the range [minRootId, maxRootId[
   * @brief ParallelPthreadsSteal
   * @param nthreads Number of threads.
   * @param minRootId Minimum id for roots search.
   * @param maxRootId Maximum id for roots search.
   */
  ParallelPthreadsSteal(int nthreads, size_t minRootId = 0,
                        size_t maxRootId = 0)
      : _nthreads(nthreads),
        _minRootId(minRootId),
        _maxRootId(maxRootId),
        _moreWorkCb(NULL),
        _checkStealCb(NULL),
        _sendStealCb(NULL) {
    std::cout << "Parallel enumerator (Pthreads): running with " << _nthreads
              << " threads." << std::endl;
  }

  void SetCallbacks(MoreWork* moreWork, CheckSteal* checkSteal, SendSteal* sendSteal) { 
    _moreWorkCb = moreWork; 
    _checkStealCb = checkSteal;
    _sendStealCb = sendSteal;
  }

 protected:
  void RunInternal(Enumerable<Node, Item>* system) override {
    moodycamel::ConcurrentQueue<Node> gnodes(_nthreads * 2);  // Global nodes
    moodycamel::ConcurrentQueue<size_t> possibleRoots;        // Global roots
    std::atomic<uint_fast32_t> waiting{0};                    // Waiting threads (on internal stealing)
    std::atomic<uint_fast32_t> gwaiting{0};                   // Waiting threads (on external stealing)
    std::atomic<uint_fast32_t> stolen{0};                     // Stolen log
    std::atomic<uint_fast32_t> qSize{0};                      // Precise size of global queue
    std::atomic<uint_fast32_t> rootsSize{0};                  // Precise size of roots queue
    std::atomic<bool> terminate{false};
  
    if (!_maxRootId) {
      _maxRootId = system->MaxRoots();
    }

    for(size_t i = _minRootId; i < _maxRootId; i++){
      possibleRoots.enqueue(i);
      ++rootsSize;
    }
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, _nthreads);

    // Thread code
    auto worker_thread = [&](int id) {
      system->SetUp();
      std::vector<Node> lnodes;  // Local nodes
      char padding[64] = {};
      (void)padding;

      std::function<bool(const Node&)> solution_cb = [this, &lnodes, &gnodes, &waiting, &stolen, &qSize, &id,
                          system, &solution_cb, &rootsSize, &possibleRoots](const Node& node) {
        bool offloadedNode = false;
        // Stealing management
        if (id == 0 && _checkStealCb && (*_checkStealCb)()) {
          std::vector<size_t> serializedNode;
          std::vector<size_t> rootsToSend;
          while(rootsSize && rootsToSend.size() < rootsSize /* && rootsSize > 10 */){
            size_t tmp;
            if(possibleRoots.try_dequeue(tmp)){
              --rootsSize;
              rootsToSend.push_back(tmp);
            }
          }

          if(!rootsToSend.empty()){
              // Send roots
              Serialize(rootsToSend, &serializedNode);
              (*_sendStealCb)(serializedNode, true);
          } else {
            // Send subtree
//#define STEAL_MULTIPLE_NODES            
#ifdef STEAL_MULTIPLE_NODES
            if (!lnodes.empty()) {
              /*
              std::vector<Node> toSerialize;
              for(size_t i = 0; i < 10 && !lnodes.empty(); i++){
                toSerialize.push_back(lnodes.back());
                lnodes.pop_back();
              }
              Serialize(toSerialize, &serializedNode);
              */
              Serialize(lnodes, &serializedNode);
              (*_sendStealCb)(serializedNode, false);
              lnodes.clear();
            } 
#else
            offloadedNode = true;
            Serialize(node, &serializedNode);
            (*_sendStealCb)(serializedNode, false);
#endif
          }
        }

        if (!offloadedNode) {
          if (qSize < waiting) {
            gnodes.enqueue(node);
            ++qSize;
#ifdef PRINT_STOLEN
            ++stolen;
#endif
          } else {
            if (!system->CanUseRecursion()) {
              lnodes.push_back(node);
            } else {
              system->ListChildren(node, solution_cb);
            }
          }
        }
        Enumerator<Node, Item>::ReportSolution(system, node);
        return true;
      };

      while (!terminate) {
        while (true) {
          Node node;
          bool nodeSet = false;

          if (!lnodes.empty()) {
            // Pick from local nodes if available
            node = std::move(lnodes.back());
            lnodes.pop_back();
            nodeSet = true;
          } else if (rootsSize) {
            size_t tmp;
            if(possibleRoots.try_dequeue(tmp)){
              --rootsSize;
              system->GetRoot(tmp, solution_cb);
              if(!lnodes.empty()){
                node = std::move(lnodes.back());
                lnodes.pop_back();
                nodeSet = true;
              }
            }
          } else {
            // Otherwise pick from global nodes
            nodeSet = gnodes.try_dequeue(node);
            if (!nodeSet) {
              // If no nodes on the global queue, try to steal.
              ++waiting;
              do {
                // std::this_thread::yield();
                nodeSet = gnodes.try_dequeue(node);
              } while (!nodeSet && (waiting + gwaiting) < (uint_fast32_t)_nthreads);

              if (nodeSet) {
                --waiting;
                --qSize;
              }
            } else {
              --qSize;
            }
          }


          if (nodeSet) {
            system->ListChildren(node, solution_cb);
          } else if (!rootsSize) {
            break;  // No more local roots, try to grab from other computing
                    // nodes (distributed)
          }
        }

        // Try to get more work.
        if (_moreWorkCb) {
          ++gwaiting;
          if (id == 0) {
            DEBUG("Invoking callback.");
            MoreWorkData mwd = (*_moreWorkCb)();
            pthread_barrier_wait(&barrier);
            waiting = 0;
            gwaiting = 0;
            qSize = 0;
            if(mwd.info == MORE_WORK_RANGE){
              DEBUG("Callback invoked.");
              for(size_t i = mwd.range.first; i < mwd.range.second; i++){
                possibleRoots.enqueue(i);
                ++rootsSize;
              }
              DEBUG("Waiting on barrier.");
            } else if(mwd.info == MORE_WORK_ROOTS) {
              const size_t* subtree = mwd.subtree;

              std::vector<size_t> additionalRoots;
              Deserialize(&subtree, &additionalRoots);
              for(auto tmp : additionalRoots){
                possibleRoots.enqueue(tmp);
                ++rootsSize;
              }
              free(mwd.subtree);              
            } else if(mwd.info == MORE_WORK_SUBTREE) {
              const size_t* subtree = mwd.subtree;

#ifdef STEAL_MULTIPLE_NODES
              std::vector<Node> deserializedNodes;
              Deserialize(&subtree, &deserializedNodes);
              for(auto dd : deserializedNodes){
                gnodes.enqueue(dd);
              }
              qSize += deserializedNodes.size();
#else
              Node deserializedNode;
              Deserialize(&subtree, &deserializedNode);
              gnodes.enqueue(deserializedNode);
              qSize += 1;
#endif

              free(mwd.subtree);
            }else{
              terminate = true;
            }
            pthread_barrier_wait(&barrier);
            DEBUG("Exiting barrier.");
          } else {
            // Two barriers are needed: the first one to be sure
            // everyone is out of the first loop, the second one to be
            // sure that the new range has been set.
            pthread_barrier_wait(&barrier);
            // Wait for zero to do its stuff
            pthread_barrier_wait(&barrier);
          }
        } else {
          terminate = true;
        }
      }
    };
    // Start and wait threads
    std::vector<std::thread> threads;
    for (int i = 0; i < _nthreads; i++) {
      threads.emplace_back(std::bind(worker_thread, i));
#ifndef PARALLEL_NOPIN
      pin(threads.back(), i);
#endif
    }
    for (int i = 0; i < _nthreads; i++) threads[i].join();
    pthread_barrier_destroy(&barrier);

#ifdef PRINT_STOLEN
    std::cout << "Stolen " << stolen << std::endl;
#endif
    system->CleanUp();
  }
};
#endif  // ENUMERATOR_PARALLEL_PTHREADS_STEAL_H
