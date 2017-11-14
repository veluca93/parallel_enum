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

template <typename node_t, typename label_t>
class CliqueEnumeration
    : public Enumerable<CliqueEnumerationNode<node_t>, Clique<node_t>> {
 public:
  using NodeCallback = typename Enumerable<CliqueEnumerationNode<node_t>,
                                           Clique<node_t>>::NodeCallback;
  explicit CliqueEnumeration(graph_t<node_t, label_t>* graph)
      : graph_(graph->Permute(DegeneracyOrder(*graph))) {}

  void ListRoots(const NodeCallback& cb) override {
    std::pair<std::vector<node_t>, node_t> root;
    for (node_t i = 0; i < graph_->size(); i++) {
      if (graph_->degree(i) == graph_->fwd_degree(i)) {
        root.first.clear();
        root.first.push_back(i);
        root.second = i;
        CompleteFwd(&root.first);
        cb(root);
      }
    }
  }

  void ListChildren(const CliqueEnumerationNode<node_t>& clique_info,
                    const NodeCallback& cb) override {
    const std::vector<node_t>& clique = clique_info.first;
    node_t parind = clique_info.second;
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
    return node.first;
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

  bool Candidates(const std::vector<node_t>& clique, node_t parind,
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
    bool go_on = true;
    for (node_t node : candidates_) {
      if (!callback(node)) {
        go_on = false;
        break;
      }
    }
    for (node_t node : candidates_) {
      candidates_bitset_[node] = false;
    }
    return go_on;
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
  std::unique_ptr<graph_t<node_t, label_t>> graph_;
};

extern template class CliqueEnumeration<uint32_t, void>;
extern template class CliqueEnumeration<uint64_t, void>;

#endif  // ENUMERABLE_CLIQUE_H
