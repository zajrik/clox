#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

#include "common.h"

/// Type tags of possible lox value types.
typedef enum ValueType {
  VAL_NIL,
  VAL_BOOL,
  VAL_NUMBER,
} ValueType;

/// Union of possible lox value types.
typedef union {
  bool boolean;
  double number;
} ValueUnion;

/// A lox value, implemented as a tagged union of [ValueUnion] types.
typedef struct Value {
  ValueType type;
  ValueUnion as;
} Value;

#define IS_BOOL(value) value.type == VAL_BOOL
#define IS_NIL(value) value.type == VAL_NIL
#define IS_NUMBER(value) value.type == VAL_NUMBER

#define AS_BOOL(value) value.as.boolean
#define AS_NUMBER(value) value.as.number

#define BOOL_VAL(value) (Value){VAL_BOOL, {.boolean = value}}
#define NIL_VAL (Value){VAL_NIL, {.number = 0}}
#define NUMBER_VAL(value) (Value){VAL_NUMBER, {.number = value}}

/// Dynamically-sized array of lox [Value]s.
typedef struct ValueArray {
  /// The current maximum capacity of the value array.
  int capacity;

  /// The current number of items in the value array.
  int count;

  /// Pointer to the first value of the values array.
  Value* values;
} ValueArray;

void initValueArray(ValueArray* array);
void freeValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);

void printValue(Value value);

#endif
