#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"
#include "value.h"
#include "hash_table.h"

#define STACK_MAX 256

/// Virtual machine for interpreting lox instructions.
typedef struct Vm {
  /// The chunk of instruction opcodes and operands being interpreted by the vm.
  Chunk* chunk;

  /// Instruction pointer, points to the next instruction to be read.
  uint8_t* ip;

  /// The stack of values in memory.
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

  /// Linked list of allocated lox objects.
  Obj* objects;
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

InterpretResult interpret(const char* source);
static InterpretResult run();

static void runtimeError(const char* format, ...);

#endif
