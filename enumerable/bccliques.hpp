#ifndef ENUMERABLE_BCCLIQUES_H
#define ENUMERABLE_BCCLIQUES_H

#include <cstdint>
#include <iostream>
#include <memory>
#include <queue>
#include <vector>

#include "absl/strings/str_join.h"
#include "enumerable/connected_hereditary.hpp"
#include "enumerable/enumerable.hpp"
#include "permute/permute.hpp"
#include "util/graph.hpp"

#define DEGENERACY

template <typename node_t>
using BCClique = CommutableItem<node_t>;

template <typename node_t>
using BCCliqueEnumerationNode = CommutableNode<node_t>;

template <typename Graph>
class BCCliqueEnumeration : public CHSystem<Graph, int> {
 public:
  using node_t = typename Graph::node_t;
  using NodeCallback = typename Enumerable<BCCliqueEnumerationNode<node_t>,
                                           BCClique<node_t>>::NodeCallback;
  explicit BCCliqueEnumeration(Graph* graph)
      : CHSystem<Graph, int>(graph->size()), graph_(graph->Clone()) {}

  bool IsNeigh(node_t a, node_t b) override {
    return graph_->are_black_neighs(a, b);
  }

  absl::Span<const node_t> Neighs(node_t a) override {
    return absl::Span<const node_t>(graph_->black_neighs(a));
  }

  /**
   * Checks if a given subset is a solution.
   */
  bool IsGood(const std::vector<node_t>& s) override {
    throw std::runtime_error("IsGood should never be called");
  }

  /**
   * Solves the restricted problem
   */
  void RestrictedProblem(
      const std::vector<node_t>& s, node_t v,
      const std::function<bool(std::vector<node_t>)>& cb) override {
    cuckoo_hash_set<node_t> ok;
    ok.insert(v);
    for (auto n : s)
      if (graph_->are_neighs(v, n)) ok.insert(n);
    std::vector<node_t> sol;
    cuckoo_hash_set<node_t> visited;
    std::queue<node_t> q;
    q.push(v);
    while (!q.empty()) {
      auto t = q.front();
      q.pop();
      if (!ok.count(t)) continue;
      if (visited.count(t)) continue;
      visited.insert(t);
      sol.push_back(t);
      for (node_t n : graph_->black_neighs(t)) {
        q.push(n);
      };
    }
    cb(sol);
  }

  bool CompleteCandNum(const std::vector<node_t>* ground_set, node_t new_elem,
                       size_t iterator_num, size_t idx, node_t* out) override {
    if (!ground_set) {
      const auto& ngh = graph_->black_neighs(new_elem);
      if (idx < ngh.size()) {
        *out = ngh[idx];
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

  void RestrictedCands(const std::vector<node_t>& s,
                       const std::vector<int32_t>& level,
                       const std::function<bool(node_t)>& cb) override {
    thread_local std::vector<bool> cands_bitset(graph_->size());
    thread_local std::vector<node_t> cands;
    cands.clear();
    for (node_t n : s) {
      cands_bitset[n] = true;
      cands.push_back(n);
    }
    for (node_t n : s) {
      for (node_t c : graph_->black_neighs(n)) {
        if (!cands_bitset[c]) {
          cands.push_back(c);
          cands_bitset[c] = true;
          if (!cb(c)) return;
        }
      }
    }
    for (node_t cand : cands) {
      cands_bitset[cand] = false;
    }
  }

  bool CanAdd(const std::vector<node_t>& s, int& aux, node_t v) override {
    uint32_t black_cnt = 0;
    uint32_t neigh_cnt = 0;
    for (auto n : s) {
      if (graph_->are_neighs(v, n)) neigh_cnt++;
      if (graph_->are_black_neighs(v, n)) black_cnt++;
    }
    return black_cnt > 0 && neigh_cnt == s.size();
  }

  bool RestrMultiple() override { return false; }

  std::unique_ptr<Graph> graph_;
};

extern template class BCCliqueEnumeration<
    product_graph_t<uint32_t, uint32_t, fast_graph_t>>;
extern template class BCCliqueEnumeration<
    product_graph_t<uint64_t, uint32_t, fast_graph_t>>;

#endif  // ENUMERABLE_BCCLIQUES_H
