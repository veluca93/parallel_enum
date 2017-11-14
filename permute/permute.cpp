#include "permute/permute.hpp"
template std::vector<uint32_t> DegeneracyOrder(const graph_t<uint32_t, void>&);
template std::vector<uint32_t> DegeneracyOrder(
    const graph_t<uint32_t, int32_t>&);
template std::vector<uint32_t> DegeneracyOrder(
    const graph_t<uint32_t, int64_t>&);
template std::vector<uint32_t> DegeneracyOrder(
    const graph_t<uint32_t, uint32_t>&);
template std::vector<uint32_t> DegeneracyOrder(
    const graph_t<uint32_t, uint64_t>&);
template std::vector<uint64_t> DegeneracyOrder(const graph_t<uint64_t, void>&);
template std::vector<uint64_t> DegeneracyOrder(
    const graph_t<uint64_t, int32_t>&);
template std::vector<uint64_t> DegeneracyOrder(
    const graph_t<uint64_t, int64_t>&);
template std::vector<uint64_t> DegeneracyOrder(
    const graph_t<uint64_t, uint32_t>&);
template std::vector<uint64_t> DegeneracyOrder(
    const graph_t<uint64_t, uint64_t>&);
