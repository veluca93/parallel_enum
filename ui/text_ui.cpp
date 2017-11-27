#include "absl/memory/memory.h"
#include "enumerable/clique.hpp"
#include "enumerator/sequential.hpp"
#include "enumerator/parallel_tbb.hpp"
#include "enumerator/parallel_pthreads.hpp"

#include "gflags/gflags.h"

DEFINE_string(enumerator, "sequential",
			  "which enumerator should be used. Possible values: sequential, parallel");
DEFINE_int32(n, 1, "number of threads to be used for parallel execution");	//TODO: da capire come renderlo valido solo in caso di enumerator=parallel

DEFINE_string(system, "clique",
              "what should be enumerated. Possible values: cliques");
DEFINE_string(graph_format, "nde",
              "format of input graphs. Only makes sense for systems defined on "
              "graphs. Possible values: nde, oly");
DEFINE_bool(fast_graph, true,
            "use the faster but more memory hungry graph format");
DEFINE_bool(huge_graph, false, "use 64 bit integers to count nodes");
DEFINE_bool(one_based, false,
            "whether the graph is one based. Used only by oly format.");
DEFINE_bool(quiet, false, "do not show any non-fatal output");

bool ValidateEnumerator(const char* flagname, const std::string& value) {
  if (value == "sequential" || value == "parallel") {
    return true;
  }
  printf("Invalid value for --%s: %s\n", flagname, value.c_str());
  return false;
}
DEFINE_validator(enumerator, &ValidateEnumerator);



bool ValidateSystem(const char* flagname, const std::string& value) {
  if (value == "clique") {
    return true;
  }
  printf("Invalid value for --%s: %s\n", flagname, value.c_str());
  return false;
}
DEFINE_validator(system, &ValidateSystem);

bool ValidateGraphFormat(const char* flagname, const std::string& value) {
  if (value == "nde" || value == "oly") {
    return true;
  }
  printf("Invalid value for --%s: %s\n", flagname, value.c_str());
  return false;
}
DEFINE_validator(graph_format, &ValidateGraphFormat);

template <typename node_t, typename label_t>
std::unique_ptr<graph_t<node_t, label_t>> ReadGraph(
    const std::string& input_file, bool directed = false) {
  FILE* in = fopen(input_file.c_str(), "re");
  if (!in) throw std::runtime_error("Could not open " + input_file);
  if (FLAGS_graph_format == "nde") {
    return FLAGS_fast_graph ? ReadNde<node_t, fast_graph_t>(in, directed)
                            : ReadNde<node_t, graph_t>(in, directed);
  }
  if (FLAGS_graph_format == "oly") {
    return FLAGS_fast_graph
               ? ReadOlympiadsFormat<node_t, label_t, fast_graph_t>(
                     in, directed, FLAGS_one_based)
               : ReadOlympiadsFormat<node_t, label_t, graph_t>(in, directed,
                                                               FLAGS_one_based);
  }
  throw std::runtime_error("Invalid format");
}

template <typename Node, typename Item>
std::unique_ptr<Enumerator<Node, Item>> MakeEnumerator() {
  if (FLAGS_enumerator == "sequential") {
    return absl::make_unique<Sequential<Node, Item>>();
  }
  if (FLAGS_enumerator == "parallel") {
    return absl::make_unique<ParallelPthreads<Node, Item>>(FLAGS_n);
  }
  throw std::runtime_error("Invalid enumerator");
}

template <typename node_t>
int CliqueMain(const std::string& input_file) {
  auto enumerator =
      MakeEnumerator<CliqueEnumerationNode<node_t>, Clique<node_t>>();
  auto graph = ReadGraph<node_t, void>(input_file);
  enumerator->ReadDone();
  enumerator
      ->template MakeEnumerableSystemAndRun<CliqueEnumeration<node_t, void>>(
          graph.get());
  if (!FLAGS_quiet) {
    enumerator->PrintStats();
  }
  return 0;
}

int main(int argc, char** argv) {
  gflags::SetUsageMessage(
      "Enumerates the maximal elements in a set system defined by a graph "
      "or other structures.");
  gflags::SetVersionString("0.1");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_system == "clique") {
    if (argc != 2) {
      fprintf(stderr, "You should specify exactly one graph");
      return 1;
    }
    return FLAGS_huge_graph ? CliqueMain<uint64_t>(argv[1])
                            : CliqueMain<uint32_t>(argv[1]);
  }
}
