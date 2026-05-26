#ifndef CLOX_MEMORY_H
#define CLOX_MEMORY_H

#include "common.h"

/// Allocate a block of memory to hold [count] values of [type].
#define ALLOCATE(type, count) \
  (type*)reallocate(NULL, 0, sizeof(type) * (count))

/// Increases the given capacity by a factor of 2.
#define GROW_CAPACITY(capacity) \
  (capacity < 8 ? 8 : capacity * 2)

/// Reallocate an array of type [type] at address [ptr] to increase its capacity
/// from [oldSize] to [newSize].
#define GROW_ARRAY(type, ptr, oldSize, newSize) ( \
  (type*)reallocate( \
    ptr, \
    sizeof(type) * oldSize, \
    sizeof(type) * newSize \
  ) \
)

/// Free the array of type [type] at address [ptr] of size [size].
#define FREE_ARRAY(type, ptr, size) \
  (reallocate(ptr, sizeof(type) * size, 0))

/// Reallocates memory at address [ptr], allocating [newSize] bytes.
///
/// If [newSize] is `0`, memory allocated at [ptr] will be freed.
///
/// Returns a pointer to the newly allocated block of memory.
static void* reallocate(void* ptr, const size_t oldSize, const size_t newSize) {
  if (newSize == 0) {
    free(ptr);
    return NULL;
  }

  void* result = realloc(ptr, newSize);
  if (result == NULL) exit(1);
  return result;
}

#endif
