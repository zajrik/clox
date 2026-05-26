#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"
#include "value.h"

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
} Vm;

/// Result of interpreting a set of instructions.
typedef enum InterpretResult {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void initVm();
void freeVm();

static void resetStack();
void push(Value value);
Value pop();

static Value peek(int distance);
static bool isFalsey(Value value);
static bool valuesEqual(Value a, Value b);

InterpretResult interpret(const char* source);
static InterpretResult run();

static void runtimeError(const char* format, ...);

#endif
