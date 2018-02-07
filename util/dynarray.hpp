#ifndef UTIL_DYNARRAY_H
#define UTIL_DYNARRAY_H
#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>

#include "util/serialize.hpp"

template <class T>
class dynarray {
 public:
  using value_type = T;
  using reference = T&;
  using const_reference = const T&;
  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  dynarray() : store(nullptr), count(0){};
  const dynarray operator=(const dynarray&) = delete;
  const dynarray operator=(dynarray&&) = delete;

  explicit dynarray(size_type c) : store(alloc(c)), count(c) { init(c); }

  explicit dynarray(size_type c, const value_type& v)
      : store(alloc(c)), count(c) {
    init(c, v);
  }

  dynarray(const dynarray& d) : store(alloc(d.count)), count(d.count) {
    try {
      std::uninitialized_copy(d.begin(), d.end(), begin());
    } catch (...) {
      free(store);
      throw;
    }
  }

  dynarray(dynarray&& d) noexcept : store(d.store), count(d.count) {
    d.store = nullptr;
    d.count = 0;
  }

  ~dynarray() {
    if (store == 0) return;
    for (size_type i = 0; i < count; ++i) (store + i)->~T();
    free(store);
  }

  void resize(size_type n) {
    this->~dynarray();
    store = alloc(n);
    count = n;
    init(n);
  }

  void resize(size_type n, const value_type& v) {
    this->~dynarray();
    store = alloc(n);
    count = n;
    init(n, v);
  }

  iterator begin() { return store; }
  const_iterator begin() const { return store; }
  const_iterator cbegin() const { return store; }
  iterator end() { return store + count; }
  const_iterator end() const { return store + count; }
  const_iterator cend() const { return store + count; }

  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  size_type size() const { return count; }
  size_type max_size() const { return count; }
  bool empty() const { return count == 0; }

  reference operator[](size_type n) { return store[n]; }
  const_reference operator[](size_type n) const { return store[n]; }

  reference front() { return store[0]; }
  const_reference front() const { return store[0]; }
  reference back() { return store[count - 1]; }
  const_reference back() const { return store[count - 1]; }

  const_reference at(size_type n) const {
    check(n);
    return store[n];
  }
  reference at(size_type n) {
    check(n);
    return store[n];
  }

  T* data() { return store; }
  const T* data() const { return store; }

  friend void swap(dynarray<value_type>& a, dynarray<value_type>& b) {
    std::swap(a.store, b.store);
    std::swap(a.count, b.count);
  }

  void Serialize(std::vector<size_t>* out) const {
    ::Serialize(count, out);
    for (size_t i = 0; i < count; i++) ::Serialize(store[i], out);
  }

  void Deserialize(const size_t** in) {
    ::Deserialize(in, &count);
    for (size_t i = 0; i < count; i++) ::Deserialize(in, &store[i]);
  }

 private:
  T* store{nullptr};
  size_type count{0};

  void check(size_type n) const {
    if (store == 0) throw std::out_of_range("dynarray");
    if (n >= count) throw std::out_of_range("dynarray");
  }

  T* alloc(size_type n) {
    if (n > std::numeric_limits<size_type>::max() / sizeof(T))
      throw std::out_of_range("dynarray");
    return reinterpret_cast<T*>(malloc(n * sizeof(T)));  // NOLINT
  }

  void init(size_t n) {
    size_type i = 0;
    try {
      for (size_type i = 0; i < count; ++i) new (store + i) T;
    } catch (...) {
      for (; i > 0; --i) (store + (i - 1))->~T();
      throw;
    }
  }

  void init(size_t n, const value_type& v) {
    size_type i;
    try {
      for (size_type i = 0; i < count; ++i) new (store + i) T(v);
    } catch (...) {
      for (; i > 0; --i) (store + (i - 1))->~T();
      throw;
    }
  }
};

extern template class dynarray<int32_t>;
extern template class dynarray<int64_t>;
extern template class dynarray<uint32_t>;
extern template class dynarray<uint64_t>;
#endif  // UTIL_DYNARRAY_H
