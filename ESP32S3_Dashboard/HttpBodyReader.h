#pragma once
#include <stddef.h>
#include <stdint.h>
// Injectable byte source: returns 0..255, -1 EOF, -2 deadline/cancellation.
// Handles fragmented/chunked responses without ever growing the allocation.
template <class Reader>
bool readHttpBody(Reader read, int64_t declared, bool chunked, char *out,
                  size_t capacity, size_t &used) {
  used = 0;
  if (!capacity || declared >= int64_t(capacity))
    return false;
  auto byte = [&]() { return read(); };
  if (chunked) {
    for (;;) {
      char line[96];
      size_t n = 0;
      int c;
      while ((c = byte()) >= 0 && c != '\n') {
        if (n + 1 >= sizeof(line))
          return false;
        line[n++] = char(c);
      }
      if (c < 0 || !n || line[n - 1] != '\r')
        return false;
      line[--n] = 0;
      uint64_t size = 0;
      size_t i = 0;
      for (; i < n && line[i] != ';'; i++) {
        int digit = line[i] >= '0' && line[i] <= '9'   ? line[i] - '0'
                    : line[i] >= 'a' && line[i] <= 'f' ? line[i] - 'a' + 10
                    : line[i] >= 'A' && line[i] <= 'F' ? line[i] - 'A' + 10
                                                       : -1;
        if (digit < 0 || size > (capacity - 1) / 16)
          return false;
        size = size * 16 + digit;
      }
      if (!i || size >= capacity - used)
        return false;
      if (!size) {
        size_t trailerBytes = 0;
        for (;;) {
          size_t length = 0;
          int last = -1;
          while ((c = byte()) >= 0 && c != '\n') {
            if (++trailerBytes > 2048)
              return false;
            ++length;
            last = c;
          }
          if (c < 0 || last != '\r' || ++trailerBytes > 2048)
            return false;
          if (length == 1)
            break;
        }
        out[used] = 0;
        return true;
      }
      for (uint64_t j = 0; j < size; j++) {
        c = byte();
        if (c < 0)
          return false;
        out[used++] = char(c);
      }
      if (byte() != '\r' || byte() != '\n')
        return false;
    }
  }
  if (declared >= 0) {
    for (int64_t i = 0; i < declared; i++) {
      int c = byte();
      if (c < 0)
        return false;
      out[used++] = char(c);
    }
    out[used] = 0;
    return true;
  }
  for (;;) {
    int c = byte();
    if (c == -1) {
      out[used] = 0;
      return true;
    }
    if (c < 0 || used + 1 >= capacity)
      return false;
    out[used++] = char(c);
  }
}
