#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (AS_STRING(value)->chars)

/// Represents the type of a heap-allocated lox object.
typedef enum ObjType {
  OBJ_STRING,
  // OBJ_INSTANCE,
  // OBJ_FUNCTION,
} ObjType;

/// Base struct for heap-allocated lox object values.
struct Obj {
  ObjType type;
  Obj* next;
};

/// Represents a lox string value.
struct ObjString {
  Obj object;
  int length;
  char* chars;
  uint32_t hash;
};

uint32_t hashString(const char* string, int length);
ObjString* copyString(const char* chars, int length);
ObjString* allocateString(char* chars, int length, uint32_t hash);
Obj* allocateObject(size_t size, ObjType type);

/// Returns whether the given lox [Value] is the given [ObjType].
static bool isObjType(const Value value, const ObjType type) {
  return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

void printObject(Value value);

#endif
