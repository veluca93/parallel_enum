#ifndef UTIL_BINARY_SEARCH_H
#define UTIL_BINARY_SEARCH_H
#include <vector>

#include "absl/container/fixed_array.h"

template <typename T = uint32_t>
class binary_search_t {
 private:
  absl::FixedArray<T> support;

 public:
  using data_type = absl::FixedArray<T>&;
  using iterator = const T*;

  void init(const std::vector<T>& v) {
    support = absl::FixedArray<T>(v.size());
    unsigned cnt = 0;
    while (cnt != v.size()) {
      support[cnt] = v[cnt];
      cnt++;
    }
  }

  iterator begin() const { return support.begin(); }

  iterator it_at(size_t p) const { return begin() + p; }

  iterator end() const { return support.end(); }

  size_t size() const { return support.size(); }

  T get_at(size_t idx) const { return support[idx]; }

  bool count(T v) const {
    int64_t n = support.size();
    int64_t cur = 0;
    while (n > 1) {
      const int64_t half = n / 2;
      cur = support[cur + half] < v ? cur + half : half;
      n -= half;
    }
    cur += support[cur] < v;
    return cur < support.size() && support[cur] == v;
  }

  iterator lower_bound(T v) const {
    return std::lower_bound(support.begin(), support.end(), v);
  }

  iterator upper_bound(T v) const {
    return std::upper_bound(support.begin(), support.end(), v);
  }

  data_type data() { return support; }
};
#endif  // UTIL_BINARY_SEARCH_H
