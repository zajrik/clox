#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "chunk.h"
#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)

#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))
#define AS_NATIVE_FUN(value) (((ObjNative*)AS_OBJ(value))->function)
#define AS_NATIVE_OBJ(value) ((ObjNative*)AS_OBJ(value))

#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (AS_STRING(value)->chars)

/// Represents the type of a heap-allocated lox object.
typedef enum ObjType {
  OBJ_FUNCTION,
  OBJ_CLOSURE,
  OBJ_NATIVE,
  OBJ_UPVALUE,
  OBJ_STRING,
} ObjType;

typedef enum FunctionType {
  TYPE_FUNCTION,
  TYPE_SCRIPT,
} FunctionType;

/// Base struct for heap-allocated lox object values.
typedef struct Obj {
  ObjType type;
  Obj* next;
} Obj;

/// Represents a lox string value.
typedef struct ObjString {
  Obj object;
  int length;
  char* chars;
  uint32_t hash;
} ObjString;

/// Represents a lox function value.
typedef struct ObjFunction {
  Obj object;
  FunctionType type;
  int arity;
  int upvalueCount;
  Chunk chunk;
  ObjString* identifier;
} ObjFunction;

/// Represents a closure around a function and the variables it accesses.
typedef struct ObjClosure {
  Obj object;
  ObjFunction* function;
  ObjUpvalue** upvalues;
  int upvalueCount;
} ObjClosure;

/// Represents a variable captured in a closure.
typedef struct ObjUpvalue {
  Obj object;
  Value* location;
  ObjUpvalue* next;

  /// The closed-over value of this upvalue.
  Value closed;
} ObjUpvalue;

/// Represents a pointer to a native function callable from lox code.
typedef Value (*NativeFn)(int, Value*);

/// Represents (and holds a pointer to) a native function value.
typedef struct ObjNative {
  Obj object;
  int arity;
  NativeFn function;
} ObjNative;

/// Returns whether the given lox [Value] is the given [ObjType].
static bool isObjType(const Value value, const ObjType type) {
  return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

uint32_t hashString(const char* string, int length);
static Obj* allocateObject(size_t size, ObjType type);
static ObjString* allocateString(char* chars, int length, uint32_t hash);
ObjString* copyString(const char* chars, int length);
ObjString* takeString(char* chars, int length);

ObjFunction* newFunction(FunctionType type);
ObjClosure* newClosure(ObjFunction* function);
ObjUpvalue* newUpvalue(Value* slot);
ObjNative* newNative(NativeFn function, int arity);

void printObject(Value value);
void printFunction(const ObjFunction* function);

#endif
