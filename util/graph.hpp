#ifndef UTIL_GRAPH_H
#define UTIL_GRAPH_H

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/fixed_array.h"
#include "absl/memory/memory.h"
#include "absl/types/span.h"
#include "util/binary_search.hpp"
#include "util/cuckoo.hpp"
#include "util/fastio.hpp"

namespace graph_internal {

template <typename node_t, typename label_t>
class label_array_t {
 public:
  explicit label_array_t(node_t size) : data_(size) {}

  label_t& at(node_t pos) { return data_.at(pos); }
  const label_t& at(node_t pos) const { return data_.at(pos); }
  label_array_t Permute(const std::vector<node_t>& new_order) const {
    label_array_t new_labels(data_.size());
    for (unsigned i = 0; i < new_order.size(); i++) {
      new_labels.at(i) = at(new_order[i]);
    }
    return new_labels;
  }

 private:
  absl::FixedArray<label_t> data_;
};

template <typename node_t>
class label_array_t<node_t, void> {
 public:
  void at(node_t pos) const {}
  label_array_t Permute(const std::vector<node_t>& new_order) const {
    return *this;
  }
};

template <typename node_t, typename label_t>
label_array_t<node_t, label_t> ReadLabels(FILE* in, node_t num) {
  label_array_t<node_t, label_t> labels(num);
  for (node_t i = 0; i < num; i++) labels.at(i) = fastio::FastRead<label_t>(in);
  return labels;
}
template <typename node_t>
label_array_t<node_t, void> ReadLabels(FILE* in, node_t num) {
  return label_array_t<node_t, void>();
}

template <typename node_t>
std::vector<std::vector<node_t>> ReadEdgeList(FILE* in, bool directed,
                                              bool one_based, node_t nodes) {
  std::vector<std::vector<node_t>> edges(nodes);
  while (true) {
    node_t a = fastio::FastRead<node_t>(in);
    node_t b = fastio::FastRead<node_t>(in);
    if (a == EOF || b == EOF) break;
    if (a == b) continue;
    if (one_based) {
      a--;
      b--;
    }
    edges[a].push_back(b);
    if (!directed) edges[b].push_back(a);
  }
  for (std::vector<node_t>& adj : edges) {
    std::sort(adj.begin(), adj.end());
    adj.erase(std::unique(adj.begin(), adj.end()), adj.end());
  }
  return edges;
}
}  // namespace graph_internal

template <typename node_t = uint32_t, typename label_t = void>
class graph_t {
 public:
  // TODO(veluca): switch to CSR format?
  using edges_t = std::vector<std::vector<node_t>>;
  using labels_t = graph_internal::label_array_t<node_t, label_t>;
  using Builder = std::function<std::unique_ptr<graph_t>(node_t, const edges_t&,
                                                         const labels_t&)>;

  graph_t(node_t N, const edges_t& edg, const labels_t& lbl)
      : N_(N), edges_(N), labels_(lbl) {
    for (node_t i = 0; i < N; i++) {
      edges_[i].init(edg[i]);
    }
  }

  node_t size() const { return N_; }
  label_t label(node_t i) const { return labels_.at(i); }
  node_t degree(node_t i) const { return edges_[i].size(); }
  node_t fwd_degree(node_t n) const { return fwd_neighs(n).size(); }
  const binary_search_t<node_t>& neighs(node_t i) const { return edges_[i]; }

  virtual const absl::Span<node_t> fwd_neighs(node_t n) const {
    auto beg = edges_[n].upper_bound(n);
    auto end = edges_[n].end();
    return absl::Span<node_t>(beg, end - beg);
  }

  virtual bool are_neighs(node_t a, node_t b) const {
    return edges_[a].count(b);
  }

  /**
   *  Node new_order[i] will go in position i.
   */
  virtual std::unique_ptr<graph_t> Permute(
      const std::vector<node_t>& new_order) const {
    return Permute(new_order, absl::make_unique<graph_t>);
  }

  virtual ~graph_t() = default;

 protected:
  std::unique_ptr<graph_t> Permute(const std::vector<node_t>& new_order,
                                   const Builder& build) const {
    std::vector<node_t> new_pos(size(), -1);
    for (node_t i = 0; i < size(); i++) new_pos[new_order[i]] = i;
    edges_t new_edges(size());
    for (node_t i = 0; i < size(); i++) {
      for (node_t x : neighs(i)) {
        new_edges[new_pos[i]].push_back(new_pos[x]);
      }
      std::sort(new_edges[new_pos[i]].begin(), new_edges[new_pos[i]].end());
    }
    return build(size(), new_edges, labels_.Permute(new_order));
  }

  labels_t labels_;
  absl::FixedArray<binary_search_t<node_t>> edges_;
  node_t N_;
};

template <typename node_t = uint32_t, typename label_t = void>
class fast_graph_t : public graph_t<node_t, label_t> {
 public:
  using edges_t = typename graph_t<node_t, label_t>::edges_t;
  using labels_t = typename graph_t<node_t, label_t>::labels_t;
  fast_graph_t(node_t N, const edges_t& edg, const labels_t& lbl)
      : graph_t<node_t, label_t>(N, edg, lbl), edges_(N), fwd_iter_(N) {
    for (node_t i = 0; i < N; i++) {
      for (node_t x : edg[i]) edges_[i].insert(x);
      fwd_iter_[i] = neighs(i).upper_bound(i);
    }
  }

  const absl::Span<node_t> fwd_neighs(node_t n) const override {
    auto beg = fwd_iter_[n];
    auto end = neighs(n).end();
    return absl::Span<node_t>(beg, end - beg);
  }

  virtual bool are_neighs(node_t a, node_t b) const override {
    return edges_[a].count(b);
  }

  virtual std::unique_ptr<graph_t<node_t, label_t>> Permute(
      const std::vector<node_t>& new_order) const override {
    return Permute(new_order, absl::make_unique<fast_graph_t>);
  }

 private:
  absl::FixedArray<cuckoo_hash_set<node_t>> edges_;
  absl::FixedArray<typename binary_search_t<node_t>::iterator> fwd_iter_;
};

template <typename node_t = uint32_t, typename label_t = void,
          typename Graph = graph_t<node_t, label_t>>
std::unique_ptr<graph_t<node_t, label_t>> ReadOlympiadsFormat(
    FILE* in = stdin, bool directed = false, bool one_based = false) {
  node_t N = fastio::FastRead<node_t>(in);
  fastio::FastRead<node_t>(in);
  auto labels = graph_internal::ReadLabels<node_t, label_t>(in, N);
  auto edges = graph_internal::ReadEdgeList<node_t>(in, directed, one_based, N);
  return Graph(N, edges, labels);
}

#endif  // UTIL_GRAPH_H
