#ifndef UTIL_SERIALIZE_H
#define UTIL_SERIALIZE_H

#include <memory>
#include <vector>

#include "absl/memory/memory.h"


template <typename T>
void Serialize(const std::vector<T>& in, std::vector<size_t>* out);

template <typename T>
void Serialize(const std::shared_ptr<T>& in, std::vector<size_t>* out);

template <typename T>
void Serialize(const std::unique_ptr<T>& in, std::vector<size_t>* out);

template<typename T, typename U>
void Serialize(const std::pair<T, U>& in, std::vector<size_t>* out);

template <typename T>
typename std::enable_if<std::is_integral<T>::value>::type Serialize(
    const T& in, std::vector<size_t>* out) {
  out->push_back(in);
}

template <typename T>
void Serialize(const std::vector<T>& in, std::vector<size_t>* out) {
  out->push_back(in.size());
  for (const T& t : in) {
    Serialize(t, out);
  }
}

template <typename T>
void Serialize(const std::shared_ptr<T>& in, std::vector<size_t>* out) {
  if (in) {
    out->push_back(1);
    Serialize(*in, out);
  } else {
    out->push_back(0);
  }
}

template <typename T>
void Serialize(const std::unique_ptr<T>& in, std::vector<size_t>* out) {
  if (in) {
    out->push_back(1);
    Serialize(*in, out);
  } else {
    out->push_back(0);
  }
}

template<typename T, typename U>
void Serialize(const std::pair<T, U>& in, std::vector<size_t>* out) {
  Serialize(in.first, out);
  Serialize(in.second, out);
}

template <typename T>
void Deserialize(const size_t** in, std::vector<T>* out);

template <typename T>
void Deserialize(const size_t** in, std::shared_ptr<T>* out);

template <typename T>
void Deserialize(const size_t** in, std::unique_ptr<T>* out);

template<typename T, typename U>
void Deserialize(const size_t** in,  std::pair<T, U>* out);

template <typename T>
typename std::enable_if<std::is_integral<T>::value>::type Deserialize(
    const size_t** in, T* out) {
  // could possibly have issues with negative values - should not happen in
  // practice
  *out = **in;
  (*in)++;
}

template <typename T>
void Deserialize(const size_t** in, std::vector<T>* out) {
  out->resize(**in);
  (*in)++;
  for (T& t : *out) {
    Deserialize(in, &t);
  }
}

template <typename T>
void Deserialize(const size_t** in, std::shared_ptr<T>* out) {
  if (**in) {
    (*in)++;
    *out = std::make_shared<T>();
    Deserialize(in, &**out);
  } else {
    (*in)++;
  }
}

template <typename T>
void Deserialize(const size_t** in, std::unique_ptr<T>* out) {
  if (**in) {
    (*in)++;
    *out = absl::make_unique<T>();
    Deserialize(in, &**out);
  } else {
    (*in)++;
  }
}

template<typename T, typename U>
void Deserialize(const size_t** in,  std::pair<T, U>* out) {
  Deserialize(in, &out->first);
  Deserialize(in, &out->second);
}

template<typename T>
auto Serialize(const T& in, std::vector<size_t>* out) -> decltype(in.Serialize(out)) {
  in.Serialize(out);
}

template<typename T>
auto Deserialize(const size_t** in, T* out) -> decltype(out->Deserialize(in)) {
  out->Deserialize(in);
}

#endif  // UTIL_SERIALIZE_H
