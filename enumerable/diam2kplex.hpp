#ifndef ENUMERABLE_DIAM2KPLEX_H
#define ENUMERABLE_DIAM2KPLEX_H

#include <iostream>
#include <vector>
#include "absl/strings/str_join.h"
#include "enumerable/enumerable.hpp"
#include "permute/permute.hpp"
#include "util/graph.hpp"

/*#ifdef DEGENERACY
#undef DEGENERACY
#endif*/
#define DEGENERACY

static const constexpr bool debug_mode = false;

template <typename node_t>
using Kplex = std::vector<node_t>;

template <typename node_t>
struct Diam2KplexNode {
  node_t v;
  std::vector<node_t> subgraph;
  std::vector<size_t> kplex;
  std::vector<uint8_t> counters;
  std::vector<size_t> cands;
  std::vector<size_t> excluded;
  bool IsMaximal() const { return cands.empty() && excluded.empty(); }
};

template <typename Graph>
class Diam2KplexEnumeration
    : public Enumerable<Diam2KplexNode<typename Graph::node_t>,
                        Kplex<typename Graph::node_t>> {
 public:
  using node_t = typename Graph::node_t;
  using NodeCallback =
      typename Enumerable<Diam2KplexNode<typename Graph::node_t>,
                          Kplex<typename Graph::node_t>>::NodeCallback;
  explicit Diam2KplexEnumeration(Graph* graph, size_t k, size_t q)
      : graph_(
#ifndef DEGENERACY
            graph->Clone()
#else
            graph->Permute(DegeneracyOrder(*graph))
#endif
                ),
        k_(k),
        q_(q) {
  }

  void SetUp() override {}

  size_t MaxRoots() override { return graph_->size(); }

  Kplex<node_t> NodeToItem(const Diam2KplexNode<node_t>& node) override {
    return ((const Diam2KplexEnumeration*)this)->NodeToItem(node);
  }

  Kplex<node_t> NodeToItem(const Diam2KplexNode<node_t>& node) const {
    std::vector<node_t> sol;
    for (size_t i : node.kplex) {
      sol.push_back(node.subgraph[i]);
    }
    return sol;
  }

  bool IsSolution(const Diam2KplexNode<node_t>& node) override {
    bool ret = node.IsMaximal() && IsReallyMaximal(node);
    if (debug_mode && ret) {
      std::cout << "\033[31;1;4mREAL_SOL: "
                << absl::StrJoin(NodeToItem(node), ", ") << "\033[0m"
                << std::endl;
    }
    return ret;
  }

  void GetRoot(size_t v, const NodeCallback& cb) override {
    // TODO: euristica vicini indietro
    thread_local Diam2KplexNode<node_t> node;
    node.counters.clear();
    node.subgraph.clear();
    node.kplex.clear();
    node.cands.clear();
    node.excluded.clear();
    node.v = v;
    thread_local std::vector<bool> subgraph_added(graph_->size());
    node.subgraph.push_back(v);
    node.counters.push_back(1);
    subgraph_added[v] = true;
    for (node_t n : graph_->fwd_neighs(v)) {
      node.subgraph.push_back(n);
      node.counters.push_back(0);
      subgraph_added[n] = true;
    }
    if (k_ != 1) {
      thread_local std::vector<node_t> subgraph_candidates;
      thread_local std::vector<uint32_t> subgraph_counts(graph_->size());
      for (node_t n : graph_->fwd_neighs(v)) {
        for (node_t nn : graph_->neighs(n)) {
          if (nn > v && !subgraph_added[nn]) {
            if (!subgraph_counts[nn]) subgraph_candidates.push_back(nn);
            subgraph_counts[nn]++;
          }
        }
      }
      for (node_t n : subgraph_candidates) {
        if (subgraph_counts[n] + 2 * k_ > q_) {
          node.subgraph.push_back(n);
          node.counters.push_back(1);
        }
        subgraph_counts[n] = 0;
      }
      subgraph_candidates.clear();
    }
    for (node_t n : node.subgraph) subgraph_added[n] = false;
    node.kplex.push_back(0);
    for (size_t i = 1; i < node.subgraph.size(); i++) {
      node.cands.push_back(i);
    }
    cb(node);
  }

  void ListChildren(const Diam2KplexNode<node_t>& node,
                    const NodeCallback& cb) override {
    if (debug_mode && node.kplex.size() == 1) {
      std::cout << "=============== CHANGE ROOT (" << node.v
                << ") ===============" << std::endl;
    }
    if (debug_mode) {
      std::cout << "NODE: " << absl::StrJoin(NodeToItem(node), ", ") << " ";
      std::cout << "SUBGRAPH: " << absl::StrJoin(node.subgraph, ", ") << " ";
      std::cout << "KPLEX: " << absl::StrJoin(node.kplex, ", ") << " ";
      std::cout << "COUNTERS: " << absl::StrJoin(node.counters, ", ") << " ";
      std::cout << "CANDS: " << absl::StrJoin(node.cands, ", ") << " ";
      std::cout << "EXCL: " << absl::StrJoin(node.excluded, ", ") << std::endl;
    }
    // Maximal node: no children
    if (node.IsMaximal()) return;
    // Too few cands
    if (node.cands.size() + node.kplex.size() < q_) return;
    thread_local const auto check_neighs = [this, &node](size_t a, size_t b) {
      return graph_->are_neighs(node.subgraph[a], node.subgraph[b]);
    };
    // Universal node in excluded: no valid children
    for (size_t u : node.excluded) {
      if (node.counters[u] != 0) continue;
      bool is_universal = true;
      for (size_t v : node.cands) {
        if (!check_neighs(u, v)) {
          is_universal = false;
          break;
        }
      }
      if (is_universal) return;
    }
    // Find total counters for nodes in cands.
    thread_local std::vector<uint32_t> cand_counters;
    cand_counters.clear();
    cand_counters.resize(node.subgraph.size());
    thread_local std::vector<size_t> universals;
    universals.clear();
    for (size_t u : node.cands) {
      for (size_t v : node.cands) {
        if (u != v && !check_neighs(u, v)) {
          cand_counters[u]++;
        }
      }
      if (node.counters[u] == 0 && cand_counters[u] == 0) {
        universals.push_back(u);
      }
    }
    // If there is at least one universal node in cands, fast forward.
    if (!universals.empty()) {
      if (debug_mode)
        std::cout << "UNIV: " << absl::StrJoin(universals, ", ") << std::endl;
      thread_local Diam2KplexNode<node_t> child_node;
      child_node = node;
      for (size_t u : universals) {
        child_node.kplex.push_back(u);
        child_node.counters[u]++;
        for (size_t e : child_node.excluded) {
          if (!check_neighs(u, e)) {
            child_node.counters[e]++;
          }
        }
      }
      child_node.excluded.clear();
      for (size_t e : node.excluded) {
        if (child_node.counters[e] < k_) {
          child_node.excluded.push_back(e);
        }
      }
      child_node.cands.clear();
      for (size_t c : node.cands) {
        if (node.counters[c] != 0 || cand_counters[c] != 0) {
          child_node.cands.push_back(c);
        }
      }
      cb(child_node);
      return;
    }
    // Sorted cands
    thread_local std::vector<size_t> sorted_cands;
    sorted_cands = node.cands;
    std::sort(sorted_cands.begin(), sorted_cands.end(),
              [&](size_t a, size_t b) {
                return node.counters[a] + cand_counters[a] >
                       node.counters[b] + cand_counters[b];
              });
    thread_local std::vector<size_t> child_potential_excluded;
    child_potential_excluded = node.excluded;
    thread_local std::vector<bool> processed_cands(graph_->size());
    thread_local Diam2KplexNode<node_t> child_node;
    child_node = node;
    // Non-unary children
    for (size_t c : sorted_cands) {
      /*if (node.counters[c] == 0) continue; TODO: maximality*/
      child_node.kplex.push_back(c);
      for (size_t e : child_node.kplex) {
        if (!check_neighs(c, e)) {
          child_node.counters[e]++;
        }
      }

      child_node.excluded.clear();
      for (size_t e : child_potential_excluded) {
        if (CanAdd(child_node.kplex, child_node.counters, e, check_neighs)) {
          child_node.excluded.push_back(e);
        }
      }
      child_node.cands.clear();
      for (size_t e : node.cands) {
        if (!processed_cands[e] &&
            CanAdd(child_node.kplex, child_node.counters, e, check_neighs)) {
          child_node.cands.push_back(e);
        }
      }

      for (size_t e : child_node.excluded) {
        if (!check_neighs(c, e)) {
          child_node.counters[e]++;
        }
      }
      for (size_t e : child_node.cands) {
        if (!check_neighs(c, e)) {
          child_node.counters[e]++;
        }
      }
      cb(child_node);
      for (size_t e : child_node.excluded) {
        if (!check_neighs(c, e)) {
          child_node.counters[e]--;
        }
      }
      for (size_t e : child_node.cands) {
        if (!check_neighs(c, e)) {
          child_node.counters[e]--;
        }
      }
      for (size_t e : child_node.kplex) {
        if (!check_neighs(c, e)) {
          child_node.counters[e]--;
        }
      }

      child_node.kplex.pop_back();
      child_potential_excluded.push_back(c);
      processed_cands[c] = true;
    }
    for (node_t c : sorted_cands) {
      processed_cands[c] = false;
    }
  }

 protected:
  template <typename F>
  bool CanAdd(const std::vector<size_t>& kplex,
              const std::vector<uint8_t>& counters, size_t v,
              const F& check_neighs) const {
    uint32_t v_cnt = 1;
    for (size_t i = 0; i < kplex.size(); i++) {
      if (v == kplex[i]) return false;
      if (!check_neighs(v, kplex[i])) {
        v_cnt++;
        if ((size_t)counters[kplex[i]] + 1 > k_) {
          return false;
        }
      }
    }
    if (v_cnt > k_) return false;
    return true;
  }

  bool CanAdd(const std::vector<node_t>& subgraph,
              const std::vector<size_t>& kplex,
              const std::vector<uint8_t>& counters, node_t v) const {
    uint32_t v_cnt = 1;
    for (size_t i = 0; i < kplex.size(); i++) {
      if (v == subgraph[kplex[i]]) return false;
      if (!graph_->are_neighs(v, subgraph[kplex[i]])) {
        v_cnt++;
        if ((size_t)counters[kplex[i]] + 1 > k_) {
          return false;
        }
      }
    }
    if (v_cnt > k_) return false;
    return true;
  }

  bool IsReallyMaximal(const Diam2KplexNode<node_t>& node) const {
    if (node.kplex.size() < q_) return false;
    // TODO: diameter
    if (k_ > 2) throw std::runtime_error("ciao ciao");
    if (k_ == 2 && node.kplex.size() == 2) {
      if (!graph_->are_neighs(node.subgraph[node.kplex[0]],
                              node.subgraph[node.kplex[1]])) {
        return false;
      }
    }
    if (debug_mode) {
      std::cout << "SOL: " << absl::StrJoin(NodeToItem(node), ", ")
                << std::endl;
      for (size_t i : node.kplex)
        std::cout << node.subgraph[i] << " -> " << (size_t)node.counters[i]
                  << std::endl;
    }
    // Check neighs of the first k nodes that are smaller than v.
    for (size_t i = 0; i < k_ && i < node.kplex.size(); i++) {
      for (node_t n : graph_->neighs(node.subgraph[node.kplex[i]])) {
        if (n >= node.v) break;
        if (debug_mode) std::cout << "ADD: " << n << std::endl;
        if (CanAdd(node.subgraph, node.kplex, node.counters, n)) {
          if (debug_mode) std::cout << "CAN ADD" << std::endl;
          return false;
        }
        if (debug_mode) std::cout << "CANNOT ADD" << std::endl;
      }
    }

    return true;
  }

 private:
  std::unique_ptr<Graph> graph_;
  const size_t k_;
  const size_t q_;
};

extern template class Diam2KplexEnumeration<fast_graph_t<uint32_t, void>>;
extern template class Diam2KplexEnumeration<fast_graph_t<uint64_t, void>>;

#endif  // ENUMERABLE_DIAM2KPLEX_H
