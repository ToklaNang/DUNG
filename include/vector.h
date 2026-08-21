#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>
#include <stdint.h>

#define VECTOR_EMPTY       {NULL, 0, 0}
#define vector_clear(_vec) (_vec.len = 0)

#define vector(type)  \
struct {              \
  type     *items;    \
  uint64_t  len;      \
  uint64_t  cap;      \
}

#define push_back(_vec, _data)       \
do {                                 \
  if (_vec.len >= _vec.cap) {        \
    _vec.cap = (_vec.cap == 0)       \
    ? _vec.cap = 128                 \
    : _vec.cap * 2;                  \
                                     \
    void *__tmp  = realloc(          \
      _vec.items,                    \
      sizeof(*_vec.items) * _vec.cap \
    );                               \
                                     \
    if (__tmp == NULL)               \
      break;                         \
                                     \
    _vec.items = __tmp;              \
  }                                  \
                                     \
  _vec.items[_vec.len++] = _data;    \
} while (0)

#define pop_back(_vec) \
do {                   \
  if (_vec.len > 0)    \
    _vec.len--;        \
} while (0)

#define pop_index(_vec, _n)             \
do {                                    \
  for (int n = _n; n < _vec.len; n++) { \
    _vec.items[n] = _vec.items[n + 1];  \
  }                                     \
  _vec.len--;                           \
} while (0)

#define vector_destroy(_vec)      \
do {                              \
  free(_vec.items);               \
  _vec.items = NULL;              \
  _vec.len   = 0;                 \
  _vec.cap   = 0;                 \
} while (0)

#define at(_vec, _n)                        \
({                                          \
  int __index = _n;                         \
  if ((_n) < 0)                             \
    __index = ((_n) + _vec.len) % _vec.len; \
  else if ((_n) >= _vec.len)                \
    __index = (_n) % _vec.len;              \
                                            \
  _vec.items[__index];                      \
})

#define ref(_vec, _n)                       \
({                                          \
  int __index = _n;                         \
  if ((_n) < 0)                             \
    __index = ((_n) + _vec.len) % _vec.len; \
  else if ((_n) >= _vec.len)                \
    __index = (_n) % _vec.len;              \
                                            \
  &_vec.items[__index];                     \
})

#endif