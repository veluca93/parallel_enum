#ifndef UTIL_FASTIO_H
#define UTIL_FASTIO_H
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace fastio {

int64_t ReadInt(FILE* in = stdin);

template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type FastRead(
    FILE* in) {
  return ReadInt(in);
}

}  // namespace fastio

#endif  // UTIL_FASTIO_H
