#ifndef ENUMERABLE_DIAM2KPLEX_H
#define ENUMERABLE_DIAM2KPLEX_H

#include <iostream>
#include <vector>
#include "absl/strings/str_join.h"
#include "enumerable/enumerable.hpp"
#include "permute/permute.hpp"
#include "util/bitset.hpp"
#include "util/graph.hpp"
#include "util/serialize.hpp"

#ifdef DEGENERACY
#undef DEGENERACY
#endif
#define DEGENERACY

static const constexpr bool debug_mode = false;

template <typename node_t>
using Kplex = std::vector<node_t>;

template <typename Node, typename Graph>
void ListChildren(const Node& node, Node& child_node, size_t k, size_t q,
                  bool enable_pivoting, Graph* graph,
                  const std::vector<typename Graph::node_t>& subgraph,
                  const std::vector<std::vector<typename Graph::node_t>>& nadj,
                  const std::function<bool()>& cb) {
  if (debug_mode && node.ToItem(subgraph).size() == 1) {
    std::cout << "=============== CHANGE ROOT (" << subgraph[0]
              << ") ===============" << std::endl;
  }
  if (debug_mode) {
    std::cout << "NODE: " << absl::StrJoin(node.ToItem(subgraph), ", ") << " ";
    std::cout << "SUBGRAPH: " << absl::StrJoin(subgraph, ", ") << std::endl;
  }

  // Maximal node: no children
  if (node.IsMaximal(k)) return;
  if (debug_mode) std::cout << "NOT MAXIMAL" << std::endl;
  // Too few cands
  if (node.CanPrune(q)) return;
  if (debug_mode) std::cout << "CANNOT PRUNE" << std::endl;
  // Universal node in excluded: no valid children
  if (node.HasUniversalInExcl(graph, subgraph, nadj)) return;
  // if (debug_mode) std::cout << "NO UNIVERSAL IN EXCL" << std::endl;
  // Find total counters for nodes in cands.
  std::vector<size_t> universals;
  std::vector<size_t> sorted_cands;
  node.GetCands(graph, subgraph, nadj, enable_pivoting, k, &universals,
                &sorted_cands);

  // If there is at least one universal node in cands, fast forward.
  std::vector<size_t> add_to_excl;
  std::vector<bool> add_to_excl_bitset;
  add_to_excl_bitset.resize(subgraph.size());

  if (!universals.empty()) {
    child_node = node;
    for (size_t u : universals) {
      child_node.AddToKplex(graph, subgraph, nadj, u, k);
    }
    child_node.UpdateCandAndExcl(graph, subgraph, nadj, add_to_excl,
                                 add_to_excl_bitset, k);
    cb();
    return;
  }

  // Non-unary children
  for (size_t c : sorted_cands) {
    child_node = node;
    child_node.AddToKplex(graph, subgraph, nadj, c, k);
    child_node.UpdateCandAndExcl(graph, subgraph, nadj, add_to_excl,
                                 add_to_excl_bitset, k);

    if (!cb()) return;

    add_to_excl.push_back(c);
    add_to_excl_bitset[c] = true;
  }
}

template <typename Graph, size_t size>
struct Diam2KplexNodeImpl {};

template <typename Graph>
struct Diam2KplexNodeImpl<Graph, 0> {
  using node_t = typename Graph::node_t;
  std::vector<size_t> kplex;
  std::vector<uint8_t> counters;
  std::vector<uint8_t> status;
  std::vector<size_t> non_neighs;
  std::vector<size_t> cands;
  std::vector<size_t> excluded;
  static const uint8_t no_status = 0;
  static const uint8_t in_kplex = 1;
  static const uint8_t in_cand = 2;
  static const uint8_t in_excl = 3;
  void Serialize(std::vector<size_t>* out) const {
    ::Serialize(kplex, out);
    ::Serialize(counters, out);
    ::Serialize(status, out);
    ::Serialize(non_neighs, out);
    ::Serialize(cands, out);
    ::Serialize(excluded, out);
  }
  void Deserialize(const size_t** in) {
    ::Deserialize(in, &kplex);
    ::Deserialize(in, &counters);
    ::Deserialize(in, &status);
    ::Deserialize(in, &non_neighs);
    ::Deserialize(in, &cands);
    ::Deserialize(in, &excluded);
  }
  void Init(const Graph* graph, const std::vector<node_t>& subgraph,
            const std::vector<std::vector<node_t>>& nadj, size_t k) {
    kplex.clear();
    cands.clear();
    excluded.clear();
    counters.clear();
    status.clear();
    non_neighs.clear();
    counters.resize(subgraph.size());
    status.resize(subgraph.size());
    non_neighs.resize(k * subgraph.size());
    kplex.push_back(0);
    status[0] = in_kplex;
    for (size_t i : nadj[0]) {
      non_neighs[k * i + counters[i]++] = 0;
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
      status[i] = in_cand;
    }
  }

  bool IsMaximal(size_t k) const {
    if (kplex.size() + 1 > k) {
      return cands.empty() && excluded.empty();
    }
    for (size_t v : cands) {
      if (counters[v] != kplex.size()) return false;
    }
    for (size_t v : excluded) {
      if (counters[v] != kplex.size()) return false;
    }
    return true;
  }

  bool CanPrune(size_t q) const { return cands.size() + kplex.size() < q; }

  bool HasUniversalInExcl(const Graph* graph,
                          const std::vector<node_t>& subgraph,
                          const std::vector<std::vector<node_t>>& nadj) const {
    for (size_t u : excluded) {
      if (counters[u] != 0) continue;
      bool is_universal = true;
      for (size_t v : nadj[u]) {
        if (status[v] != in_cand) continue;
        is_universal = false;
        break;
      }
      if (is_universal) return true;
    }
    return false;
  }

  void GetCands(const Graph* graph, const std::vector<node_t>& subgraph,
                const std::vector<std::vector<node_t>>& nadj,
                bool enable_pivoting, size_t k, std::vector<size_t>* universals,
                std::vector<size_t>* real_cands) const {
    if (cands.size() == 0) return;
    thread_local std::vector<uint32_t> cand_counters;
    cand_counters.clear();
    cand_counters.resize(subgraph.size());
    for (size_t u : cands) {
      for (size_t v : nadj[u]) {
        if (status[v] == in_cand && u != v) cand_counters[u]++;
      }
      if (counters[u] == 0 && cand_counters[u] == 0) {
        universals->push_back(u);
      }
    }
    if (!universals->empty()) return;
    real_cands->clear();
    if (enable_pivoting && kplex.size() + 1 > k) {
      size_t best = cands[0];
      size_t best_not_cuts = cands.size();
      auto not_cuts = [&](size_t u, const std::function<void(size_t)>& cb) {
        if (k != 1) {
          for (size_t v : cands) {
            if (!graph->are_neighs(subgraph[u], subgraph[v])) {
              cb(v);
              continue;
            }
            bool ok = true;
            for (size_t i = k * u; i < k * u + counters[u]; i++) {
              if (!graph->are_neighs(subgraph[v], subgraph[non_neighs[i]])) {
                ok = false;
                break;
              }
            }
            if (!ok) cb(v);
          }
        } else {
          for (size_t v : nadj[u]) {
            if (status[v] == in_cand) cb(v);
          }
        }
      };
      for (size_t u : cands) {
        size_t count = 0;
        not_cuts(u, [&count](size_t v) { count++; });
        if (count < best_not_cuts) {
          best = u;
          best_not_cuts = count;
        }
      }
      for (size_t u : excluded) {
        size_t count = 0;
        not_cuts(u, [&count](size_t v) { count++; });
        if (count < best_not_cuts) {
          best = u;
          best_not_cuts = count;
        }
      }
      not_cuts(best, [real_cands](size_t v) { real_cands->push_back(v); });
    } else {
      for (size_t u : cands) {
        if (counters[u] != kplex.size()) {
          real_cands->push_back(u);
        }
      }
    }
    std::sort(real_cands->begin(), real_cands->end(), [&](size_t a, size_t b) {
      return counters[a] + cand_counters[a] > counters[b] + cand_counters[b];
    });
  }

  void AddToKplex(const Graph* graph, const std::vector<node_t>& subgraph,
                  const std::vector<std::vector<node_t>>& nadj, size_t c,
                  size_t k) {
    kplex.push_back(c);
    status[c] = in_kplex;
    if (debug_mode) {
      std::cout << "NEW KPLEX: " << absl::StrJoin(ToItem(subgraph), ", ")
                << std::endl;
      for (size_t i = 0; i < subgraph.size(); i++) {
        std::cout << subgraph[i] << " OLD COUNTER " << (size_t)counters[i]
                  << std::endl;
      }
    }
    for (size_t e : nadj[c]) {
      if (counters[e] < k) {
        non_neighs[k * e + counters[e]] = c;
      }
      counters[e]++;
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
                         const std::vector<std::vector<node_t>>& nadj,
                         const std::vector<size_t>& add_to_excl,
                         const std::vector<bool>& add_to_excl_bitset,
                         size_t k) {
    thread_local std::vector<size_t> excluded_cache;
    excluded_cache = excluded;
    excluded.clear();
    for (size_t e : excluded_cache) {
      if (CanAdd(graph, subgraph, nadj, k, e)) {
        excluded.push_back(e);
        status[e] = in_excl;
      } else {
        status[e] = no_status;
      }
    }
    for (size_t e : add_to_excl) {
      if (CanAdd(graph, subgraph, nadj, k, e)) {
        excluded.push_back(e);
        status[e] = in_excl;
      } else {
        status[e] = no_status;
      }
    }

    thread_local std::vector<size_t> cands_cache;
    cands_cache = cands;
    cands.clear();
    for (size_t e : cands_cache) {
      if (!add_to_excl_bitset[e] && CanAdd(graph, subgraph, nadj, k, e)) {
        cands.push_back(e);
        status[e] = in_cand;
      } else {
        status[e] = no_status;
      }
    }
    if (debug_mode && !std::is_sorted(cands.begin(), cands.end()))
      throw std::runtime_error("ciao");
  }

  bool HasDiameterTwo(const Graph* graph, const std::vector<node_t>& subgraph,
                      const std::vector<std::vector<node_t>>& nadj,
                      size_t k) const {
    if (kplex.size() + 2 > 2 * k) return true;
    if (k == 2) return true;
    for (size_t v : kplex) {
      for (size_t u : kplex) {
        if (graph->are_neighs(subgraph[u], subgraph[v])) continue;
        bool have_common_neigh = false;
        for (size_t x : kplex) {
          if (graph->are_neighs(subgraph[u], subgraph[x]) &&
              graph->are_neighs(subgraph[v], subgraph[x])) {
            have_common_neigh = true;
            break;
          }
        }
        if (!have_common_neigh) return false;
      }
    }
    return true;
  }

  bool IsReallyMaximal(const Graph* graph, const std::vector<node_t>& subgraph,
                       const std::vector<std::vector<node_t>>& nadj, size_t k,
                       size_t q) const {
    if (!IsMaximal(k)) return false;
    if (kplex.size() < q) return false;
    if (!HasDiameterTwo(graph, subgraph, nadj, k)) return false;
    // Check neighs of the first k nodes that are smaller than v.
    for (size_t i = 0; i < k && i < kplex.size(); i++) {
      for (node_t n : graph->neighs(subgraph[kplex[i]])) {
        if (n >= subgraph[0]) break;
        if (CanAdd(graph, subgraph, nadj, k, n, /*is_index*/ false)) {
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
  bool CanAdd(const Graph* graph, const std::vector<node_t>& subgraph,
              const std::vector<std::vector<node_t>>& nadj, size_t k,
              size_t idx, bool is_index = true) const {
    uint32_t v_cnt = 1;
    if (is_index && status[idx] == in_kplex) {
      if (debug_mode) std::cout << "NO (in kplex)" << std::endl;
      return false;
    }
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
  void Clear() {
    if (!subgraph_)
      subgraph_ = std::make_shared<std::vector<node_t>>();
    else
      subgraph_->clear();
  }
  void AddToSubgraph(node_t v) { subgraph_->push_back(v); }
  const std::vector<node_t>& Subgraph() const { return *subgraph_; }

  bool IsReallyMaximal(const Graph* graph, size_t k, size_t q) const {
    if (current_impl_ == 0)
      return impl0_.IsReallyMaximal(graph, *subgraph_, *nadj_, k, q);
    throw std::runtime_error("Invalid current implementation!");
  }

  Kplex<node_t> ToItem() const {
    if (current_impl_ == 0) return impl0_.ToItem(*subgraph_);
    throw std::runtime_error("Invalid current implementation!");
  }

  void Init(const Graph* graph, size_t k) {
    current_impl_ = 0;
    if (!nadj_) {
      nadj_ = std::make_shared<
          typename std::remove_reference<decltype(*nadj_)>::type>(
          subgraph_->size());
    } else {
      if (nadj_->size() < subgraph_->size()) {
        nadj_->resize(subgraph_->size());
      }
      for (auto& v : *nadj_) v.clear();
    }
    for (size_t i = 0; i < subgraph_->size(); i++) {
      for (size_t j = 0; j < subgraph_->size(); j++) {
        if (!graph->are_neighs((*subgraph_)[i], (*subgraph_)[j])) {
          (*nadj_)[i].push_back(j);
        }
      }
    }
    impl0_.Init(graph, *subgraph_, *nadj_, k);
  }

  void ListChildren(Diam2KplexNode<Graph>& child_node, size_t k, size_t q,
                    bool enable_pivoting, Graph* graph,
                    const std::function<bool()>& cb) const {
    child_node.current_impl_ = current_impl_;
    child_node.subgraph_ = subgraph_;
    child_node.nadj_ = nadj_;
    switch (current_impl_) {
      case 0:
        return ::ListChildren(impl0_, child_node.impl0_, k, q, enable_pivoting,
                              graph, *subgraph_, *nadj_, cb);
      default:
        throw std::runtime_error("Invalid current implementation!");
    }
  }

  void Serialize(std::vector<size_t>* out) const {
    ::Serialize(subgraph_, out);
    ::Serialize(nadj_, out);
    ::Serialize(impl0_, out);
    ::Serialize(current_impl_, out);
  }
  void Deserialize(const size_t** in) {
    ::Deserialize(in, &subgraph_);
    ::Deserialize(in, &nadj_);
    ::Deserialize(in, &impl0_);
    ::Deserialize(in, &current_impl_);
  }

 private:
  std::shared_ptr<std::vector<node_t>> subgraph_;
  // Complement of the subgraph induced by subgraph_
  std::shared_ptr<std::vector<std::vector<node_t>>> nadj_;
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

  explicit Diam2KplexEnumeration(Graph* graph, size_t k, size_t q,
                                 bool enable_pivoting)
      : graph_(), k_(k), q_(q), enable_pivoting_(enable_pivoting) {
#ifdef DEGENERACY
    auto degen = DegeneracyOrder(*graph);
    inverse_perm = degen;
    for (int i = 0; i < degen.size(); i++) {
      inverse_perm[i] = degen[i];
    }
    graph_ = graph->Permute(degen);
#else
    graph_ = graph->Clone();
#endif
  }

  void SetUp() override {}

  bool CanUseRecursion() override { return true; }

  size_t MaxRoots() override { return graph_->size(); }

  Kplex<node_t> NodeToItem(const Diam2KplexNode<Graph>& node) override {
#ifdef DEGENERACY
    auto kp = ((const Diam2KplexEnumeration*)this)->NodeToItem(node);
    Kplex<node_t> inv_permuted(kp.size());
    for (int i = 0; i < kp.size(); i++) {
      inv_permuted[i] = inverse_perm[kp[i]];
    }
    return inv_permuted;
#else
    return ((const Diam2KplexEnumeration*)this)->NodeToItem(node);
#endif
  }

  Kplex<node_t> NodeToItem(const Diam2KplexNode<Graph>& node) const {
    return node.ToItem();
  }

  bool IsSolution(const Diam2KplexNode<Graph>& node) override {
    bool ret = node.IsReallyMaximal(graph_.get(), k_, q_);
    if (debug_mode && ret) {
      std::cout << "\033[31;1;4mREAL_SOL: "
                << absl::StrJoin(NodeToItem(node), ", ") << "\033[0m"
                << std::endl;
    }

    return ret;
  }

  void GetRoot(size_t v, const NodeCallback& cb) override {
    // TODO: euristica vicini indietro
    if (graph_->fwd_degree(v) + k_ < q_) return;
    thread_local Diam2KplexNode<Graph> node;
    node.Clear();
    thread_local std::vector<bool> subgraph_added(graph_->size());
    subgraph_added[v] = true;
    node.AddToSubgraph(v);
    std::vector<node_t> sg;
    for (node_t n : graph_->fwd_neighs(v)) {
      sg.push_back(n);
      subgraph_added[n] = true;
    }
    auto filter_sg = [&](const std::vector<node_t>& sg) {
      std::vector<node_t> filtered;
      for (size_t i = 0; i < sg.size(); i++) {
        size_t count = 0;
        for (size_t j = 0; j < sg.size(); j++) {
          if (graph_->are_neighs(sg[i], sg[j])) count++;
        }
        if (count + 2 * k_ >= q_) filtered.push_back(sg[i]);
      }
      return filtered;
    };
    size_t sz;
    do {
      sz = sg.size();
      sg = filter_sg(sg);
    } while (sg.size() != sz);
    for (node_t n : sg) {
      node.AddToSubgraph(n);
    }
    if (k_ != 1) {
      thread_local std::vector<node_t> subgraph_candidates;
      thread_local std::vector<uint32_t> subgraph_counts(graph_->size());
      for (node_t n : sg) {
        for (node_t nn : graph_->neighs(n)) {
          if (nn > v && !subgraph_added[nn]) {
            if (!subgraph_counts[nn]) subgraph_candidates.push_back(nn);
            subgraph_counts[nn]++;
          }
        }
      }
      for (node_t n : subgraph_candidates) {
        if (subgraph_counts[n] + 2 * k_ >= q_ + 2) {
          node.AddToSubgraph(n);
        }
        subgraph_counts[n] = 0;
      }
      subgraph_candidates.clear();
    }
    for (node_t n : node.Subgraph()) subgraph_added[n] = false;
    for (node_t n : graph_->fwd_neighs(v)) subgraph_added[n] = false;
    node.Init(graph_.get(), k_);
    cb(node);
  }

  void ListChildren(const Diam2KplexNode<Graph>& node,
                    const NodeCallback& cb) override {
    Diam2KplexNode<Graph> child_node;
    node.ListChildren(child_node, k_, q_, enable_pivoting_, graph_.get(),
                      [&]() { return cb(child_node); });
  }

 private:
  std::unique_ptr<Graph> graph_;
  const size_t k_;
  const size_t q_;
  const bool enable_pivoting_;
#ifdef DEGENERACY
  std::vector<node_t> inverse_perm;
#endif
};

extern template class Diam2KplexEnumeration<fast_graph_t<uint32_t, void>>;
extern template class Diam2KplexEnumeration<fast_graph_t<uint64_t, void>>;

#endif  // ENUMERABLE_DIAM2KPLEX_H
