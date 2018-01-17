#ifndef ENUMERABLE_COMMUTABLE_H
#define ENUMERABLE_COMMUTABLE_H
#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "absl/strings/str_join.h"
#include "enumerable/enumerable.hpp"
#include "util/graph.hpp"

//#define DEBUG_CANDIDATES
//#define DEBUG_COMPLETE
//#define DEBUG_CHILDREN

template <class T, class S, class C>
void clearpq(std::priority_queue<T, S, C>& q) {
  struct HackedQueue : private std::priority_queue<T, S, C> {
    static S& Container(std::priority_queue<T, S, C>& q) {
      return q.*&HackedQueue::c;
    }
  };
  HackedQueue::Container(q).clear();
}

template <class T, class S, class C>
S& pqc(std::priority_queue<T, S, C>& q) {
  struct HackedQueue : private std::priority_queue<T, S, C> {
    static S& Container(std::priority_queue<T, S, C>& q) {
      return q.*&HackedQueue::c;
    }
  };
  return HackedQueue::Container(q);
}

template <typename node_t>
using CommutableItem = std::vector<node_t>;

template <typename node_t>
using CommutableNode = std::pair<CommutableItem<node_t>, std::vector<int32_t>>;

template <typename Graph, typename Aux = int32_t>
class CommutableSystem
    : public Enumerable<CommutableNode<typename Graph::node_t>,
                        CommutableItem<typename Graph::node_t>> {
 public:
  using node_t = typename Graph::node_t;
  using NodeCallback =
      typename Enumerable<CommutableNode<node_t>,
                          CommutableItem<node_t>>::NodeCallback;

  size_t MaxRoots() override { return graph_size_; }

  void GetRoot(size_t i, const NodeCallback& cb) {
    if (!IsSeed(i, nullptr)) return;
    CommutableNode<node_t> root;
    root.first.push_back(i);
    root.second.push_back(0);
    if (Complete(root.first, root.second, nullptr, nullptr, true)) {
      cb(root);
    }
  }

  void ListChildren(const CommutableNode<node_t>& node,
                    const NodeCallback& cb) {
    Children(node.first, node.second,
             [&cb](const std::vector<node_t>& sol,
                   const std::vector<int32_t>& levels) {
               CommutableNode<node_t> node{sol, levels};
               return cb(node);
             });
  }

  CommutableItem<node_t> NodeToItem(const CommutableNode<node_t>& node) {
    return node.first;
  }

 protected:
  CommutableSystem(size_t graph_size) : graph_size_(graph_size) {}
  /**
   * Checks if a given subset is a solution.
   */
  virtual bool IsGood(const std::vector<node_t>& s) = 0;

  /**
   * Solves the restricted problem
   */
  virtual void RestrictedProblem(
      const std::vector<node_t>& s, node_t v,
      const std::function<bool(std::vector<node_t>)>& cb) = 0;

  /**
   * Checks if we can add a given element to a solution
   */
  virtual bool CanAdd(const std::vector<node_t>& s, Aux& aux, node_t v) {
    auto cnd = s;
    cnd.push_back(v);
    return IsGood(cnd);
  }

  /**
   * Returns true if the resticted problem may have at least two solutions.
   */
  virtual bool RestrMultiple() { return true; }

  /**
   * Checks if the given element can be a valid seed of a solution,
   * or a root if NULL is specified.
   */
  virtual bool IsSeed(node_t v, const std::vector<node_t>* s) {
    return IsGood({v});
  }

  /**
   * Iterates over all the possible new elements that could be added
   * because of a single new element in a solution.
   */
  virtual bool CompleteCandNum(const std::vector<node_t>* ground_set,
                               node_t new_elem, size_t iterator_num, size_t idx,
                               node_t* out) {
    if (!ground_set) {
      if (idx < graph_size_) {
        *out = idx;
        return true;
      }
      return false;
    } else {
      if (idx < ground_set->size()) {
        *out = (*ground_set)[idx];
        return true;
      }
      return false;
    }
  }

  /**
   * Iterates over all the possible new elements that could be used
   * for the restricted problem
   */
  virtual void RestrictedCands(const std::vector<node_t>& s,
                               const std::vector<int32_t>& level,
                               const std::function<bool(node_t)>& cb) {
    auto ss = s;
    std::sort(ss.begin(), ss.end());
    for (node_t i = 0; i < graph_size_; i++) {
      if (std::binary_search(ss.begin(), ss.end(), i)) continue;
      if (!cb(i)) break;
    }
  }

  virtual Aux InitAux(const std::vector<node_t>& s) { return {}; }

  virtual void UpdateAux(Aux& aux, const std::vector<node_t>& s, size_t pos) {}

  /**
   * Recomputes the order and the level of the elements in s with another seed.
   */
  virtual void Resort(std::vector<node_t>& s, std::vector<int32_t>& level,
                      node_t seed) {
    // TODO: implement this to be faster.
    throw std::runtime_error("Never call this for now");
  }

  class Candidates {
   public:
    Candidates(CommutableSystem* commutable_system, std::vector<node_t>& s,
               Aux& aux, const std::vector<node_t>* ground_set)
        : commutable_system_(commutable_system),
          s_(s),
          aux_(aux),
          ground_set_(ground_set) {
      added_so_far_.clear();
      clearpq(pq_);
      info_.clear();
    }
    bool Next(node_t* n, int32_t* lv) {
#ifdef DEBUG_CANDIDATES
      std::cout << "PQ: ";
      for (auto t : pqc(pq_)) {
        std::cout << "(" << std::get<0>(t) << ", " << std::get<1>(t) << ", "
                  << std::get<2>(t) << ")  ";
      }
      std::cout << std::endl;
#endif
      while (!pq_.empty()) {
        auto p = pq_.top();
        pq_.pop();
        *n = std::get<1>(p);
        *lv = std::get<0>(p);
#ifdef DEBUG_CANDIDATES
        std::cout << "POP: " << *n << "@" << *lv << std::endl;
#endif
        InsertInPQ(std::get<2>(p));
        if (CanReallyAdd(*n)) return true;
      }
      return false;
    }
    void Add(node_t v, int32_t lv) {
      added_so_far_.push_back(v);
      info_.emplace_back(0, v, lv);
      InsertInPQ(info_.size() - 1);
    }

   private:
    bool CanReallyAdd(node_t v) {
      bool present = false;
      for (node_t n : added_so_far_) {
        if (v == n) {
          present = true;
          break;
        }
      }
      if (present) return false;
      for (node_t n : s_) {
        if (v == n) {
          present = true;
          break;
        }
      }
      if (present) return true;
      return commutable_system_->CanAdd(s_, aux_, v);
    }
    void InsertInPQ(size_t iterator_num) {
      node_t cand;
      auto& inf = info_[iterator_num];
      if (commutable_system_->CompleteCandNum(ground_set_, std::get<1>(inf),
                                              iterator_num, std::get<0>(inf)++,
                                              &cand)) {
        pq_.emplace(std::get<2>(inf) + 1, cand, iterator_num);
      }
    }
    CommutableSystem* commutable_system_;
    std::vector<node_t>& s_;
    static thread_local std::vector<node_t> added_so_far_;
    Aux& aux_;
    const std::vector<node_t>* ground_set_;
    // level, node, iterator number
    using CandIter = std::tuple<int32_t, node_t, size_t>;
    using PQT = std::priority_queue<CandIter, std::vector<CandIter>,
                                    std::greater<CandIter>>;

    // iteration idx, owner node, owner lvl
    using info_t = std::tuple<size_t, node_t, int32_t>;
    static thread_local PQT pq_;
    static thread_local std::vector<info_t> info_;
  };

  /**
   * Returns false if it failed for some reason.
   * We must have s \subseteq target \subseteq ground_set.
   */
  virtual bool Complete(std::vector<node_t>& s, std::vector<int32_t>& level,
                        const std::vector<node_t>* ground_set = nullptr,
                        const std::vector<node_t>* target = nullptr,
                        bool fail_on_seed_change = false,
                        std::pair<int32_t, node_t> fail_if_smaller_than = {-1,
                                                                           0}) {
#ifdef DEBUG_COMPLETE
    std::cout << "COMPLETE: " << absl::StrJoin(s, ", ") << std::endl;
    std::cout << "  LEVELS: " << absl::StrJoin(level, ", ") << std::endl;
    if (ground_set) {
      std::cout << "          IN: " << absl::StrJoin(*ground_set, ", ")
                << std::endl;
    }
    if (target) {
      std::cout << "         TGT: " << absl::StrJoin(*target, ", ")
                << std::endl;
    }
    if (fail_on_seed_change) {
      std::cout << "         FAIL_SEED" << std::endl;
    }
    if (fail_if_smaller_than.first != -1) {
      std::cout << "         FAIL_SMALLER " << fail_if_smaller_than.first
                << ", " << fail_if_smaller_than.second << std::endl;
    }
#endif
    std::function<bool(node_t)> is_in_target;
    std::function<void()> cleanup_target;
    if (target) {
      thread_local std::vector<bool> target_bitset(graph_size_);
      for (node_t v : *target) {
        target_bitset[v] = true;
      }
      is_in_target = [](node_t v) { return target_bitset[v]; };
      cleanup_target = [target]() {
        for (node_t v : *target) {
          target_bitset[v] = false;
        }
      };
    } else {
      is_in_target = [](node_t) { return true; };
      cleanup_target = []() {};
    }
    while (true) {
      Aux aux = InitAux(s);
      Candidates candidates(this, s, aux, ground_set);
      bool finished = false;
      candidates.Add(s[0], 0);
      size_t next_in_s = 1;
      while (true) {
        // Add cands_to_add
        node_t next = 0;
        int32_t next_lvl = 0;
        if (!candidates.Next(&next, &next_lvl)) {
          finished = true;
          break;
        }
#ifdef DEBUG_COMPLETE
        std::cout << "CAND: " << next << "@" << next_lvl << std::endl;
#endif
        if (next_in_s >= s.size() || next != s[next_in_s]) {
          if (!is_in_target(next)) {
#ifdef DEBUG_COMPLETE
            std::cout << "NOT IN TARGET" << std::endl << std::endl;
#endif
            cleanup_target();
            return false;
          }
          if (std::pair<int32_t, node_t>{next_lvl, next} <
              fail_if_smaller_than) {
#ifdef DEBUG_COMPLETE
            std::cout << "FAILED_SMALL" << std::endl << std::endl;
#endif
            cleanup_target();
            return false;
          }
          s.push_back(next);
          level.push_back(next_lvl);
          UpdateAux(aux, s, s.size() - 1);
          // seed change
          if (next < s[0]) {
            std::swap(s.front(), s.back());
            if (fail_on_seed_change || fail_if_smaller_than.first != -1) {
#ifdef DEBUG_COMPLETE
              std::cout << "FAILED_SEED" << std::endl << std::endl;
#endif
              cleanup_target();
              return false;
            }
            break;
          }
        } else {
          next_in_s++;
        }
        candidates.Add(next, next_lvl);
      }
      Resort(s, level, s.front());
      if (finished) {
#ifdef DEBUG_COMPLETE
        std::cout << "  DONE: " << absl::StrJoin(s, ", ") << std::endl;
        std::cout << "LEVELS: " << absl::StrJoin(level, ", ") << std::endl
                  << std::endl;
#endif
        cleanup_target();
        return true;
      }
#ifdef DEBUG_COMPLETE
      std::cout << "SEED_CHANGE: " << absl::StrJoin(s, ", ") << std::endl;
      std::cout << "     LEVELS: " << absl::StrJoin(level, ", ") << std::endl;
#endif
    }
  }

  /**
   * Computes the prefix of the solution with a given seed and ending with v
   */
  virtual void GetPrefix(std::vector<node_t>& s, std::vector<int32_t>& level,
                         node_t seed, node_t v) {
    Resort(s, level, seed);
    std::size_t i;
    for (i = 0; i < s.size(); i++)
      if (s[i] == v) break;
    s.resize(i + 1);
    level.resize(i + 1);
  }

  virtual void ValidSeeds(const std::vector<node_t>& sol, node_t cand,
                          const std::function<bool(node_t)>& cb) {
    for (auto seed : sol) {
      if (!IsSeed(seed, &sol)) continue;
      if (cand <= seed) continue;
      if (!cb(seed)) break;
    }
  }

  /**
   * Computes the children of a given solution. Returns true if we stopped
   * generating them because the callback returned false.
   */
  virtual bool Children(
      const std::vector<node_t>& s, const std::vector<int32_t>& level,
      const std::function<bool(const std::vector<node_t>&,
                               const std::vector<int32_t>&)>& cb) {
    bool not_done = true;
#ifdef DEBUG_CHILDREN
    std::cout << "CHILDREN: " << absl::StrJoin(s, ", ") << std::endl;
    std::cout << "  LEVELS: " << absl::StrJoin(level, ", ") << std::endl;
#endif
    RestrictedCands(s, level, [&](node_t cand) {
#ifdef DEBUG_CHILDREN
      std::cout << "RCAND: " << cand << std::endl;
#endif
      RestrictedProblem(s, cand, [&](const std::vector<node_t>& sol) {
#ifdef DEBUG_CHILDREN
        std::cout << "SOL: " << absl::StrJoin(sol, ", ") << std::endl;
#endif
        ValidSeeds(sol, cand, [&](node_t seed) {
#ifdef DEBUG_CHILDREN
          std::cout << "SEED: " << seed << std::endl;
#endif
          thread_local std::vector<node_t> core;
          thread_local std::vector<int32_t> clvl;
          core = sol;
          clvl = level;
          GetPrefix(core, clvl, seed, cand);
          thread_local std::vector<node_t> child;
          thread_local std::vector<int32_t> lvl;
          child = core;
          lvl = clvl;
          // Finding the solution from a wrong seed.
          node_t correct_seed = child[0];
          for (node_t n : child) {
            if (n < correct_seed) {
              correct_seed = n;
            }
          }
          if (seed != correct_seed) return true;
            // There was a seed change
#ifdef DEBUG_CHILDREN
          std::cout << "CORE+RCAND: " << absl::StrJoin(core, ", ") << std::endl;
#endif
          if (!Complete(child, lvl, nullptr, nullptr, true,
                        {lvl.back(), child.back()}))
            return true;
#ifdef DEBUG_CHILDREN
          std::cout << "CHILD: " << absl::StrJoin(child, ", ") << std::endl;
#endif
          // Parent check. NOTE: assumes things to be
          // in the correct order.
          bool starts_with_core = true;
          for (size_t i = 0; i < core.size(); i++) {
            if (core[i] != child[i]) {
              starts_with_core = false;
              break;
            }
          }
          if (!starts_with_core) return true;
          thread_local std::vector<node_t> p;
          thread_local std::vector<int32_t> plvl;
          p = core;
          plvl = clvl;
          p.pop_back();
          plvl.pop_back();
#ifdef DEBUG_CHILDREN
          std::cout << "CORE: " << absl::StrJoin(child, ", ") << std::endl;
#endif
          if (!Complete(p, plvl, nullptr, &s)) return true;
#ifdef DEBUG_CHILDREN
          std::cout << "PARENT: " << absl::StrJoin(p, ", ") << std::endl;
#endif
          if (RestrMultiple()) {
            p.push_back(cand);
            if (!Complete(core, clvl, &p, &sol)) return true;
          }
#ifdef DEBUG_CHILDREN
          std::cout << "OK" << std::endl;
#endif
          if (!cb(child, lvl)) {
            not_done = false;
          }
          return not_done;
        });
        return not_done;
      });
      return not_done;
    });
#ifdef DEBUG_CHILDREN
    std::cout << std::endl;
#endif
    return not_done;
  }
  size_t graph_size_;
};

template <typename Graph, typename Aux>
thread_local std::vector<typename Graph::node_t>
    CommutableSystem<Graph, Aux>::Candidates::added_so_far_;

template <typename Graph, typename Aux>
thread_local typename CommutableSystem<Graph, Aux>::Candidates::PQT
    CommutableSystem<Graph, Aux>::Candidates::pq_;

template <typename Graph, typename Aux>
thread_local std::vector<
    typename CommutableSystem<Graph, Aux>::Candidates::info_t>
    CommutableSystem<Graph, Aux>::Candidates::info_;

#endif  // ENUMERABLE_COMMUTABLE_H
