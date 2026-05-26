#include <stdio.h>
#include <string.h>

#include "headers/value.h"
#include "headers/object.h"
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
void writeValueArray(ValueArray* array, const Value value) {
  if (array->capacity < array->count + 1) {
    const int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
  }

  array->values[array->count++] = value;
  //array->count++;
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
    case VAL_OBJ: {
      const ObjString* aString = AS_STRING(a);
      const ObjString* bString = AS_STRING(b);

      // Compare full string bytes for now. If I had to guess, the book will have
      // me implement char-by-char comparison with early exit on mismatched chars?
      return aString->length == bString->length
        && memcmp(aString->chars, bString->chars, aString->length) == 0;
    };

    // Reference equality
    default: return &a == &b;
  }
}

/// Prints the given [value].
void printValue(const Value value) {
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
    case VAL_OBJ:
      printObject(value);
      break;
  }
}
