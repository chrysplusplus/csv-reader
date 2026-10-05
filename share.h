#ifndef SHARE_H
#define SHARE_H

#include <assert.h>
#include <stdlib.h>

#define DA_INIT_CAPACITY 256

#define da_declare(T) struct { T *items; size_t count, capacity; }

#define da_destroy(da)  \
  do {                  \
    free((da)->items);  \
    (da)->items = NULL; \
    (da)->capacity = 0; \
    (da)->count = 0;    \
  } while (0)

#define da_reserve(da, exp_capacity)                                             \
  do {                                                                           \
    if ((exp_capacity) > (da)->capacity) {                                       \
      if ((da)->capacity == 0)                                                   \
        (da)->capacity = DA_INIT_CAPACITY;                                       \
      while ((exp_capacity) > (da)->capacity)                                    \
        (da)->capacity *= 2;                                                     \
      (da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items)); \
      assert("Out of memory" && (da)->items != NULL);                            \
    }                                                                            \
  } while (0)

#define da_append_n(da, elem, n)                    \
  do {                                              \
    da_reserve((da), (da)->count + (n));            \
    memcpy((da)->items + (da)->count, (elem), (n)); \
    (da)->count += (n);                             \
  } while (0)

#define da_append(da, elem)            \
  do {                                 \
    da_reserve((da), (da)->count + 1); \
    (da)->items[(da)->count] = elem;   \
    ++(da)->count;                     \
  } while (0)                          \

#define da_free(da)                          \
  do {                                       \
    void *ptr_to_free = (void *)(da)->items; \
    (da)->items = NULL;                      \
    (da)->count = 0;                         \
    (da)->capacity = 0;                      \
    free(ptr_to_free);                       \
  } while (0)

#define range_end(rg) (rg)->items + (rg)->count

#define ASSERT_UNREACHABLE assert("Unreachable" && 0)
#endif//SHARE_H
