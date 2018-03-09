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

struct bar_t {
  unsigned const count;
  std::atomic<unsigned> spaces;
  std::atomic<unsigned> generation;
  bar_t(unsigned count_) : count(count_), spaces(count_), generation(0) {}
  void wait() {
    unsigned const my_generation = generation;
    if (!--spaces) {
      spaces = count;
      ++generation;
    } else {
      while (generation == my_generation)
        ;
    }
  }
};
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

typedef enum {
  MORE_WORK_RANGE = 0,  // Assigned roots
  MORE_WORK_ROOTS,      // Stolen roots
  MORE_WORK_SUBTREE,    // Stolen subtree
  MORE_WORK_NOTHING
} MoreWorkInfo;

typedef struct {
  MoreWorkInfo info;
  std::pair<size_t, size_t> range;
  size_t* subtree;
  size_t subtreeLength;
} MoreWorkData;

using MoreWork = std::function<MoreWorkData(size_t newsolutions)>;
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

  void SetCallbacks(MoreWork* moreWork, CheckSteal* checkSteal,
                    SendSteal* sendSteal) {
    _moreWorkCb = moreWork;
    _checkStealCb = checkSteal;
    _sendStealCb = sendSteal;
  }

 protected:
  void RunInternal(Enumerable<Node, Item>* system) override {
    moodycamel::ConcurrentQueue<Node> gnodes(_nthreads * 2);  // Global nodes
    moodycamel::ConcurrentQueue<size_t> possibleRoots;        // Global roots
    std::atomic<uint_fast32_t> waiting{
        0};  // Waiting threads (on internal stealing)
    std::atomic<uint_fast32_t> stolen{0};  // Stolen log
    std::atomic<bool> terminate{false};
    std::atomic_flag checkingSteal = ATOMIC_FLAG_INIT;

    if (!_maxRootId) {
      _maxRootId = system->MaxRoots();
    }

    for (size_t i = _minRootId; i < _maxRootId; i++) {
      possibleRoots.enqueue(i);
    }
    // pthread_barrier_t barrier;
    // pthread_barrier_init(&barrier, NULL, _nthreads);
    bar_t barrier(_nthreads);

    // Thread code
    auto worker_thread = [&](int id) {
      system->SetUp();
      std::vector<Node> lnodes;  // Local nodes
      char padding[64] = {};
      (void)padding;

      // Returns true if we gave the node passed as parameter, false if we gave
      // something else
      auto ServeSteal = [&](const Node& node, bool useNode = true) {
        // Stealing management
        bool offloadedNode = false;
        if (_checkStealCb) {
          if (!checkingSteal.test_and_set()) {
            if ((*_checkStealCb)()) {
              std::vector<size_t> serializedNode;
              std::vector<size_t> rootsToSend;
              while (possibleRoots.size_approx() &&
                     rootsToSend.size() <
                         possibleRoots.size_approx() /* && rootsSize > 10 */) {
                size_t tmp;
                if (possibleRoots.try_dequeue(tmp)) {
                  rootsToSend.push_back(tmp);
                }
              }

              if (!rootsToSend.empty()) {
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
                } else if (useNode) {
                  std::vector<Node> toSerialize;
                  toSerialize.push_back(node);
                  Serialize(toSerialize, &serializedNode);
                  (*_sendStealCb)(serializedNode, false);
                  offloadedNode = true;
                }
#else
                if (useNode) {
                  std::vector<Node> toSerialize;
                  toSerialize.push_back(node);
                  Serialize(toSerialize, &serializedNode);
                  (*_sendStealCb)(serializedNode, false);
                  offloadedNode = true;
                }
#endif
              }
            }
            checkingSteal.clear();
          }
        }
        return offloadedNode;
      };

      std::function<bool(const Node&)> solution_cb =
          [this, &lnodes, &gnodes, &waiting, &stolen, &id, system, &solution_cb,
           &possibleRoots, &checkingSteal, &ServeSteal](const Node& node) {
            if (!ServeSteal(node)) {
              if (gnodes.size_approx() < waiting) {
                gnodes.enqueue(node);
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

      auto OpenRoot = [&]() {
        size_t tmp;
        if (possibleRoots.try_dequeue(tmp)) {
          DEBUG("Opening root " << tmp);
          system->GetRoot(tmp, solution_cb);
          return true;
        } else {
          return false;
        }
      };

      while (!terminate) {
        bool localData;
        // In this loop we keep processing local data (local to this computing
        // node, stealing from other threads if needed)
        do {
          localData = false;
          if (!lnodes.empty()) {
            // Pick from local nodes if available
            Node node;
            node = std::move(lnodes.back());
            lnodes.pop_back();
            system->ListChildren(node, solution_cb);
            localData = true;
          } else if (possibleRoots.size_approx() && OpenRoot()) {
            localData = true;
          }

          Node dummyNode;
          ServeSteal(dummyNode, false);  // TODO: Really useful?

          // No local work, try to steal from other threads.
          if (!localData) {
            Node node;
            ++waiting;
            do {
              // std::this_thread::yield();
              localData = gnodes.try_dequeue(node);
            } while (!localData && !terminate &&
                     waiting < (uint_fast32_t)_nthreads);
            --waiting;

            if (localData) {
              system->ListChildren(node, solution_cb);
            }
          }
        } while (localData);
        // No more data to process, try to grab from other computing
        // nodes (distributed)

        // Try to get more work.
        if (_moreWorkCb) {
          ++waiting;
          // pthread_barrier_wait(&barrier);
          barrier.wait();
          if (!checkingSteal.test_and_set()) {
            if (lnodes.empty() && !possibleRoots.size_approx() &&
                !gnodes.size_approx() && !terminate) {
              DEBUG("Requiring more data.");
              MoreWorkData mwd =
                  (*_moreWorkCb)(Enumerator<Node, Item>::solutions_found_);
              Enumerator<Node, Item>::solutions_found_ = 0;
              if (mwd.info == MORE_WORK_RANGE) {
                DEBUG("Received a range.");
                for (size_t i = mwd.range.first; i < mwd.range.second; i++) {
                  possibleRoots.enqueue(i);
                }
              } else if (mwd.info == MORE_WORK_ROOTS) {
                DEBUG("Received stolen roots.");
                const size_t* subtree = mwd.subtree;
                std::vector<size_t> additionalRoots;
                Deserialize(&subtree, &additionalRoots);
                for (auto tmp : additionalRoots) {
                  possibleRoots.enqueue(tmp);
                }
                free(mwd.subtree);
              } else if (mwd.info == MORE_WORK_SUBTREE) {
                DEBUG("Received stolen subtree.");
                const size_t* subtree = mwd.subtree;

                std::vector<Node> deserializedNodes;
                Deserialize(&subtree, &deserializedNodes);
                for (auto dd : deserializedNodes) {
                  gnodes.enqueue(dd);
                }

                free(mwd.subtree);
              } else {
                DEBUG("Received termination message.");
                terminate = true;
              }
            }
            checkingSteal.clear();
          }
          --waiting;
          // pthread_barrier_wait(&barrier);
          barrier.wait();
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
    for (int i = 0; i < _nthreads; i++) threads.at(i).join();
      // pthread_barrier_destroy(&barrier);

#ifdef PRINT_STOLEN
    std::cout << "Stolen " << stolen << std::endl;
#endif
    system->CleanUp();
  }
};
#endif  // ENUMERATOR_PARALLEL_PTHREADS_STEAL_H
