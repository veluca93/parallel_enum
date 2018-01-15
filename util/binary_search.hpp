#ifndef UTIL_BINARY_SEARCH_H
#define UTIL_BINARY_SEARCH_H
#include <cstdint>
#include <vector>

#include "util/dynarray.hpp"

template <typename T = uint32_t>
class binary_search_t {
 private:
  dynarray<T> support;

 public:
  using data_type = dynarray<T>&;
  using iterator = const T*;

  void init(const std::vector<T>& v) {
    support.resize(v.size());
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

  T operator[](size_t idx) const { return get_at(idx); }

  bool count(T v) const {
    size_t n = support.size();
    size_t cur = 0;
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

extern template class binary_search_t<int32_t>;
extern template class binary_search_t<int64_t>;
extern template class binary_search_t<uint32_t>;
extern template class binary_search_t<uint64_t>;

#endif  // UTIL_BINARY_SEARCH_H
