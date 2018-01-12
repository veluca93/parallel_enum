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

template <class T, class S, class C>
void clearpq(std::priority_queue<T, S, C>& q) {
  struct HackedQueue : private std::priority_queue<T, S, C> {
    static S& Container(std::priority_queue<T, S, C>& q) {
      return q.*&HackedQueue::c;
    }
  };
  HackedQueue::Container(q).clear();
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
    if (!Complete(root.first, root.second, true)) {
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
  virtual bool IsSeed(node_t v, const std::unordered_set<node_t>* s) {
    return IsGood({v});
  }

  /**
   * Iterates over all the possible new elements that could be added
   * because of a single new element in a solution.
   */
  virtual void CompleteCands(const std::vector<node_t>* ground_set,
                             node_t new_elem, size_t sol_size,
                             const std::function<bool(node_t)>& cb) {
    if (!ground_set) {
      for (node_t i = 0; i < graph_size_; i++) {
        if (!cb(i)) break;
      }
    } else {
      for (auto i : *ground_set) {
        if (!cb(i)) break;
      }
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

  using CandEl = std::pair<int32_t, node_t>;
  using CandSet =
      std::priority_queue<CandEl, std::vector<CandEl>, std::greater<CandEl>>;

  virtual Aux InitAux(const std::vector<node_t>& s) { return {}; }

  virtual void UpdateAux(Aux& aux, const std::vector<node_t>& s, size_t pos) {}

  /**
   * Update candidate list when a new element is added to the solution.
   */
  virtual void UpdateStep(std::vector<node_t>& s, node_t v, int32_t level,
                          size_t sol_size, CandSet& candidates,
                          std::unordered_map<node_t, int32_t>& cand_level,
                          const std::vector<node_t>* ground_set, Aux& aux) {
    CompleteCands(ground_set, v, sol_size, [&](node_t cnd) {
      for (node_t n : s) {
        if (cnd == n) return true;
      }
      if (!CanAdd(s, aux, cnd)) return true;
      cand_level[cnd] = level + 1;
      candidates.emplace(level + 1, cnd);
      return true;
    });
  }

  /**
   * Extracts the next valid cand from candidates
   */
  virtual std::pair<node_t, int32_t> NextCand(const std::vector<node_t>& s,
                                              CandSet& candidates, Aux& aux) {
    while (!candidates.empty()) {
      auto p = candidates.top();
      candidates.pop();
      bool present = false;
      for (node_t n : s) {
        if (p.second == n) {
          present = true;
          break;
        }
      }
      if (present) continue;
      if (!CanAdd(s, aux, p.second)) continue;
      return {p.second, p.first};
    }
    return {graph_size_, -1};
  }

  /**
   * Recomputes the order and the level of the elements in s with another seed.
   */
  virtual void Resort(std::vector<node_t>& s, std::vector<int32_t>& level,
                      node_t seed) {
    std::vector<node_t> sn{seed};
    std::vector<int32_t> ln{0};
    CompleteInside(sn, ln, s, false);
    s = sn;
    level = ln;
  }

  /**
   * Complete function. Returns true if there was a seed change, false otherwise
   */
  virtual bool Complete(std::vector<node_t>& s, std::vector<int32_t>& level,
                        bool stop_on_seed_change = false) {
    if (s.empty()) throw std::runtime_error("??");
    CandSet candidates;
    // Only needed for CanAdd
    Aux aux = InitAux(s);
    std::unordered_map<node_t, int32_t> cand_level;
    for (uint32_t i = 0; i < s.size(); i++) {
      UpdateStep(s, s[i], level[i], i, candidates, cand_level, nullptr, aux);
    }
    bool seed_change = false;
    while (true) {
      node_t n;
      int32_t l;
      std::tie(n, l) = NextCand(s, candidates, aux);
      if (n == graph_size_) break;
      unsigned pos = s.size();
      while (pos > 0 &&
             (l < level[pos - 1] || (l == level[pos - 1] && n < s[pos - 1])))
        pos--;
      s.insert(s.begin() + pos, n);
      level.insert(level.begin() + pos, l);
      if (n < s[0]) {  // Seed change
        if (stop_on_seed_change) {
          return true;
        }
        seed_change = true;
        Resort(s, level, n);
        cand_level.clear();
        clearpq(candidates);
        aux = InitAux(s);
        for (uint32_t i = 0; i < s.size(); i++) {
          UpdateStep(s, s[i], level[i], i, candidates, cand_level, nullptr,
                     aux);
        }
      } else {
        UpdateAux(aux, s, pos);
        UpdateStep(s, n, l, s.size() - 1, candidates, cand_level, nullptr, aux);
      }
    }
    return seed_change;
  }

  /**
   * Runs complete inside a given set.
   */
  virtual void CompleteInside(std::vector<node_t>& s,
                              std::vector<int32_t>& level,
                              const std::vector<node_t>& inside,
                              bool change_seed = true) {
    if (s.empty()) throw std::runtime_error("??");
    CandSet candidates;
    Aux aux = InitAux(s);
    std::unordered_map<node_t, int32_t> cand_level;
    for (uint32_t i = 0; i < s.size(); i++) {
      UpdateStep(s, s[i], level[i], i, candidates, cand_level, &inside, aux);
    }
    while (true) {
      node_t n;
      int32_t l;
      std::tie(n, l) = NextCand(s, candidates, aux);
      if (n == graph_size_) break;
      unsigned pos = s.size();
      while (pos > 0 &&
             (l < level[pos - 1] || (l == level[pos - 1] && n < s[pos - 1])))
        pos--;
      s.insert(s.begin() + pos, n);
      level.insert(level.begin() + pos, l);
      if (n < s[0] && change_seed) {  // Seed change
        Resort(s, level, n);
        cand_level.clear();
        clearpq(candidates);
        aux = InitAux(s);
        for (uint32_t i = 0; i < s.size(); i++) {
          UpdateStep(s, s[i], level[i], i, candidates, cand_level, &inside,
                     aux);
        }
      } else {
        UpdateAux(aux, s, pos);
        UpdateStep(s, n, l, s.size() - 1, candidates, cand_level, &inside, aux);
      }
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

  /**
   * Parent function, returns the parent index.
   */
  virtual node_t Parent(const std::vector<node_t>& s,
                        const std::vector<int32_t>& level,
                        std::vector<node_t>& parent,
                        std::vector<int32_t>& parent_level) {
    for (unsigned parind_pos = s.size() - 1; parind_pos > 0; parind_pos--) {
      parent = s;
      parent_level = level;
      parent.resize(parind_pos);
      parent_level.resize(parind_pos);
      Complete(parent, parent_level);
      if (parent != s) {
        return s[parind_pos];
      }
    }
    parent.clear();
    return graph_size_;
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
    RestrictedCands(s, level, [&](node_t cand) {
      RestrictedProblem(s, cand, [&](const std::vector<node_t>& sol) {
        std::unordered_set<node_t> sol_set(sol.begin(), sol.end());
        for (auto seed : sol) {
          if (!IsSeed(seed, &sol_set)) continue;
          if (cand <= seed) continue;
          std::vector<node_t> core = sol;
          std::vector<int32_t> clvl = level;
          GetPrefix(core, clvl, seed, cand);
          std::vector<node_t> child = core;
          std::vector<int32_t> lvl = clvl;
          // There was a seed change
          if (Complete(child, lvl, true)) continue;
          std::vector<node_t> p;
          std::vector<int32_t> plvl;
          node_t parind = Parent(child, lvl, p, plvl);
          // Not the parent of this child
          if (p != s) continue;
          // Wrong parent index
          if (parind != cand) continue;
          // Finding the solution from a wrong seed.
          node_t correct_seed = child[0];
          for (node_t n : child) {
            if (n < correct_seed) {
              correct_seed = n;
            }
          }
          if (seed != correct_seed) continue;
          if (RestrMultiple()) {
            p.push_back(parind);
            CompleteInside(core, clvl, p);
            // TODO: improve this ?
            std::vector<node_t> sol_copy(sol.begin(), sol.end());
            std::sort(core.begin(), core.end());
            std::sort(sol_copy.begin(), sol_copy.end());
            // Wrong restricted problem solution
            if (core != sol_copy) continue;
          }
          if (!cb(child, lvl)) {
            not_done = false;
            break;
          }
        }
        return not_done;
      });
      return not_done;
    });
    return not_done;
  }
  size_t graph_size_;
};

#endif  // ENUMERABLE_COMMUTABLE_H
