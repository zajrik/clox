#include <stdio.h>

#include "headers/value.h"
#include "headers/memory.h"

/// Initialize a [ValueArray] at address [array].
void initValueArray(ValueArray* array) {
  array->capacity = 0;
  array->count = 0;
  array->values = NULL;
}

/// Free resources used by the [ValueArray] at address [array].
///
/// The value-array will be zeroed-out for re-use.
void freeValueArray(ValueArray* array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  initValueArray(array);
}

/// Write the given [value] to the [ValueArray] at address [array].
///
/// The array will be resized and reallocated to fit as needed.
void writeValueArray(ValueArray* array, Value value) {
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
  }

  array->values[array->count++] = value;
  //array->count++;
}

void printValue(Value value) {
  switch (value.type) {
    case VAL_BOOL:
      printf(AS_BOOL(value) ? "true" : "false");
      break;
    case VAL_NIL:
      printf("nil");
      break;
    case VAL_NUMBER:
      printf("%g", AS_NUMBER(value));
      break;
  }
}
