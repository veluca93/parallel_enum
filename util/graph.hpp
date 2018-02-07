#ifndef UTIL_GRAPH_H
#define UTIL_GRAPH_H

#include <functional>
#include <memory>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/span.h"
#include "util/binary_search.hpp"
#include "util/cuckoo.hpp"
#include "util/dynarray.hpp"
#include "util/fastio.hpp"
#include "util/serialize.hpp"

namespace graph_internal {

template <typename node_t, typename label_t>
class label_array_t {
 public:
  label_array_t() {}  // For deserialization
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
  void Serialize(std::vector<size_t>* out) const { ::Serialize(data_, out); }

  void Deserialize(const size_t** in) { ::Deserialize(in, &data_); }

 private:
  dynarray<label_t> data_;
};

template <typename node_t>
class label_array_t<node_t, void> {
 public:
  label_array_t() {}  // For deserialization
  explicit label_array_t(node_t size) {}
  void at(node_t pos) const {}
  label_array_t Permute(const std::vector<node_t>& new_order) const {
    return *this;
  }
  void Serialize(std::vector<size_t>* out) const {}

  void Deserialize(const size_t** in) {}
};

template <typename node_t, typename label_t>
typename std::enable_if<!std::is_void<label_t>::value,
                        label_array_t<node_t, label_t>>::type
ReadLabels(FILE* in, node_t num) {
  label_array_t<node_t, label_t> labels(num);
  for (node_t i = 0; i < num; i++) labels.at(i) = fastio::FastRead<label_t>(in);
  return labels;
}

template <typename node_t, typename label_t>
typename std::enable_if<std::is_void<label_t>::value,
                        label_array_t<node_t, label_t>>::type
ReadLabels(FILE* in, node_t num) {
  return label_array_t<node_t, void>(num);
}

template <typename node_t>
std::vector<std::vector<node_t>> ReadEdgeList(FILE* in, bool directed,
                                              bool one_based, node_t nodes) {
  std::vector<std::vector<node_t>> edges(nodes);
  while (true) {
    node_t a = fastio::FastRead<node_t>(in);
    node_t b = fastio::FastRead<node_t>(in);
    if (a == node_t(EOF) || b == node_t(EOF)) break;
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

extern template std::vector<std::vector<int32_t>> ReadEdgeList<int32_t>(
    FILE* in, bool directed, bool one_based, int32_t nodes);
extern template std::vector<std::vector<int64_t>> ReadEdgeList<int64_t>(
    FILE* in, bool directed, bool one_based, int64_t nodes);
extern template std::vector<std::vector<uint32_t>> ReadEdgeList<uint32_t>(
    FILE* in, bool directed, bool one_based, uint32_t nodes);
extern template std::vector<std::vector<uint64_t>> ReadEdgeList<uint64_t>(
    FILE* in, bool directed, bool one_based, uint64_t nodes);
extern template class label_array_t<uint32_t, void>;
extern template class label_array_t<uint32_t, int32_t>;
extern template class label_array_t<uint32_t, int64_t>;
extern template class label_array_t<uint32_t, uint32_t>;
extern template class label_array_t<uint32_t, uint64_t>;
extern template class label_array_t<uint64_t, void>;
extern template class label_array_t<uint64_t, int32_t>;
extern template class label_array_t<uint64_t, int64_t>;
extern template class label_array_t<uint64_t, uint32_t>;
extern template class label_array_t<uint64_t, uint64_t>;
}  // namespace graph_internal

template <typename node_t_ = uint32_t, typename label_t = void>
class graph_t {
 public:
  // TODO(veluca): switch to CSR format?
  using node_t = node_t_;
  using edges_t = std::vector<std::vector<node_t>>;
  using labels_t = graph_internal::label_array_t<node_t, label_t>;
  using Builder = std::function<std::unique_ptr<graph_t>(node_t, const edges_t&,
                                                         const labels_t&)>;
  graph_t() {}  // For deserialization

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

  const absl::Span<const node_t> fwd_neighs(node_t n) const {
    auto beg = edges_[n].upper_bound(n);
    auto end = edges_[n].end();
    return absl::Span<const node_t>(beg, end - beg);
  }

  bool are_neighs(node_t a, node_t b) const { return edges_[a].count(b); }

  /**
   *  Node new_order[i] will go in position i.
   */
  std::unique_ptr<graph_t> Permute(const std::vector<node_t>& new_order) const {
    std::vector<node_t> new_pos(size(), -1);
    for (node_t i = 0; i < size(); i++) new_pos[new_order[i]] = i;
    edges_t new_edges(size());
    for (node_t i = 0; i < size(); i++) {
      for (node_t x : neighs(i)) {
        new_edges[new_pos[i]].push_back(new_pos[x]);
      }
      std::sort(new_edges[new_pos[i]].begin(), new_edges[new_pos[i]].end());
    }
    return absl::make_unique<graph_t>(size(), new_edges,
                                      labels_.Permute(new_order));
  }

  std::unique_ptr<graph_t> Clone() const {
    std::vector<node_t> identity(size());
    std::iota(identity.begin(), identity.end(), 0);
    return Permute(identity);
  }

  graph_t(const graph_t&) = delete;
  graph_t(graph_t&&) noexcept = default;
  graph_t& operator=(const graph_t&) = delete;
  graph_t& operator=(graph_t&&) = delete;
  virtual ~graph_t() = default;

  void Serialize(std::vector<size_t>* out) const {
    ::Serialize(N_, out);
    ::Serialize(edges_, out);
    ::Serialize(labels_, out);
  }

  void Deserialize(const size_t** in) {
    ::Deserialize(in, &N_);
    ::Deserialize(in, &edges_);
    ::Deserialize(in, &labels_);
  }

 protected:
  node_t N_;
  std::vector<binary_search_t<node_t>> edges_;
  labels_t labels_;
};

template <typename node_t_ = uint32_t, typename label_t = void>
class fast_graph_t : public graph_t<node_t_, label_t> {
  using base_ = graph_t<node_t_, label_t>;

 public:
  using node_t = node_t_;
  using edges_t = typename base_::edges_t;
  using labels_t = typename base_::labels_t;
  fast_graph_t() {}  // For deserialization
  fast_graph_t(node_t N, const edges_t& edg, const labels_t& lbl)
      : base_(N, edg, lbl), edges_(N), fwd_iter_(N) {
    for (node_t i = 0; i < N; i++) {
      for (node_t x : edg[i]) edges_[i].insert(x);
      fwd_iter_[i] = base_::neighs(i).upper_bound(i) - base_::neighs(i).begin();
    }
  }

  const absl::Span<const node_t> fwd_neighs(node_t n) const {
    auto beg = base_::neighs(n).begin() + fwd_iter_[n];
    auto end = base_::neighs(n).end();
    return absl::Span<const node_t>(beg, end - beg);
  }

  bool are_neighs(node_t a, node_t b) const { return edges_[a].count(b); }

  std::unique_ptr<fast_graph_t> Permute(
      const std::vector<node_t>& new_order) const {
    std::vector<node_t> new_pos(base_::size(), -1);
    for (node_t i = 0; i < base_::size(); i++) new_pos[new_order[i]] = i;
    edges_t new_edges(base_::size());
    for (node_t i = 0; i < base_::size(); i++) {
      for (node_t x : base_::neighs(i)) {
        new_edges[new_pos[i]].push_back(new_pos[x]);
      }
      std::sort(new_edges[new_pos[i]].begin(), new_edges[new_pos[i]].end());
    }
    return absl::make_unique<fast_graph_t>(base_::size(), new_edges,
                                           base_::labels_.Permute(new_order));
  }

  std::unique_ptr<fast_graph_t> Clone() const {
    std::vector<node_t> identity(base_::size());
    std::iota(identity.begin(), identity.end(), 0);
    return Permute(identity);
  }
  void Serialize(std::vector<size_t>* out) const {
    base_::Serialize(out);
    ::Serialize(edges_, out);
    ::Serialize(fwd_iter_, out);
  }

  void Deserialize(const size_t** in) {
    base_::Deserialize(in);
    ::Deserialize(in, &edges_);
    ::Deserialize(in, &fwd_iter_);
  }

 private:
  dynarray<cuckoo_hash_set<node_t>> edges_;
  dynarray<size_t> fwd_iter_;
};

template <typename node_t = uint32_t, typename label_t = void,
          template <typename, typename> class Graph = fast_graph_t>
std::unique_ptr<Graph<node_t, label_t>> ReadOlympiadsFormat(
    FILE* in = stdin, bool directed = false, bool one_based = false) {
  node_t N = fastio::FastRead<node_t>(in);
  fastio::FastRead<node_t>(in);
  auto labels = graph_internal::ReadLabels<node_t, label_t>(in, N);
  auto edges = graph_internal::ReadEdgeList<node_t>(in, directed, one_based, N);
  return absl::make_unique<Graph<node_t, label_t>>(N, edges, labels);
}

template <typename node_t = uint32_t,
          template <typename, typename> class Graph = fast_graph_t>
std::unique_ptr<Graph<node_t, void>> ReadNde(FILE* in = stdin,
                                             bool directed = false) {
  node_t N = fastio::FastRead<node_t>(in);
  for (node_t i = 0; i < N; i++) {
    // Discard degree information
    fastio::FastRead<node_t>(in);
    fastio::FastRead<node_t>(in);
  }
  auto labels = graph_internal::ReadLabels<node_t, void>(in, N);
  auto edges = graph_internal::ReadEdgeList<node_t>(in, directed,
                                                    /* one_based = */ false, N);
  return absl::make_unique<Graph<node_t, void>>(N, edges, labels);
}

extern template class graph_t<uint32_t, void>;
extern template class graph_t<uint32_t, int32_t>;
extern template class graph_t<uint32_t, int64_t>;
extern template class graph_t<uint32_t, uint32_t>;
extern template class graph_t<uint32_t, uint64_t>;
extern template class graph_t<uint64_t, void>;
extern template class graph_t<uint64_t, int32_t>;
extern template class graph_t<uint64_t, int64_t>;
extern template class graph_t<uint64_t, uint32_t>;
extern template class graph_t<uint64_t, uint64_t>;
extern template class fast_graph_t<uint32_t, void>;
extern template class fast_graph_t<uint32_t, int32_t>;
extern template class fast_graph_t<uint32_t, int64_t>;
extern template class fast_graph_t<uint32_t, uint32_t>;
extern template class fast_graph_t<uint32_t, uint64_t>;
extern template class fast_graph_t<uint64_t, void>;
extern template class fast_graph_t<uint64_t, int32_t>;
extern template class fast_graph_t<uint64_t, int64_t>;
extern template class fast_graph_t<uint64_t, uint32_t>;
extern template class fast_graph_t<uint64_t, uint64_t>;

#endif  // UTIL_GRAPH_H
