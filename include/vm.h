#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"
#include "hash_table.h"
#include "object.h"
#include "value.h"

#define FRAME_MAX 64
#define STACK_MAX (FRAME_MAX * UINT8_COUNT)

/// Function call frame, used to determine where on the stack a function's values
/// live and to track instruction execution within the function itself.
typedef struct CallFrame {
  /// The function being invoked.
  ObjClosure* closure;

  /// Instruction pointer, points to the next instruction in the function to be
  /// read and executed.
  uint8_t* ip;

  /// Values on the stack within this function call frame.
  Value* slots;
} CallFrame;

/// Virtual machine for interpreting lox instructions.
typedef struct Vm {
  /// Function call frame stack.
  CallFrame frames[FRAME_MAX];

  /// Height of the call frame stack.
  int frameCount;

  /// Stack of values in memory.
  Value stack[STACK_MAX];

  /// Pointer to the top of the stack, where the next item will be inserted.
  Value* stackTop;

  /// Hash-table of interned strings, used to ensure any allocated string exists
  /// only once. When a string is produced, we'll check here for it first and
  /// return a pointer to the existing interned string if it exists before allocating
  /// a new string.
  Table strings;

  /// Hash-table of all global-scoped program variables.
  Table globals;

  /// Pointer to the head of a linked list of open upvalues to be closed over and
  /// hoisted to the heap when they go out of scope.
  ///
  /// The upvalues are ordered to match their order on the stack
  ObjUpvalue* openUpvalues;

  /// Linked list of allocated lox objects.
  Obj* objects;

  /// Total bytes allocated by the VM.
  size_t bytesAllocated;

  /// Threshold of allocated bytes before the GC will run next.
  size_t nextGC;

  /// Current count of objects marked by the GC.
  int gcCount;

  /// Current capacity for objects marked by GC.
  int gcCapacity;

  /// Array of pointers to objects marked by the GC.
  Obj** gcStack;
} Vm;

/// Result of interpreting a set of instructions.
typedef enum InterpretResult {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern Vm vm;

void initVm();
void freeVm();

static void resetStack();
void push(Value value);
Value pop();

static Value peek(int distance);
static void concatenate();
static bool callValue(Value callee, int argCount);
static bool callFun(ObjClosure* closure, int argCount);
static ObjUpvalue* captureUpvalue(Value* local);

InterpretResult interpret(const char* source);
static InterpretResult run();

static void defineNative(const char* name, NativeFn function, int arity);
static Value clockNative(int argCount, Value* args);

static void runtimeError(const char* format, ...);

#endif
