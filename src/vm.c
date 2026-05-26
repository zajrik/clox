#include <stdarg.h>

#include "headers/vm.h"
#include "headers/common.h"
#include "headers/compiler.h"
#include "headers/debug.h"

/// Read the byte pointed to by the current vm instruction pointer and increment
/// the instruction pointer to point to the next byte.
#define READ_BYTE() *vm.ip++

/// Read the constant value at the offset obtained by reading the next byte.
#define READ_CONSTANT() vm.chunk->constants.values[READ_BYTE()]

/// Perform a binary operation using the top two values off the top of the stack,
/// pushing the result of the operation back onto the stack.
#define BINARY_OP(valueType, op) do { \
  if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
    runtimeError("Operands must be numbers."); \
    return INTERPRET_RUNTIME_ERROR; \
  } \
  double b = AS_NUMBER(pop()); \
  double a = AS_NUMBER(pop()); \
  push(valueType(a op b)); \
} while (false)

/// Global lox virtual machine instance.
Vm vm;

/// Initialize the virtual machine.
void initVm() {
  resetStack();
}

/// Free resources used by the virtual machine. (eventually)
void freeVm() {}

/// Reset the virtual machine stack.
///
/// Resets the pointer to the top of the stack to the address of the first value,
/// allowing it to be overwritten when the next value is pushed to the stack.
static void resetStack() {
  vm.stackTop = vm.stack;
}

/// Push [value] to the top of the vm stack.
///
/// Inserts the value into memory at the address pointed to by [vm.stackTop] and
/// increments the stack top pointer to point to the next open slot on the stack.
void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

/// Pop the top value from the vm stack.
///
/// Decrements the stack top pointer and returns the value at the decremented
/// address (points to the top value in the stack which can safely be overwritten
/// when a value is next pushed).
Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

static Value peek(const int distance) {
  return *(vm.stackTop - 1 - distance);
  // return vm.stackTop[-1 - distance];
}

/// Interpret the given lox source code text.
///
/// Code will be compiled before being run on the virtual machine.
InterpretResult interpret(const char* source) {
  Chunk chunk;
  initChunk(&chunk);

  if (!compile(source, &chunk)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm.chunk = &chunk;
  vm.ip = vm.chunk->instructions;

  const InterpretResult result = run();

  freeChunk(&chunk);
  return result;
}

/// Run the instructions from the currently loaded [Chunk].
static InterpretResult run() {
  for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
    printf("            ");

    // Print contents of stack, starting at pointer to the first slot
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }

    printf("\n");
    disassembleInstruction(vm.chunk, vm.ip - vm.chunk->instructions);
    #endif

    uint8_t instruction;

    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT:
        push(READ_CONSTANT());
        break;

      case OP_ADD:
        BINARY_OP(NUMBER_VAL, +);
        break;
      case OP_SUBTRACT:
        BINARY_OP(NUMBER_VAL, -);
        break;
      case OP_MULTIPLY:
        BINARY_OP(NUMBER_VAL, *);
        break;
      case OP_DIVIDE:
        BINARY_OP(NUMBER_VAL, /);
        break;

      case OP_NEGATE:
        if (!IS_NUMBER(peek(0))) {
          // runtimeError("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUMBER_VAL(-AS_NUMBER(pop())));
        break;

      case OP_RETURN:
        printValue(pop());
        printf("\n");
        return INTERPRET_OK;

      default:
        return INTERPRET_RUNTIME_ERROR;
    }
  }
}

static void runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  // Find the instruction that failed.
  //
  // vm.ip points to the next instruction to be read, vm.chunk->instructions
  // is the address of the first instruction. Subtracting those provides the
  // offset of the next instruction to be read
  const size_t instruction = vm.ip - vm.chunk->instructions - 1;
  const int line = vm.chunk->lines[instruction];

  fprintf(stderr, "[line %d] in script\n", line);
  resetStack();
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
