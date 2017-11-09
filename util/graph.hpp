#ifndef UTIL_GRAPH_H
#define UTIL_GRAPH_H

#include <type_traits>
#include <utility>

#include "absl/container/fixed_array.h"

namespace graph_internal {

template <typename label_t>
class label_array_t {
 public:
  explicit label_array_t(std::size_t size) : data_(size) {}

  label_t& at(std::size_t pos) { return data_.at(pos); }
  const label_t& at(std::size_t pos) const { return data_.at(pos); }

 private:
  absl::FixedArray<label_t> data_;
};

template <>
class label_array_t<void> {
 public:
  explicit label_array_t(std::size_t size) {}
  void at(std::size_t pos) const {}
};
}  // namespace graph_internal

template <typename node_t = uint32_t, typename label_t = void>
class graph_t {};
#endif  // UTIL_GRAPH_H
