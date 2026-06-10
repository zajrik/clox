#ifndef CLOX_CHUNK_H
#define CLOX_CHUNK_H

#include "common.h"
#include "hash_table.h"
#include "value.h"

/// An instruction opcode to be executed by the lox virtual machine.
typedef enum OpCode {
  /// Push a constant value onto the stack.
  ///
  /// Must be followed by an offset pointing to the constant value in the executing
  /// chunk's constants array.
  OP_CONSTANT,

  /// Push literal `nil` onto the stack.
  OP_NIL,

  /// Push literal `true` onto the stack.
  OP_TRUE,

  /// Push literal `false` onto the stack.
  OP_FALSE,

  /// Pop two values off stack, compare, push result to stack.
  OP_EQUAL,

  /// Pop two values off stack, compare, push result to stack.
  OP_GREATER,

  /// Pop two values off stack, compare, push result to stack.
  OP_LESS,

  /// Pop two values off stack, add them, push result to stack.
  OP_ADD,

  /// Pop two values off stack, subtract them, push result to the stack.
  OP_SUBTRACT,

  /// Pop two values off stack, multiply them, push result to stack.
  OP_MULTIPLY,

  /// Pop two values off stack, divide them, push result to stack.
  OP_DIVIDE,

  /// Pop boolean values off stack, negate it, push result to stack.
  OP_NOT,

  /// Pop one value off stack, negate it, push result to stack.
  OP_NEGATE,

  /// Push a copy of the value at the top of the stack to the top of the stack.
  OP_COPY,

  /// Pop the top value off of the stack, discarding it.
  OP_POP,

  /// Pop the top N values off of the stack, where N is the operand, discarding them.
  OP_POP_N,

  /// Close upvalue on top of the stack, moving it to the heap and popping it from
  /// the top of the stack.
  OP_CLOSE_UPVALUE,

  /// Pop the top value off of the stack, printing it.
  OP_PRINT,

  OP_DEFINE_GLOBAL,
  OP_SET_GLOBAL,
  OP_GET_GLOBAL,

  OP_SET_LOCAL,
  OP_GET_LOCAL,

  OP_SET_UPVALUE,
  OP_GET_UPVALUE,

  /// Pop the top value off of the stack, assign it to a field with the name obtained
  /// from the operand byte (string constants table offset) on the receiver object
  /// below the popped value on the stack.
  OP_SET_PROPERTY,

  /// Pop the receiver object off of the top of the stack. Push the property with
  /// the name obtained from the operand byte (string constants table offset)
  /// found on the receiver to the top of the stack.
  OP_GET_PROPERTY,

  OP_JUMP_IF_FALSE,
  OP_JUMP_IF_TRUE,
  OP_JUMP_IF_NOT_NIL,
  OP_JUMP,
  OP_LOOP,

  /// Call a function on the stack. Must be followed by an operand specifying
  /// how many arguments are to be passed to the function.
  ///
  /// The function value will be on the stack, followed by all of its argument
  /// values.
  OP_CALL,

  /// Create a closure at runtime. Must be followed by an operand specifying a
  /// constants table offset pointing to a function value.
  OP_CLOSURE,

  /// Creates a class object at runtime. Must be followed by an operand specifying
  /// a constants table offset pointing to the class identifier string.
  OP_CLASS,

  /// Assign the closure object on top of the stack as a method on the class object
  /// below it on the stack, popping the method closure off of the stack.
  OP_METHOD,

  /// Return a value (or nothing) from a function.
  OP_RETURN,
} OpCode;

/// A chunk of lox instruction opcodes and operands compiled from source.
///
/// Implemented as a dynamically-sized array of instructions and values (operands).
typedef struct Chunk {
  /// The maximum capacity of the instructions array.
  int capacity;

  /// The number of items in the instructions array.
  int count;

  /// Pointer to the first item in the instructions array.
  uint8_t* instructions;

  /// Pointer to the first item in the lines array.
  ///
  /// Lines array stores source line data and is wastefully allocated to match
  /// the instructions array. Challenge suggested run-length encoding instead.
  int* lines;

  /// Array of constants declared in this chunk.
  ///
  /// To be accessed by [OP_CONSTANT] instructions consuming offset operands
  /// pointing to the constant in the array.
  ValueArray constants;

  /// Hash-table of interned compile-time string constants, used to eliminate
  /// insertion of duplicate string entries into the constants array.
  ///
  /// Maps string value to offset within the constants array.
  Table strings;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);

int addConstant(Chunk* chunk, Value value);

#endif
