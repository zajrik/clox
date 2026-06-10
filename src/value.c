#include <stdio.h>

#include "value.h"
#include "memory.h"
#include "object.h"

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
void writeValueArray(ValueArray* array, const Value value) {
  if (array->capacity < array->count + 1) {
    const int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
  }

  array->values[array->count++] = value;
}

/// Returns whether the given [value] is falsey.
///
/// `nil` and `false` are falsey, everything else is truthy.
bool isFalsey(const Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

/// Returns whether the given lox values are equal.
bool valuesEqual(const Value a, const Value b) {
  if (a.type != b.type) return false;
  switch (a.type) {
    case VAL_NIL: return true;
    case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
    case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
    case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
    default: return false;
  }
}

/// Prints the given [value].
void printValue(const Value value) {
  switch (value.type) {
    case VAL_BOOL: DO(printf(AS_BOOL(value) ? "true" : "false"));
    case VAL_NIL: DO(printf("nil"));
    case VAL_NUMBER: DO(printf("%g", AS_NUMBER(value)));
    case VAL_OBJ: DO(printObjectValue(value));
  }
}
