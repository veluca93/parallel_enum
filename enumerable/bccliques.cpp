#include "enumerable/bccliques.hpp"

template class BCCliqueEnumeration<
    product_graph_t<uint32_t, uint32_t, fast_graph_t>>;
template class BCCliqueEnumeration<
    product_graph_t<uint64_t, uint32_t, fast_graph_t>>;
