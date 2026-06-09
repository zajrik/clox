#ifndef CLOX_MEMORY_H
#define CLOX_MEMORY_H

#include "common.h"
#include "value.h"

/// Allocate a block of memory to hold [count] values of [type].
#define ALLOCATE(type, count) \
  (type*)reallocate(NULL, 0, sizeof(type) * (count))

/// Increases the given capacity by a factor of 2.
#define GROW_CAPACITY(capacity) \
  ((capacity) < 8 ? 8 : (capacity) * 2)

/// Reallocate an array of type [type] at address [ptr] to increase its capacity
/// from [oldSize] to [newSize].
#define GROW_ARRAY(type, ptr, oldSize, newSize) ( \
  (type*)reallocate( \
    ptr, \
    sizeof(type) * (oldSize), \
    sizeof(type) * (newSize) \
  ) \
)

/// Free memory for an array of type [type] at address [ptr] of size [size].
#define FREE_ARRAY(type, ptr, size) \
  reallocate(ptr, sizeof(type) * (size), 0)

/// Free memory for an object of type [type] at address [ptr].
#define FREE(type, ptr) \
  reallocate(ptr, sizeof(type), 0)

void* reallocate(void* ptr, size_t oldSize, size_t newSize);
void freeObject(Obj* object);
void freeObjects();

void collectGarbage();
static void markRoots();
static void traceReferences();
static void sweep();
void markValue(Value value);
void markObject(Obj* obj);
static void markArray(const ValueArray* array);
static void visitObject(Obj* obj);

#endif
