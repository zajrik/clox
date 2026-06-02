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

  /// Pop the top value off of the stack, printing it.
  OP_PRINT,

  OP_DEFINE_GLOBAL,
  OP_SET_GLOBAL,
  OP_GET_GLOBAL,

  OP_SET_LOCAL,
  OP_GET_LOCAL,

  OP_JUMP_IF_FALSE,
  OP_JUMP_IF_TRUE,
  OP_JUMP,
  OP_LOOP,

  /// Return something eventually.
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
