#ifndef UTIL_BITSET_HPP
#define UTIL_BITSET_HPP
#include <array>
#include <iostream>
#include <vector>

template <size_t SZ>
class bitset {
 public:
  bitset() { static_assert(sizeof(uint64_t) == 8, "WTF?"); }

  static const constexpr size_t max() { return SZ * bits_per_item; }

  void clear() {
    for (size_t i = 0; i < SZ; i++) {
      data_[i] = 0;
    }
  }

  void set(size_t i) {
    data_[i / bits_per_item] |= 1ULL << (i % bits_per_item);
  }

  void reset(size_t i) {
    data_[i / bits_per_item] &= ~(1ULL << (i % bits_per_item));
  }

  bool get(size_t i) const {
    return data_[i / bits_per_item] & (1ULL << (i % bits_per_item));
  }

  size_t count() const {
    size_t sum = 0;
    for (size_t i = 0; i < SZ; i++) sum += __builtin_popcountll(data_[i]);
    return sum;
  }

  size_t size() const { return count(); }

  bool empty() const {
    size_t d = 0;
    for (size_t i = 0; i < SZ; i++) d |= data_[i];
    return !d;
  }

  void list(std::vector<size_t>* out, size_t limit = 0) const {
    size_t cnt = 0;
    for (size_t i = 0; i < SZ; i++) {
      uint64_t curr = data_[i];
      while (curr != 0) {
        size_t off = __builtin_ctzll(curr);
        out->push_back(i * bits_per_item + off);
        cnt++;
        if (cnt == limit) return;
        curr &= ~(1ULL << off);
      }
    }
  }

  /*
  size_t front() const {
    for (size_t i = 0; i < SZ; i++) {
      uint64_t curr = data_[i];
      if (curr != 0) {
        size_t off = __builtin_ctzll(curr);
        return i * bits_per_item + off;
      }
    }
  }

  size_t back() const {
    for (size_t i = SZ; i > 0; i--) {
      uint64_t curr = data_[i - 1];
      if (curr != 0) {
        size_t off = bits_per_item - 1 - __builtin_clzll(curr);
        return (i - 1) * bits_per_item + off;
      }
    }
  }*/

  bitset& operator|(const bitset& other) const {
    bitset ans;
    for (size_t i = 0; i < SZ; i++) ans.data_[i] = data_[i] | other.data_[i];
    return ans;
  }

  bitset& operator&(const bitset& other) const {
    bitset ans;
    for (size_t i = 0; i < SZ; i++) ans.data_[i] = data_[i] & other.data_[i];
    return ans;
  }

  bitset& operator-(const bitset& other) const {
    bitset ans;
    for (size_t i = 0; i < SZ; i++) ans.data_[i] = data_[i] & ~other.data_[i];
    return ans;
  }

  bitset& operator|=(const bitset& other) {
    for (size_t i = 0; i < SZ; i++) data_[i] |= other.data_[i];
    return *this;
  }

  bitset& operator&=(const bitset& other) {
    for (size_t i = 0; i < SZ; i++) data_[i] &= other.data_[i];
    return *this;
  }

  bitset& operator-=(const bitset& other) {
    for (size_t i = 0; i < SZ; i++) data_[i] &= ~other.data_[i];
    return *this;
  }

 private:
  static const constexpr size_t bits_per_item = 64;
  std::array<uint64_t, SZ> data_;
};

#endif
