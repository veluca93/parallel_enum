#ifndef PERMUTE_PERMUTE_H
#define PERMUTE_PERMUTE_H
#include "util/graph.hpp"

template <typename node_t, typename label_t>
std::vector<node_t> DegeneracyOrder(const graph_t<node_t, label_t>& graph) {
  std::vector<node_t> perm;
  std::vector<std::vector<node_t>> d(graph.size());
  std::vector<node_t> degs(graph.size());
  std::vector<node_t> positions(graph.size());
  std::vector<bool> used(graph.size());
  std::size_t degeneracy = 0;
  for (node_t i = 0; i < graph.size(); i++) {
    d[graph.degree(i)].push_back(i);
    degs[i] = graph.degree(i);
    positions[i] = d[degs[i]].size() - 1;
  }
  std::size_t j = 0;
  for (node_t i = 0; i < graph.size(); i++) {
    while (d[j].empty()) j++;
    node_t v = d[j].back();
    d[j].pop_back();
    perm.push_back(v);
    used[v] = true;
    if (degeneracy < j) degeneracy = j;
    for (auto g : graph.neighs(v)) {
      if (used[g]) continue;
      node_t& to_swap = d[degs[g]][positions[g]];
      std::swap(to_swap, d[degs[g]].back());
      positions[to_swap] = positions[g];
      d[degs[g]].pop_back();
      degs[g]--;
      d[degs[g]].push_back(g);
      positions[g] = d[degs[g]].size() - 1;
    }
    if (j > 0) j--;
  }
  return perm;
}

template <typename node_t, typename label_t>
std::vector<node_t> RandomOrder(const graph_t<node_t, label_t>& graph) {
  std::vector<node_t> ans;
  for (size_t i = 0; i < graph.size(); i++) ans.push_back(i);
  std::random_shuffle(ans.begin(), ans.end());
  return ans;
}

extern template std::vector<uint32_t> DegeneracyOrder(
    const graph_t<uint32_t, void>&);
extern template std::vector<uint32_t> DegeneracyOrder(
    const graph_t<uint32_t, int32_t>&);
extern template std::vector<uint32_t> DegeneracyOrder(
    const graph_t<uint32_t, int64_t>&);
extern template std::vector<uint32_t> DegeneracyOrder(
    const graph_t<uint32_t, uint32_t>&);
extern template std::vector<uint32_t> DegeneracyOrder(
    const graph_t<uint32_t, uint64_t>&);
extern template std::vector<uint64_t> DegeneracyOrder(
    const graph_t<uint64_t, void>&);
extern template std::vector<uint64_t> DegeneracyOrder(
    const graph_t<uint64_t, int32_t>&);
extern template std::vector<uint64_t> DegeneracyOrder(
    const graph_t<uint64_t, int64_t>&);
extern template std::vector<uint64_t> DegeneracyOrder(
    const graph_t<uint64_t, uint32_t>&);
extern template std::vector<uint64_t> DegeneracyOrder(
    const graph_t<uint64_t, uint64_t>&);

#endif
