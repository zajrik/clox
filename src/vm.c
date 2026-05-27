#include <stdarg.h>
#include <string.h>

#include "headers/vm.h"
#include "headers/common.h"
#include "headers/compiler.h"
#include "headers/debug.h"
#include "headers/memory.h"
#include "headers/object.h"

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
  vm.objects = NULL;
}

/// Free resources used by the virtual machine. (eventually)
void freeVm() {
  freeObjects();
}

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
void push(const Value value) {
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

/// Peek at a value in the stack, offset by [distance] from the top of the stack.
static Value peek(const int distance) {
  return *(vm.stackTop - 1 - distance);
  // return vm.stackTop[-1 - distance];
}

/// Pop two lox strings off of the stack, allocate a new string, copy the two
/// strings into the new block of memory and push the new string value to the
/// stack.
static void concatenate() {
  const ObjString* b = AS_STRING(pop());
  const ObjString* a = AS_STRING(pop());
  const int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);

  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  // TODO: Free the abandoned strings!
  // https://craftinginterpreters.com/strings.html#freeing-objects

  push(OBJ_VAL(allocateString(chars, length)));
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
    for (const Value* slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }

    printf("\n");
    // ReSharper disable once CppRedundantCastExpression
    disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->instructions));
    #endif

    switch (READ_BYTE()) {
      case OP_CONSTANT:
        push(READ_CONSTANT());
        break;

      case OP_NIL:
        push(NIL_VAL);
        break;
      case OP_TRUE:
        push(BOOL_VAL(true));
        break;
      case OP_FALSE:
        push(BOOL_VAL(false));
        break;

      case OP_EQUAL: {
        const Value b = pop();
        const Value a = pop();
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }

      case OP_GREATER:
        BINARY_OP(BOOL_VAL, >);
        break;
      case OP_LESS:
        BINARY_OP(BOOL_VAL, <);
        break;

      case OP_ADD: {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          // ObjString* b = AS_STRING(pop());
          // ObjString* a = AS_STRING(pop());
          concatenate();
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          const double b = AS_NUMBER(pop());
          const double a = AS_NUMBER(pop());
          push(NUMBER_VAL(b + a));
        } else {
          runtimeError("Operands must both be numbers or strings.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SUBTRACT:
        BINARY_OP(NUMBER_VAL, -);
        break;
      case OP_MULTIPLY:
        BINARY_OP(NUMBER_VAL, *);
        break;
      case OP_DIVIDE:
        BINARY_OP(NUMBER_VAL, /);
        break;

      case OP_NOT:
        push(BOOL_VAL(isFalsey(pop())));
        break;

      case OP_NEGATE:
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number.");
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
