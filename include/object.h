#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "chunk.h"
#include "common.h"
#include "hash_table.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_CLASS(value) isObjType(value, OBJ_CLASS)
#define IS_INSTANCE(value) isObjType(value, OBJ_INSTANCE)
#define IS_METHOD(value) isObjType(value, OBJ_METHOD)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)

#define AS_CLASS(value) ((ObjClass*)AS_OBJ(value))
#define AS_INSTANCE(value) ((ObjInstance*)AS_OBJ(value))
#define AS_METHOD(value) ((ObjMethod*)AS_OBJ(value))
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))

#define AS_NATIVE_FUN(value) (((ObjNative*)AS_OBJ(value))->function)
#define AS_NATIVE_OBJ(value) ((ObjNative*)AS_OBJ(value))

#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (AS_STRING(value)->chars)

/// Represents the type of a heap-allocated lox object.
typedef enum ObjType {
  OBJ_CLASS,
  OBJ_INSTANCE,
  OBJ_METHOD,
  OBJ_FUNCTION,
  OBJ_CLOSURE,
  OBJ_NATIVE,
  OBJ_UPVALUE,
  OBJ_STRING,
} ObjType;

/// Represents the type of a function.
typedef enum FunctionType {
  TYPE_FUNCTION,
  TYPE_METHOD,
  TYPE_SCRIPT,
} FunctionType;

/// Base struct for heap-allocated lox object values.
typedef struct Obj {
  /// The type of this object.
  ObjType type;

  /// Whether this object is alive (as determined by the GC).
  ///
  /// Marked during GC tracing/marking phase. During GC sweep phase, all non-alive
  /// objects will be freed and this will be unset until the next GC cycle.
  bool isAlive;

  /// Pointer to the next object tracked by the VM.
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

  /// The function type of this function.
  FunctionType type;

  /// The number of arguments this function accepts.
  int arity;

  /// The number of upvalues this function references.
  int upvalueCount;

  /// Compiled chunk of instructions of this function.
  Chunk chunk;

  /// Identifier name string of this function.
  ObjString* identifier;
} ObjFunction;

/// Represents a closure around a function and the variables it accesses.
typedef struct ObjClosure {
  Obj object;

  /// Underlying function object of this closure.
  ObjFunction* function;

  /// Array of upvalues referenced by this closure.
  ObjUpvalue** upvalues;

  /// Count of upvalues this closure references.
  int upvalueCount;
} ObjClosure;

/// Represents a variable captured in a closure.
typedef struct ObjUpvalue {
  Obj object;

  /// Pointer to the underlying value of this upvalue.
  ///
  /// When this upvalue is closed, its value will be moved into [closed] and this
  /// pointer will be updated to point to [closed].
  Value* location;

  /// Pointer linking this upvalue to the next open upvalue.
  ObjUpvalue* next;

  /// The closed-over value of this upvalue.
  Value closed;
} ObjUpvalue;

/// Represents a lox class object.
typedef struct ObjClass {
  Obj object;

  /// The identifier string for this class.
  ObjString* identifier;

  /// Hash-table of methods instances of this class will have.
  Table methods;
} ObjClass;

/// Represents a method bound to an instance of a class.
typedef struct ObjMethod {
  Obj object;

  /// The class instance on which the method may be called.
  Value receiver;

  /// The method closure itself.
  ObjClosure* closure;
} ObjMethod;

/// Represents an instance of a lox class at runtime.
typedef struct ObjInstance {
  Obj object;

  /// The class this object is an instance of.
  ObjClass* classObj;

  /// The fields of this class instance.
  Table fields;
} ObjInstance;

/// Represents a pointer to a native function callable from lox code.
typedef Value (*NativeFn)(int, Value*);

/// Represents (and holds a pointer to) a native function value.
typedef struct ObjNative {
  Obj object;

  /// The number of arguments the wrapped native function accepts.
  int arity;

  /// Pointer to the native function this object wraps.
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

ObjClass* newClass(ObjString* identifier);
ObjInstance* newInstance(ObjClass* classObj);
ObjMethod* newMethod(Value receiver, ObjClosure* closure);
ObjFunction* newFunction(FunctionType type);
ObjClosure* newClosure(ObjFunction* function);
ObjUpvalue* newUpvalue(Value* slot);
ObjNative* newNative(NativeFn function, int arity);

void printObjectValue(Value value);
void printFunction(const ObjFunction* function);

#endif
