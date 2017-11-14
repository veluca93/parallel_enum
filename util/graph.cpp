#include "util/graph.hpp"

template std::vector<std::vector<int32_t>>
graph_internal::ReadEdgeList<int32_t>(FILE* in, bool directed, bool one_based,
                                      int32_t nodes);
template std::vector<std::vector<int64_t>>
graph_internal::ReadEdgeList<int64_t>(FILE* in, bool directed, bool one_based,
                                      int64_t nodes);
template std::vector<std::vector<uint32_t>>
graph_internal::ReadEdgeList<uint32_t>(FILE* in, bool directed, bool one_based,
                                       uint32_t nodes);
template std::vector<std::vector<uint64_t>>
graph_internal::ReadEdgeList<uint64_t>(FILE* in, bool directed, bool one_based,
                                       uint64_t nodes);
template class graph_internal::label_array_t<uint32_t, void>;
template class graph_internal::label_array_t<uint32_t, int32_t>;
template class graph_internal::label_array_t<uint32_t, int64_t>;
template class graph_internal::label_array_t<uint32_t, uint32_t>;
template class graph_internal::label_array_t<uint32_t, uint64_t>;
template class graph_internal::label_array_t<uint64_t, void>;
template class graph_internal::label_array_t<uint64_t, int32_t>;
template class graph_internal::label_array_t<uint64_t, int64_t>;
template class graph_internal::label_array_t<uint64_t, uint32_t>;
template class graph_internal::label_array_t<uint64_t, uint64_t>;
template class graph_t<uint32_t, void>;
template class graph_t<uint32_t, int32_t>;
template class graph_t<uint32_t, int64_t>;
template class graph_t<uint32_t, uint32_t>;
template class graph_t<uint32_t, uint64_t>;
template class graph_t<uint64_t, void>;
template class graph_t<uint64_t, int32_t>;
template class graph_t<uint64_t, int64_t>;
template class graph_t<uint64_t, uint32_t>;
template class graph_t<uint64_t, uint64_t>;
template class fast_graph_t<uint32_t, void>;
template class fast_graph_t<uint32_t, int32_t>;
template class fast_graph_t<uint32_t, int64_t>;
template class fast_graph_t<uint32_t, uint32_t>;
template class fast_graph_t<uint32_t, uint64_t>;
template class fast_graph_t<uint64_t, void>;
template class fast_graph_t<uint64_t, int32_t>;
template class fast_graph_t<uint64_t, int64_t>;
template class fast_graph_t<uint64_t, uint32_t>;
template class fast_graph_t<uint64_t, uint64_t>;
