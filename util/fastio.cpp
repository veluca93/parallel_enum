#include "util/fastio.hpp"

namespace fastio {

int64_t ReadInt(FILE* in) {
  int64_t n = 0;
  int64_t ch = getc_unlocked(in);
  while (ch != EOF && (ch < '0' || ch > '9')) ch = getc_unlocked(in);
  if (ch == EOF) return EOF;
  while (ch >= '0' && ch <= '9') {
    n = 10 * n + ch - '0';
    ch = getc_unlocked(in);
  }
  return n;
}

}  // namespace fastio
