#ifndef ENUMERABLE_CLIQUE_H
#define ENUMERABLE_CLIQUE_H
#include <cstdint>
#include <memory>
#include <vector>

#include "enumerable/enumerable.hpp"
#include "permute/permute.hpp"
#include "util/graph.hpp"

template <typename node_t>
using Clique = std::vector<node_t>;

template <typename node_t>
using CliqueEnumerationNode = std::pair<Clique<node_t>, node_t>;

#define DEGENERACY

template <typename Graph>
class CliqueEnumeration
    : public Enumerable<CliqueEnumerationNode<typename Graph::node_t>,
                        Clique<typename Graph::node_t>> {
 public:
  using node_t = typename Graph::node_t;
  using NodeCallback = typename Enumerable<CliqueEnumerationNode<node_t>,
                                           Clique<node_t>>::NodeCallback;

  explicit CliqueEnumeration(Graph* graph) : graph_() {
#ifndef DEGENERACY
    graph_ = graph->Clone();
#else
    auto degen = DegeneracyOrder(*graph);
    inverse_perm = degen;
    for (int i = 0; i < degen.size(); i++) {
      inverse_perm[i] = degen[i];
    }
    graph_ = graph->Permute(degen);
#endif
  }

  void SetUp() override {
    candidates_bitset_.resize(graph_->size());
    bad_bitset_.resize(graph_->size());
  }

  size_t MaxRoots() override { return graph_->size(); }

  void GetRoot(size_t i, const NodeCallback& cb) override {
    std::pair<std::vector<node_t>, node_t> root;
    if (graph_->degree(i) == graph_->fwd_degree(i)) {
      root.first.clear();
      root.first.push_back(i);
      root.second = i;
      CompleteFwd(&root.first);
      cb(root);
    }
  }

  void ListChildren(const CliqueEnumerationNode<node_t>& clique_info,
                    const NodeCallback& cb) override {
    const std::vector<node_t>& clique = clique_info.first;
    node_t parind = clique_info.second;
    assert(!clique.empty());
    node_t max_bad = clique.front();
    for (node_t neigh : graph_->fwd_neighs(clique.front())) {
      bool ok = true;
      for (node_t node : clique) {
        if (node > neigh) break;
        if (!graph_->are_neighs(neigh, node)) {
          ok = false;
          break;
        }
      }
      if (ok) {
        bad_bitset_[neigh] = true;
        bad_.push_back(neigh);
      }
      if (ok && max_bad < neigh) {
        max_bad = neigh;
      }
    }
    Candidates(clique, parind, [this, max_bad, &clique, &cb](node_t cand) {
      return ChildrenCand(
          clique, cand,
          [this, max_bad, cand, &clique, &cb](std::vector<node_t>* child) {
            node_t idx = child->front();
            for (node_t node : *child)
              if (graph_->degree(node) < graph_->degree(idx)) {
                idx = node;
              }
            for (node_t neigh : graph_->neighs(idx)) {
              bool works = true;
              for (node_t node : *child) {
                if (!graph_->are_neighs(neigh, node)) {
                  works = false;
                  break;
                }
              }
              if (works) {
                if (neigh < clique.front()) return true;
                if (neigh <= cand && graph_->are_neighs(neigh, cand))
                  return true;
                if (bad_bitset_[neigh]) return true;
              }
            }
            std::pair<std::vector<node_t>, node_t> child_info;
            child_info.second = cand;
            child->push_back(cand);
            CompleteFwd(child);
            child_info.first = *child;
            return cb(child_info);
          });
    });
    for (node_t b : bad_) bad_bitset_[b] = false;
    bad_.clear();
  }

  Clique<node_t> NodeToItem(
      const CliqueEnumerationNode<node_t>& node) override {
#ifdef DEGENERACY
    Clique<node_t> inv_permuted(node.first.size());
    for (int i = 0; i < node.first.size(); i++) {
      inv_permuted[i] = inverse_perm[node.first[i]];
    }
    return inv_permuted;
#else
    return node.first;
#endif
  }

 private:
  void CompleteSmallest(std::vector<node_t>* clique, node_t small) {
    for (node_t neigh : graph_->fwd_neighs(small)) {
      bool can_add = true;
      for (node_t node : *clique) {
        if (!graph_->are_neighs(neigh, node)) {
          can_add = false;
          break;
        }
      }
      if (can_add) clique->push_back(neigh);
    }
  }

  void Candidates(const std::vector<node_t>& clique, node_t parind,
                  const std::function<bool(node_t)>& callback) {
    for (node_t node : clique) {
      candidates_bitset_[node] = true;
    }
    for (node_t node : clique) {
      for (node_t neigh : graph_->fwd_neighs(node)) {
        if (neigh > parind && !candidates_bitset_[neigh]) {
          candidates_bitset_[neigh] = true;
          candidates_.push_back(neigh);
        }
      }
    }
    for (node_t node : clique) {
      candidates_bitset_[node] = false;
    }
    for (node_t node : candidates_) {
      if (!callback(node)) {
        break;
      }
    }
    for (node_t node : candidates_) {
      candidates_bitset_[node] = false;
    }
    candidates_.clear();
  }

  bool ChildrenCand(const std::vector<node_t>& clique, node_t cand,
                    const std::function<bool(std::vector<node_t>*)>& callback) {
    for (node_t node : clique) {
      if (node > cand) continue;
      if (graph_->are_neighs(cand, node)) {
        child_.push_back(node);
      }
    }
    bool go_on = callback(&child_);
    child_.clear();
    return go_on;
  }

  void CompleteFwd(std::vector<node_t>* clique) {
    CompleteSmallest(clique, clique->front());
  }

  static thread_local std::vector<bool> candidates_bitset_;
  static thread_local std::vector<node_t> candidates_;
  static thread_local std::vector<node_t> child_;
  static thread_local std::vector<bool> bad_bitset_;
  static thread_local std::vector<node_t> bad_;
  std::unique_ptr<Graph> graph_;
#ifdef DEGENERACY
  std::vector<node_t> inverse_perm;
#endif
};

template <typename Graph>
thread_local std::vector<bool> CliqueEnumeration<Graph>::candidates_bitset_;
template <typename Graph>
thread_local std::vector<typename Graph::node_t>
    CliqueEnumeration<Graph>::candidates_;
template <typename Graph>
thread_local std::vector<typename Graph::node_t>
    CliqueEnumeration<Graph>::child_;
template <typename Graph>
thread_local std::vector<bool> CliqueEnumeration<Graph>::bad_bitset_;
template <typename Graph>
thread_local std::vector<typename Graph::node_t> CliqueEnumeration<Graph>::bad_;

extern template class CliqueEnumeration<fast_graph_t<uint32_t, void>>;
extern template class CliqueEnumeration<fast_graph_t<uint64_t, void>>;

#endif  // ENUMERABLE_CLIQUE_H
