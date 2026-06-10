#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "vm.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"

/// Global lox virtual machine instance.
Vm vm;

/// Initialize the virtual machine.
void initVm() {
  resetStack();
  vm.objects = NULL;

  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;

  vm.gcCount = 0;
  vm.gcCapacity = 0;
  vm.gcStack = NULL;

  initTable(&vm.strings);
  initTable(&vm.globals);

  defineNative("clock", clockNative, 0);
}

/// Free resources used by the virtual machine.
void freeVm() {
  freeTable(&vm.strings);
  freeTable(&vm.globals);
  freeObjects();
}

/// Reset the virtual machine stack.
///
/// Resets the pointer to the top of the stack to the address of the first value,
/// allowing it to be overwritten when the next value is pushed to the stack.
static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
}

/// Push [value] to the top of the vm stack.
///
/// Inserts the value into memory at the address pointed to by [vm.stackTop] and
/// increments the stack top pointer to point to the next open slot on the stack.
void push(const Value value) {
  *vm.stackTop++ = value;
}

/// Pop the top value from the vm stack.
///
/// Decrements the stack top pointer and returns the value at the decremented
/// address (points to the top value in the stack which can safely be overwritten
/// when a value is next pushed).
Value pop() {
  return *--vm.stackTop;
}

/// Pop the top [n] values off of the stack, discarding them.
void popN(const uint8_t n) {
  vm.stackTop -= n;
}

/// Hoist a value and any upvalues above it on the stack off of the stack and onto
/// the heap.
static void closeUpvalues(const Value* last) {
  while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
    ObjUpvalue* upvalue = vm.openUpvalues;

    // Close the open upvalue with a copy of its value from the stack, then set
    // the upvalue location to point to the copy, allowing the closed-over value
    // to be accessed and modified within the upvalue on the heap
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;

    // Drop the now-closed upvalue from the open upvalues list
    vm.openUpvalues = upvalue->next;
  }
}

// void hoistN(const uint8_t n) {}

/// Peek at a value in the stack, offset by [distance] from the top of the stack.
static Value peek(const int distance) {
  return *(vm.stackTop - 1 - distance);
}

/// Pop two lox strings off of the stack, allocate a new string, copy the two
/// strings into the new block of memory and push the new string value to the
/// stack.
static void concatenate() {
  const ObjString* b = AS_STRING(peek(0));
  const ObjString* a = AS_STRING(peek(1));
  const int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);

  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  popN(2);

  push(OBJ_VAL(takeString(chars, length)));
}

/// Attempt to call the given [callee] as a function using the top [argCount] values
/// from the stack as its arguments.
///
/// Emits a runtime error if [callee] is not a callable function, method, or class
/// initializer.
static bool callValue(const Value callee, const int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_CLASS: {
        ObjClass* classObj = AS_CLASS(callee);
        vm.stackTop[-1 - argCount] = OBJ_VAL(newInstance(classObj));
        return true;
      }

      case OBJ_CLOSURE: return callFun(AS_CLOSURE(callee), argCount);

      case OBJ_NATIVE: {
        const ObjNative* nativeObj = AS_NATIVE_OBJ(callee);

        if (argCount != nativeObj->arity) {
          runtimeError("Expected %d arguments but got %d.", nativeObj->arity, argCount);
          return false;
        }

        const NativeFn nativeFun = nativeObj->function;
        const Value result = nativeFun(argCount, vm.stackTop - argCount);
        vm.stackTop -= argCount + 1;
        push(result);
        return true;
      }

      default: break;
    }
  }
  runtimeError("Can only call functions, methods, and class initializers.");
  return false;
}

/// Call the given function [closure] using the top [argCount] stack values as
/// arguments.
static bool callFun(ObjClosure* closure, const int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
    return false;
  }

  if (vm.frameCount == FRAME_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  // Initialize call frame for the given function closure
  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.instructions;
  frame->slots = vm.stackTop - 1 - argCount;

  return true;
}

/// Capture an upvalue from the given [local] value pointer.
///
/// Attempts to find an existing upvalue for the given local before creating one.
static ObjUpvalue* captureUpvalue(Value* local) {
  // Start at the head of the open upvalues list
  ObjUpvalue* upvalue = NULL;
  ObjUpvalue* nextUpvalue = vm.openUpvalues;

  // Walk the open upvalues list, exiting if we hit the end of the list, or after
  // we encounter an upvalue with a slot past the given local
  while (nextUpvalue != NULL && nextUpvalue->location > local) {
    upvalue = nextUpvalue;
    nextUpvalue = nextUpvalue->next;
  }

  // If we find an upvalue matching the given local, return it
  if (nextUpvalue != NULL && nextUpvalue->location == local) {
    return nextUpvalue;
  }

  // Otherwise create a new upvalue and insert it into the open upvalues list, sorted
  // relative to its position on the stack
  ObjUpvalue* createdUpvalue = newUpvalue(local);
  createdUpvalue->next = nextUpvalue;

  if (upvalue == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    upvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

/// Interpret the given lox source code text.
///
/// Code will be compiled and then run on the virtual machine.
InterpretResult interpret(const char* source) {
  // Compile script to root function object
  ObjFunction* function = compile(source);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

  // Push root function to slot 0 on the stack to keep it safe from GC while we
  // wrap it in a closure, with which we then replace the function in slot 0
  push(OBJ_VAL(function));
  ObjClosure* closure = newClosure(function);
  pop();
  push(OBJ_VAL(closure));

  // Initialize the call frame for the root function
  callFun(closure, 0);

  return run();
}

/// Run the instructions from the currently loaded [Chunk].
static InterpretResult run() {
  CallFrame* frame = &vm.frames[vm.frameCount - 1];

  /// Read the byte pointed to by the current vm instruction pointer and increment
  /// the instruction pointer to point to the next byte.
  #define READ_BYTE() (*frame->ip++)

  /// Read the next two bytes at the current instruction pointer as a single 16-bit
  /// value.
  #define READ_SHORT() (frame->ip += 2, (uint16_t)(frame->ip[-2] << 8 | frame->ip[-1]))

  /// Read the constant value at the offset obtained by reading the next byte.
  #define READ_CONSTANT() frame->closure->function->chunk.constants.values[READ_BYTE()]

  /// Read the string constant value at the offset obtained by reading the next byte.
  #define READ_STRING() AS_STRING(READ_CONSTANT())

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

  loop {
    #ifdef DEBUG_TRACE_EXECUTION
    printf("            ");

    // Print contents of stack, starting at pointer to the first slot
    for (const Value* slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }

    printf("\n");

    disassembleInstruction(
      &frame->closure->function->chunk,
      (int)(frame->ip - frame->closure->function->chunk.instructions)
    );
    fflush(stdout);
    #endif

    switch (READ_BYTE()) {
      case OP_CONSTANT: DO(push(READ_CONSTANT()));
      case OP_NIL: DO(push(NIL_VAL));
      case OP_TRUE: DO(push(BOOL_VAL(true)));
      case OP_FALSE: DO(push(BOOL_VAL(false)));

      case OP_EQUAL: {
        const Value b = pop();
        const Value a = pop();
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }

      case OP_GREATER: DO(BINARY_OP(BOOL_VAL, >));
      case OP_LESS: DO(BINARY_OP(BOOL_VAL, <));

      case OP_ADD: {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
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
      case OP_SUBTRACT: DO(BINARY_OP(NUMBER_VAL, -));
      case OP_MULTIPLY: DO(BINARY_OP(NUMBER_VAL, *));
      case OP_DIVIDE: DO(BINARY_OP(NUMBER_VAL, /));
      case OP_NOT: DO(push(BOOL_VAL(isFalsey(pop()))));

      case OP_NEGATE:
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUMBER_VAL(-AS_NUMBER(pop())));
        break;

      case OP_COPY: DO(push(peek(0)));
      case OP_POP: DO(pop());
      case OP_POP_N: DO(popN(READ_BYTE()));

      case OP_CLOSE_UPVALUE: DO(closeUpvalues(vm.stackTop - 1), pop());

      // Global variable declaration statement expression op. Assigns the value at
      // the top of the stack to the global variable with the identifier string
      // obtained from the operand. The value will be popped off of the stack after
      // the value has been assigned to the global
      case OP_DEFINE_GLOBAL: {
        ObjString* name = READ_STRING();
        tableSet(&vm.globals, name, peek(0));
        pop();
        break;
      }

      // Global variable set expression op, leaves value on the stack for the same
      // reasons described in the notes for OP_SET_LOCAL below
      case OP_SET_GLOBAL: {
        ObjString* name = READ_STRING();
        if (tableSet(&vm.globals, name, peek(0))) {
          tableDelete(&vm.globals, name);
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }

      case OP_GET_GLOBAL: {
        // Get global variable name from constants via offset operand
        const ObjString* name = READ_STRING();

        // Get value from globals table
        Value value;
        if (!tableGet(&vm.globals, name, &value)) {
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        push(value);
        break;
      }

      // Local variable set expression op. Assigns the value at the top of the stack
      // to the stack slot where the local variable lives (obtained from the operand).
      //
      // The stack-top value stays because it is also the result of the assignment
      // expression and will be popped when consumed by further expressions (or at
      // the end of the statement if the assignment is an expression statement)
      case OP_SET_LOCAL: DO(frame->slots[READ_BYTE()] = peek(0));

      // Local variable get expression op. Reads the local variable from its stack
      // slot (obtained from the operand) and pushes it to the top of stack.
      //
      // The variable's value already exists on the stack in the slot from when it
      // was declared, which will be modified in-place by OP_SET_LOCAL and will stay on
      // the stack until the local goes out of scope.
      //
      // In contrast, the value we're pushing to the top of the stack here is the
      // result of variable get expression and will be consumed by the enclosing
      // expression (or discarded at the end of a statement)
      case OP_GET_LOCAL: DO(push(frame->slots[READ_BYTE()]));

      // Set the upvalue obtained from the operand byte.
      case OP_SET_UPVALUE: {
        const uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }

      // Pull an upvalue from the current closure and push it to the stack.
      case OP_GET_UPVALUE: {
        const uint8_t slot = READ_BYTE();
        push(*frame->closure->upvalues[slot]->location);
        break;
      }

      // Set the value of the property obtained from the operand byte on the instance
      // one slot down from the top of the stack to the value on the top of the stack.
      case OP_SET_PROPERTY: {
        if (!IS_INSTANCE(peek(1))) {
          runtimeError("Only class instances have fields");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjInstance* instance = AS_INSTANCE(peek(1));
        ObjString* ident = READ_STRING();

        tableSet(&instance->fields, ident, peek(0));
        const Value value = pop();

        // Pop instance off of the stack
        pop();

        push(value);
        break;
      }

      // Get the value of the property obtained from the operand byte on the instance
      // at the top of the stack
      case OP_GET_PROPERTY: {
        if (!IS_INSTANCE(peek(0))) {
          runtimeError("Only class instances have properties.");
          return INTERPRET_RUNTIME_ERROR;
        }

        const ObjInstance* instance = AS_INSTANCE(peek(0));
        const ObjString* ident = READ_STRING();

        Value value;
        if (tableGet(&instance->fields, ident, &value)) {
          pop();
          push(value);
          break;
        }

        // Return nil if property doesn't exist.
        push(NIL_VAL);
        break;
      }

      case OP_PRINT:
        printValue(pop());
        printf("\n");
        break;

      case OP_JUMP_IF_FALSE: {
        const uint16_t offset = READ_SHORT();
        if (isFalsey(peek(0))) frame->ip += offset;
        break;
      }

      case OP_JUMP_IF_TRUE: {
        const uint16_t offset = READ_SHORT();
        if (!isFalsey(peek(0))) frame->ip += offset;
        break;
      }

      case OP_JUMP_IF_NOT_NIL: {
        const uint16_t offset = READ_SHORT();
        if (!IS_NIL(peek(0))) frame->ip += offset;
        break;
      }

      case OP_JUMP: {
        const uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }

      case OP_LOOP: {
        const uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }

      case OP_CALL: {
        const int argCount = READ_BYTE();
        if (!callValue(peek(argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }

        // Update call frame to begin executing the called function
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }

      // Obtain lox function pointer from operand (constants table offset), wrap
      // it in a closure object and push it to the stack
      case OP_CLOSURE: {
        ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
        ObjClosure* closure = newClosure(function);
        push(OBJ_VAL(closure));

        // Capture upvalues from enclosing scopes
        for (int i = 0; i < function->upvalueCount; i++) {
          const uint8_t isLocal = READ_BYTE();
          const uint8_t offset = READ_BYTE();
          if (isLocal) {
            // Capture local upvalue from the surrounding call frame
            Value* upvaluePtr = frame->slots + offset;
            closure->upvalues[i] = captureUpvalue(upvaluePtr);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[offset];
          }
        }
        break;
      }

      // Obtain class identifier string from operand (constants table offset),
      // wrap it in a class object and push it to the stack
      case OP_CLASS: DO(push(OBJ_VAL(newClass(READ_STRING()))));

      // Return from the current call frame, pushing the result to the stack
      case OP_RETURN: {
        const Value result = pop();

        // Close open upvalues in the current call frame
        closeUpvalues(frame->slots);

        // Pop the call frame off the frame stack. If this was the outermost frame,
        // everything ran successfully, and we can exit the program
        vm.frameCount--;
        if (vm.frameCount == 0) {
          pop();
          return INTERPRET_OK;
        }

        // Discard frame stack by setting the stack top back to the frame's first
        // stack slot address
        vm.stackTop = frame->slots;

        push(result);

        // Set frame to the topmost call frame (the previous frame)
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }

      default:
        return INTERPRET_RUNTIME_ERROR;
    }
  }

  #undef READ_BYTE
  #undef READ_SHORT
  #undef READ_CONSTANT
  #undef READ_STRING
  #undef BINARY_OP
}

static void runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  // Print stack trace
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    const CallFrame* frame = &vm.frames[i];
    const ObjFunction* function = frame->closure->function;
    const size_t instruction = frame->ip - function->chunk.instructions;

    fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
    if (function->identifier == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->identifier->chars);
    }
  }

  resetStack();
}

// Native functions ===========================================================

static void defineNative(const char* name, const NativeFn function, const int arity) {
  // Copy name string, pushing it and native function to the stack so they can't
  // be garbage-collected before we're done defining the native function
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function, arity)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  popN(2);
}

static Value clockNative(int argCount, Value* args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}
