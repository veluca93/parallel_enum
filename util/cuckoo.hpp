#ifndef UTIL_CUCKOO_H
#define UTIL_CUCKOO_H
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

#include <immintrin.h>

#include "util/serialize.hpp"

template <typename T, T missing = T(-1),
#ifdef __KNC__
          int bucket_size = 64 / sizeof(T)
#else
          int bucket_size = 16 / sizeof(T)
#endif
          >
class cuckoo_hash_set {
 public:
  using value_type = T;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using difference_type = std::ptrdiff_t;
  using size_type = size_t;

  void Serialize(std::vector<size_t>* out) const {
    ::Serialize(mask, out);
    ::Serialize(sz, out);
    for (size_t i = 0; i < capacity(); i++) ::Serialize(ht[i], out);
  }

  void Deserialize(const size_t** in) {
    ::Deserialize(in, &mask);
    ::Deserialize(in, &sz);
    for (size_t i = 0; i < capacity(); i++) ::Deserialize(in, &ht[i]);
  }

 private:
  pointer ht{nullptr};
  size_t mask{0};
  size_t sz{0};

  size_t hash_1(const value_type& k) const { return k & mask; }

  size_t hash_2(const value_type& k) const { return ~k & mask; }

  void insert(value_type&& k, value_type** table) {
    int h1 = hash_1(k);
    for (int pos = 0; pos < bucket_size; pos++) {
      if ((*table)[h1 * bucket_size + pos] == missing) {
        (*table)[h1 * bucket_size + pos] = k;
        return;
      }
    }
    int h2 = hash_2(k);
    for (int pos = 0; pos < bucket_size; pos++) {
      if ((*table)[h2 * bucket_size + pos] == missing) {
        (*table)[h2 * bucket_size + pos] = k;
        return;
      }
    }
    bool use_hash_1 = true;
    for (unsigned i = 0; i < mask; i++) {
      value_type cuckooed;
      size_t hash;
      if (use_hash_1) {
        hash = hash_1(k);
      } else {
        hash = hash_2(k);
      }
      int pos = 0;
      for (; pos < bucket_size; pos++) {
        if ((*table)[hash * bucket_size + pos] == missing) break;
      }
      if (pos == bucket_size) {
        cuckooed = std::move((*table)[hash * bucket_size]);
        pos = 1;
        for (; pos < bucket_size; pos++) {
          (*table)[hash * bucket_size + pos - 1] =
              std::move((*table)[hash * bucket_size + pos]);
        }
        (*table)[hash * bucket_size + pos - 1] = k;
      } else {
        cuckooed = std::move((*table)[hash * bucket_size + pos]);
        (*table)[hash * bucket_size + pos] = k;
      }
      use_hash_1 = hash == hash_2(cuckooed);
      k = std::move(cuckooed);
      if (k == missing) return;
    }
    rehash(table);
    insert(std::move(k), table);
  }

  void rehash(value_type** table) {
    auto oldmask = mask;
    if (mask == 0)
      mask = 1;
    else
      mask = (mask << 1) | mask;
    pointer newt = 0;
    if (posix_memalign((void**)&newt, sizeof(T) * bucket_size,
                       sizeof(T) * capacity()) != 0) {
      throw std::runtime_error("posix_memalign");
    }
    std::fill(newt, newt + capacity(), missing);
    for (size_t i = 0; i < (oldmask + 1) * bucket_size; i++)
      if ((*table)[i] != missing) insert(std::move((*table)[i]), &newt);
    std::swap(*table, newt);
    free(newt);
  }

 public:
  class const_iterator {
   private:
    const cuckoo_hash_set& container;
    size_type offset;

   public:
    using value_type = cuckoo_hash_set::value_type;
    using const_reference = cuckoo_hash_set::const_reference;
    using const_pointer = cuckoo_hash_set::const_pointer;
    using difference_type = cuckoo_hash_set::difference_type;
    using iterator_category = std::forward_iterator_tag;

    const_iterator(const cuckoo_hash_set& container, size_type offset_)
        : container(container), offset(offset_) {
      while (offset != container.capacity() && container.ht[offset] == missing)
        ++offset;
    }
    bool operator==(const const_iterator& other) const {
      return &container == &other.container && offset == other.offset;
    }
    bool operator!=(const const_iterator& other) const {
      return !(*this == other);
    }

    const_iterator& operator++() {
      ++offset;
      while (offset != container.capacity() && container.ht[offset] == missing)
        ++offset;
      return *this;
    }
    const const_iterator operator++(int) {
      const_iterator tmp = *this;
      ++*this;
      return tmp;
    }

    const_reference operator*() const { return container.ht[offset]; };
  };

  using iterator = const_iterator;
  friend class const_iterator;

  cuckoo_hash_set& operator=(const cuckoo_hash_set& other) {
    free(ht);
    if (posix_memalign((void**)&ht, sizeof(T) * bucket_size,
                       sizeof(T) * other.capacity()) != 0) {
      throw std::runtime_error("posix_memalign");
    }
    memcpy(ht, other.ht, sizeof(T) * other.capacity());
    mask = other.mask;
    sz = other.sz;
    return *this;
  }

  cuckoo_hash_set(const cuckoo_hash_set& other) : ht(nullptr) {
    *this = other;
  };

  cuckoo_hash_set& operator=(cuckoo_hash_set&& other) noexcept {
    free(ht);
    mask = other.mask;
    sz = other.sz;
    ht = other.ht;
    other.mask = 0;
    other.sz = 0;
    if (posix_memalign((void**)&other.ht, sizeof(T) * bucket_size,
                       sizeof(T) * bucket_size) != 0) {
      std::terminate();
    }
    return *this;
  }

  cuckoo_hash_set(cuckoo_hash_set&& other) noexcept : ht(nullptr) {
    *this = other;
  };

  ~cuckoo_hash_set() { free(ht); }

  cuckoo_hash_set() : mask(0), sz(0) {
    if (posix_memalign((void**)&ht, sizeof(T) * bucket_size,
                       sizeof(T) * bucket_size) != 0) {
      throw std::runtime_error("posix_memalign");
    }
    std::fill(ht, ht + bucket_size, missing);
  }

  const_iterator begin() const { return const_iterator(*this, 0); }
  const_iterator end() const { return const_iterator(*this, capacity()); }

  bool operator==(const cuckoo_hash_set<T>& oth) {
    if (oth.size() != size()) return false;
    for (const auto& x : oth)
      if (!count(x)) return false;
    return true;
  }

  bool operator!=(const cuckoo_hash_set<T>& oth) { return !(*this == oth); }

  void insert(value_type k) {
    if (count(k)) {
      return;
    }
    insert(std::move(k), &ht);
    sz++;
  }
  void insert(value_type&& k) {
    if (count(k)) {
      return;
    }
    insert(std::move(k), &ht);
    sz++;
  }
  bool count(const value_type& k) const {
    int h1 = hash_1(k);
    int h2 = hash_2(k);
#ifndef __KNC__
    if (bucket_size == 4 && sizeof(T) == 4) {
      __m128i cmp = _mm_set1_epi32(k);
      __m128i b1 = _mm_load_si128((__m128i*)&ht[bucket_size * h1]);
      __m128i b2 = _mm_load_si128((__m128i*)&ht[bucket_size * h2]);
      __m128i flag =
          _mm_or_si128(_mm_cmpeq_epi32(cmp, b1), _mm_cmpeq_epi32(cmp, b2));
      return _mm_movemask_epi8(flag) != 0;
    }
#else
    if (bucket_size == 16 && sizeof(T) = 4) {
      __m512i cmp = _mm512_set1_epi32(k);
      __m512i b1 = _mm512_load_epi32(&ht[bucket_size * h1]);
      __m512i b2 = _mm512_load_epi32(&ht[bucket_size * h2]);
      return _mm512_cmpeq_epi32_mask(b1, cmp) ||
             _mm512_cmpeq_epi32_mask(b2, cmp);
    }
#endif
    bool result = false;
    for (unsigned i = 0; i < bucket_size; i++)
      result |=
          (ht[(bucket_size * h1) | i] == k || ht[(bucket_size * h2) | i] == k);
    return result;
  }
  void reserve(size_type sz) {
    if (sz <= capacity()) return;
    mask++;
    while (mask <= sz / bucket_size) mask <<= 1;
    free(ht);
    if (posix_memalign((void**)&ht, sizeof(T) * bucket_size,
                       sizeof(T) * capacity()) != 0) {
      throw std::runtime_error("posix_memalign");
    }
    std::fill(ht, ht + capacity(), missing);
    mask--;
  }
  size_type size() const { return sz; }
  size_type capacity() const { return (mask + 1) * bucket_size; }
  bool empty() const { return sz == 0; }
  void erase(const value_type& k) {
    int h1 = hash_1(k);
    for (int pos = 0; pos < bucket_size; pos++)
      if (ht[h1 * bucket_size + pos] == k) {
        ht[h1 * bucket_size + pos] = missing;
        sz--;
        return;
      }
    int h2 = hash_2(k);
    for (int pos = 0; pos < bucket_size; pos++)
      if (ht[h2 * bucket_size + pos] == k) {
        ht[h2 * bucket_size + pos] = missing;
        sz--;
        return;
      }
  }
  void clear() {
    if (ht) std::fill(ht, ht + capacity(), missing);
  }

  int front() const { return *begin(); }
};

extern template class cuckoo_hash_set<int32_t>;
extern template class cuckoo_hash_set<int64_t>;
extern template class cuckoo_hash_set<uint32_t>;
extern template class cuckoo_hash_set<uint64_t>;

#endif  // UTIL_CUCKOO_H
