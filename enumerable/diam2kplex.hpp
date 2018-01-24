#ifndef ENUMERABLE_DIAM2KPLEX_H
#define ENUMERABLE_DIAM2KPLEX_H

#include <iostream>
#include <vector>
#include "absl/strings/str_join.h"
#include "enumerable/enumerable.hpp"
#include "permute/permute.hpp"
#include "util/graph.hpp"

#ifdef DEGENERACY
#undef DEGENERACY
#endif
#define DEGENERACY

static const constexpr bool debug_mode = false;

template <typename node_t>
using Kplex = std::vector<node_t>;

template <typename Graph, size_t size>
struct Diam2KplexNodeImpl {
  // TODO
};

template <typename Graph>
struct Diam2KplexNodeImpl<Graph, 0> {
  using node_t = typename Graph::node_t;
  std::vector<size_t> kplex;
  std::vector<uint8_t> counters;
  std::vector<size_t> cands;
  std::vector<size_t> excluded;
  void Init(const Graph* graph, const std::vector<node_t>& subgraph) {
    kplex.clear();
    cands.clear();
    excluded.clear();
    counters.clear();
    kplex.push_back(0);
    counters.resize(subgraph.size());
    for (size_t i = 0; i < subgraph.size(); i++) {
      counters[i] = !graph->are_neighs(subgraph[0], subgraph[i]);
    }
    if (debug_mode) {
      std::cout << "SUBGRAPH FOR " << subgraph[0] << std::endl;
      for (size_t i = 0; i < subgraph.size(); i++) {
        std::cout << subgraph[i] << " HAS COUNTER " << (size_t)counters[i]
                  << std::endl;
      }
    }
    for (size_t i = 1; i < subgraph.size(); i++) {
      cands.push_back(i);
    }
  }

  bool IsMaximal() const { return cands.empty() && excluded.empty(); }

  bool CanPrune(size_t q) const { return cands.size() + kplex.size() < q; }

  bool HasUniversalInExcl(const Graph* graph,
                          const std::vector<node_t>& subgraph) const {
    for (size_t u : excluded) {
      if (counters[u] != 0) continue;
      bool is_universal = true;
      for (size_t v : cands) {
        if (!graph->are_neighs(subgraph[u], subgraph[v])) {
          is_universal = false;
          break;
        }
      }
      if (is_universal) return true;
    }
    return false;
  }

  void GetUniversalsAndSortedCands(const Graph* graph,
                                   const std::vector<node_t>& subgraph,
                                   std::vector<size_t>* universals,
                                   std::vector<size_t>* sorted_cands) const {
    thread_local std::vector<uint32_t> cand_counters;
    cand_counters.clear();
    cand_counters.resize(subgraph.size());
    for (size_t u : cands) {
      for (size_t v : cands) {
        if (u != v && !graph->are_neighs(subgraph[u], subgraph[v])) {
          cand_counters[u]++;
        }
      }
      if (counters[u] == 0 && cand_counters[u] == 0) {
        universals->push_back(u);
      }
    }
    *sorted_cands = cands;
    std::sort(sorted_cands->begin(), sorted_cands->end(),
              [&](size_t a, size_t b) {
                return counters[a] + cand_counters[a] >
                       counters[b] + cand_counters[b];
              });
  }

  void AddToKplex(const Graph* graph, const std::vector<node_t>& subgraph,
                  size_t c) {
    kplex.push_back(c);
    if (debug_mode) {
      std::cout << "NEW KPLEX: " << absl::StrJoin(ToItem(subgraph), ", ")
                << std::endl;
      for (size_t i = 0; i < subgraph.size(); i++) {
        std::cout << subgraph[i] << " OLD COUNTER " << (size_t)counters[i]
                  << std::endl;
      }
    }
    for (size_t e = 0; e < subgraph.size(); e++) {
      if (!graph->are_neighs(subgraph[c], subgraph[e])) {
        counters[e]++;
      }
    }
    if (debug_mode) {
      for (size_t i = 0; i < subgraph.size(); i++) {
        std::cout << subgraph[i] << " NEW COUNTER " << (size_t)counters[i]
                  << std::endl;
      }
    }
  }

  void UpdateCandAndExcl(const Graph* graph,
                         const std::vector<node_t>& subgraph,
                         const std::vector<size_t>& add_to_excl,
                         const std::vector<bool>& add_to_excl_bitset,
                         size_t k) {
    thread_local std::vector<size_t> excluded_cache;
    excluded_cache = excluded;
    excluded.clear();
    for (size_t e : excluded_cache) {
      if (CanAdd(graph, subgraph, k, e)) {
        excluded.push_back(e);
      }
    }
    for (size_t e : add_to_excl) {
      if (CanAdd(graph, subgraph, k, e)) {
        excluded.push_back(e);
      }
    }

    thread_local std::vector<size_t> cands_cache;
    cands_cache = cands;
    cands.clear();
    for (size_t e : cands_cache) {
      if (!add_to_excl_bitset[e] && CanAdd(graph, subgraph, k, e)) {
        cands.push_back(e);
      }
    }
  }

  bool IsReallyMaximal(const Graph* graph, const std::vector<node_t>& subgraph,
                       size_t k, size_t q) const {
    if (kplex.size() < q) return false;
    // TODO: diameter
    if (k > 2) throw std::runtime_error("ciao ciao");
    if (k == 2 && kplex.size() == 2) {
      if (!graph->are_neighs(subgraph[kplex[0]], subgraph[kplex[1]])) {
        return false;
      }
    }
    // Check neighs of the first k nodes that are smaller than v.
    for (size_t i = 0; i < k && i < kplex.size(); i++) {
      for (node_t n : graph->neighs(subgraph[kplex[i]])) {
        if (n >= subgraph[0]) break;
        if (CanAdd(graph, subgraph, k, n, /*is_index*/ false)) {
          return false;
        }
      }
    }
    return true;
  }

  Kplex<node_t> ToItem(const std::vector<node_t>& subgraph) const {
    std::vector<node_t> sol;
    for (size_t i : kplex) {
      sol.push_back(subgraph[i]);
    }
    return sol;
  }

 protected:
  bool CanAdd(const Graph* graph, const std::vector<node_t>& subgraph, size_t k,
              size_t idx, bool is_index = true) const {
    uint32_t v_cnt = 1;
    size_t v = is_index ? subgraph[idx] : idx;
    if (debug_mode) {
      std::cout << "CAN_ADD " << v << " TO "
                << absl::StrJoin(ToItem(subgraph), ", ") << std::endl;
      std::cout << "COUNTERS: " << std::endl;
      for (size_t i = 0; i < kplex.size(); i++) {
        std::cout << subgraph[kplex[i]] << " HAS COUNTER "
                  << (size_t)counters[kplex[i]] << std::endl;
      }
    }
    for (size_t i = 0; i < kplex.size(); i++) {
      if (v == subgraph[kplex[i]]) {
        if (debug_mode) std::cout << "NO (in kplex)" << std::endl;
        return false;
      }
      if (!graph->are_neighs(v, subgraph[kplex[i]])) {
        v_cnt++;
        if ((size_t)counters[kplex[i]] + 1 > k) {
          if (debug_mode)
            std::cout << "NO (counter of " << subgraph[kplex[i]] << ")"
                      << std::endl;
          return false;
        }
      }
    }
    if (v_cnt > k) {
      if (debug_mode) std::cout << "NO (v_cnt)" << std::endl;
      return false;
    }
    if (debug_mode) std::cout << "YES" << std::endl;
    return true;
  }
};

template <typename Graph>
class Diam2KplexNode {
 public:
  using node_t = typename Graph::node_t;
  void Clear() { subgraph_ = std::make_shared<std::vector<node_t>>(); }
  void AddToSubgraph(node_t v) { subgraph_->push_back(v); }
  const std::vector<node_t>& Subgraph() const { return *subgraph_; }

  bool IsMaximal() const {
    if (current_impl_ == 0) return impl0_.IsMaximal();
    throw std::runtime_error("Invalid current implementation!");
  }

  bool CanPrune(size_t q) const {
    if (current_impl_ == 0) return impl0_.CanPrune(q);
    throw std::runtime_error("Invalid current implementation!");
  }

  bool HasUniversalInExcl(const Graph* graph) const {
    if (current_impl_ == 0) return impl0_.HasUniversalInExcl(graph, *subgraph_);
    throw std::runtime_error("Invalid current implementation!");
  }

  void GetUniversalsAndSortedCands(const Graph* graph,
                                   std::vector<size_t>* universals,
                                   std::vector<size_t>* sorted_cands) const {
    if (current_impl_ == 0)
      return impl0_.GetUniversalsAndSortedCands(graph, *subgraph_, universals,
                                                sorted_cands);
    throw std::runtime_error("Invalid current implementation!");
  }

  void AddToKplex(const Graph* graph, size_t idx) {
    if (current_impl_ == 0) return impl0_.AddToKplex(graph, *subgraph_, idx);
    throw std::runtime_error("Invalid current implementation!");
  }

  void UpdateCandAndExcl(const Graph* graph,
                         const std::vector<size_t>& add_to_excl,
                         const std::vector<bool>& add_to_excl_bitset,
                         size_t k) {
    if (current_impl_ == 0)
      return impl0_.UpdateCandAndExcl(graph, *subgraph_, add_to_excl,
                                      add_to_excl_bitset, k);
    throw std::runtime_error("Invalid current implementation!");
  }

  bool IsReallyMaximal(const Graph* graph, size_t k, size_t q) const {
    if (current_impl_ == 0)
      return impl0_.IsReallyMaximal(graph, *subgraph_, k, q);
    throw std::runtime_error("Invalid current implementation!");
  }

  Kplex<node_t> ToItem() const {
    if (current_impl_ == 0) return impl0_.ToItem(*subgraph_);
    throw std::runtime_error("Invalid current implementation!");
  }

  void Init(const Graph* graph) {
    // TODO
    impl0_.Init(graph, *subgraph_);
  }

 private:
  std::shared_ptr<std::vector<node_t>> subgraph_;
  Diam2KplexNodeImpl<Graph, 0> impl0_;
  size_t current_impl_ = 0;
};

template <typename Graph>
class Diam2KplexEnumeration
    : public Enumerable<Diam2KplexNode<Graph>, Kplex<typename Graph::node_t>> {
 public:
  using node_t = typename Graph::node_t;
  using NodeCallback =
      typename Enumerable<Diam2KplexNode<Graph>,
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

  Kplex<node_t> NodeToItem(const Diam2KplexNode<Graph>& node) override {
    return ((const Diam2KplexEnumeration*)this)->NodeToItem(node);
  }

  Kplex<node_t> NodeToItem(const Diam2KplexNode<Graph>& node) const {
    return node.ToItem();
  }

  bool IsSolution(const Diam2KplexNode<Graph>& node) override {
    bool ret = node.IsMaximal() && node.IsReallyMaximal(graph_.get(), k_, q_);
    if (debug_mode && ret) {
      std::cout << "\033[31;1;4mREAL_SOL: "
                << absl::StrJoin(NodeToItem(node), ", ") << "\033[0m"
                << std::endl;
    }

    return ret;
  }

  void GetRoot(size_t v, const NodeCallback& cb) override {
    // TODO: euristica vicini indietro
    thread_local Diam2KplexNode<Graph> node;
    node.Clear();
    thread_local std::vector<bool> subgraph_added(graph_->size());
    node.AddToSubgraph(v);
    subgraph_added[v] = true;
    for (node_t n : graph_->fwd_neighs(v)) {
      node.AddToSubgraph(n);
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
          node.AddToSubgraph(n);
        }
        subgraph_counts[n] = 0;
      }
      subgraph_candidates.clear();
    }
    for (node_t n : node.Subgraph()) subgraph_added[n] = false;
    node.Init(graph_.get());
    cb(node);
  }

  void ListChildren(const Diam2KplexNode<Graph>& node,
                    const NodeCallback& cb) override {
    if (debug_mode && node.ToItem().size() == 1) {
      std::cout << "=============== CHANGE ROOT (" << node.Subgraph()[0]
                << ") ===============" << std::endl;
    }
    if (debug_mode) {
      std::cout << "NODE: " << absl::StrJoin(node.ToItem(), ", ") << " ";
      std::cout << "SUBGRAPH: " << absl::StrJoin(node.Subgraph(), ", ")
                << std::endl;
    }

    // Maximal node: no children
    if (node.IsMaximal()) return;
    if (debug_mode) std::cout << "NOT MAXIMAL" << std::endl;
    // Too few cands
    if (node.CanPrune(q_)) return;
    if (debug_mode) std::cout << "CANNOT PRUNE" << std::endl;
    // Universal node in excluded: no valid children
    if (node.HasUniversalInExcl(graph_.get())) return;
    if (debug_mode) std::cout << "NO UNIVERSAL IN EXCL" << std::endl;
    // Find total counters for nodes in cands.
    thread_local std::vector<size_t> universals;
    universals.clear();
    thread_local std::vector<size_t> sorted_cands;
    sorted_cands.clear();
    node.GetUniversalsAndSortedCands(graph_.get(), &universals, &sorted_cands);

    thread_local std::vector<size_t> add_to_excl;
    thread_local std::vector<bool> add_to_excl_bitset;
    add_to_excl.clear();
    add_to_excl_bitset.clear();
    add_to_excl_bitset.resize(node.Subgraph().size());

    // If there is at least one universal node in cands, fast forward.
    thread_local Diam2KplexNode<Graph> child_node;
    if (!universals.empty()) {
      child_node = node;
      for (size_t u : universals) {
        child_node.AddToKplex(graph_.get(), u);
      }
      child_node.UpdateCandAndExcl(graph_.get(), add_to_excl,
                                   add_to_excl_bitset, k_);
      cb(child_node);
      return;
    }
    // Non-unary children
    for (size_t c : sorted_cands) {
      child_node = node;
      /*if (node.counters[c] == 0) continue; TODO: maximality*/
      child_node.AddToKplex(graph_.get(), c);
      child_node.UpdateCandAndExcl(graph_.get(), add_to_excl,
                                   add_to_excl_bitset, k_);

      cb(child_node);

      add_to_excl.push_back(c);
      add_to_excl_bitset[c] = true;
    }
  }

 private:
  std::unique_ptr<Graph> graph_;
  const size_t k_;
  const size_t q_;
};

extern template class Diam2KplexEnumeration<fast_graph_t<uint32_t, void>>;
extern template class Diam2KplexEnumeration<fast_graph_t<uint64_t, void>>;

#endif  // ENUMERABLE_DIAM2KPLEX_H
