#include "absl/memory/memory.h"
#include "permute/permute.hpp"
#include "util/graph.hpp"

#include <iomanip>
#include <iostream>
#include <thread>
#include "gflags/gflags.h"

DEFINE_int32(k, 2, "value of k for the k-plexes");
DEFINE_int32(q, 1, "only find diam-2 kplexes at least this big");

DEFINE_string(graph_format, "nde",
              "format of input graphs. Only makes sense for systems defined on "
              "graphs. Possible values: nde, oly");
DEFINE_bool(fast_graph, true,
            "use the faster but more memory hungry graph format");
DEFINE_bool(huge_graph, false, "use 64 bit integers to count nodes");
DEFINE_bool(one_based, false,
            "whether the graph is one based. Used only by oly format.");
DEFINE_bool(quiet, false, "do not show any non-fatal output");

bool ValidateGraphFormat(const char* flagname, const std::string& value) {
  if (value == "nde" || value == "oly") {
    return true;
  }
  printf("Invalid value for --%s: %s\n", flagname, value.c_str());
  return false;
}
DEFINE_validator(graph_format, &ValidateGraphFormat);

template <typename node_t, typename label_t>
std::unique_ptr<fast_graph_t<node_t, label_t>> ReadFastGraph(
    const std::string& input_file, bool directed = false) {
  FILE* in = fopen(input_file.c_str(), "re");
  if (!in) throw std::runtime_error("Could not open " + input_file);
  if (FLAGS_graph_format == "nde") {
    return ReadNde<node_t, fast_graph_t>(in, directed);
  }
  if (FLAGS_graph_format == "oly") {
    return FLAGS_fast_graph
               ? ReadOlympiadsFormat<node_t, label_t, fast_graph_t>(
                     in, directed, FLAGS_one_based)
               : ReadOlympiadsFormat<node_t, label_t, fast_graph_t>(
                     in, directed, FLAGS_one_based);
  }
  throw std::runtime_error("Invalid format");
}

template <typename Graph>
size_t RootSize(const Graph& graph, size_t v, size_t k, size_t q) {
  using node_t = typename Graph::node_t;
  if (graph.fwd_degree(v) + k < q) return 0;
  thread_local std::vector<bool> subgraph_added(graph.size());
  subgraph_added[v] = true;
  std::vector<node_t> ans;
  ans.push_back(v);
  std::vector<node_t> sg;
  for (node_t n : graph.fwd_neighs(v)) {
    sg.push_back(n);
    subgraph_added[n] = true;
  }
  auto filter_sg = [&](const std::vector<node_t>& sg) {
    std::vector<node_t> filtered;
    for (size_t i = 0; i < sg.size(); i++) {
      size_t count = 0;
      for (size_t j = 0; j < sg.size(); j++) {
        if (graph.are_neighs(sg[i], sg[j])) count++;
      }
      if (count + 2 * k >= q) filtered.push_back(sg[i]);
    }
    return filtered;
  };
  size_t sz;
  do {
    sz = sg.size();
    sg = filter_sg(sg);
  } while (sg.size() != sz);
  for (node_t n : sg) {
    ans.push_back(n);
  }
  if (k != 1) {
    thread_local std::vector<node_t> subgraph_candidates;
    thread_local std::vector<uint32_t> subgraph_counts(graph.size());
    for (node_t n : sg) {
      for (node_t nn : graph.neighs(n)) {
        if (nn > v && !subgraph_added[nn]) {
          if (!subgraph_counts[nn]) subgraph_candidates.push_back(nn);
          subgraph_counts[nn]++;
        }
      }
    }
    for (node_t n : subgraph_candidates) {
      if (subgraph_counts[n] + 2 * k >= q + 2) {
        ans.push_back(n);
      }
      subgraph_counts[n] = 0;
    }
    subgraph_candidates.clear();
  }
  for (node_t n : ans) subgraph_added[n] = false;
  for (node_t n : graph.fwd_neighs(v)) subgraph_added[n] = false;
  return ans.size();
}

template <typename node_t>
void Stats(const fast_graph_t<node_t>& graph, const std::string& order,
           const std::string& pad) {
  size_t tot = 0;
  size_t big = 0;
  for (size_t i = 0; i < graph.size(); i++) {
    size_t cmp = RootSize(graph, i, FLAGS_k, FLAGS_q);
    if (cmp > big) big = cmp;
    tot += cmp;
  }
  std::cout << pad << "Maximum block size for " << order << " ordering: " << big
            << std::endl;
}

template <typename node_t>
size_t TwoHopSize(const fast_graph_t<node_t>& graph, node_t v) {
  thread_local std::vector<bool> subgraph_added(graph.size());
  subgraph_added[v] = true;
  std::vector<node_t> ans;
  for (node_t n : graph.neighs(v)) {
    if (!subgraph_added[n]) {
      ans.push_back(n);
      subgraph_added[n] = true;
    }
    for (node_t nn : graph.neighs(n)) {
      if (!subgraph_added[nn]) {
        ans.push_back(nn);
        subgraph_added[nn] = true;
      }
    }
  }
  for (node_t n : ans) subgraph_added[n] = false;
  return ans.size();
}

template <typename node_t>
void TwoHopSz(const fast_graph_t<node_t>& graph) {
  size_t tot = 0;
  size_t big = 0;
  for (node_t i = 0; i < graph.size(); i++) {
    size_t cmp = TwoHopSize(graph, i);
    if (cmp > big) big = cmp;
    tot += cmp;
  }
  std::cout << "  Total two hop size: " << tot << std::endl;
  std::cout << "Maximum two hop size: " << big << std::endl;
}

template <typename node_t>
int StatsMain(const std::string& input_file) {
  auto graph = ReadFastGraph<node_t, void>(input_file);
  Stats(*graph, "natural", "   ");
  Stats(*graph->Permute(DegeneracyOrder(*graph)), "degeneracy", "");
  Stats(*graph->Permute(RandomOrder(*graph)), "random", "    ");
  //TwoHopSz(*graph);
  return 0;
}

int main(int argc, char** argv) {
  gflags::SetUsageMessage(
      "Gives some stats that show the goodness of our approach for kplexes.");
  gflags::SetVersionString("0.1");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (argc != 2) {
    fprintf(stderr, "You should specify exactly one graph\n");
    return 1;
  }
  return FLAGS_huge_graph ? StatsMain<uint64_t>(argv[1])
                          : StatsMain<uint32_t>(argv[1]);
}
