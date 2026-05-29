#ifndef CLOX_CHUNK_H
#define CLOX_CHUNK_H

#include "common.h"
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

  /// Pop two operands off stack, compare, push result to stack.
  OP_EQUAL,

  /// Pop two operands off stack, compare, push result to stack.
  OP_GREATER,

  /// Pop two operands off stack, compare, push result to stack.
  OP_LESS,

  /// Pop two operands off stack, add them, push result to stack.
  OP_ADD,

  /// Pop two operands off stack, subtract them, push result to the stack.
  OP_SUBTRACT,

  /// Pop two operands off stack, multiply them, push result to stack.
  OP_MULTIPLY,

  /// Pop two operands off stack, divide them, push result to stack.
  OP_DIVIDE,

  /// Pop boolean operand off stack, negate it, push result to stack.
  OP_NOT,

  /// Pop one operand off stack, negate it, push result to stack.
  OP_NEGATE,

  /// Pop the top value off of the stack, discarding it.
  OP_POP,

  /// Pop the top value off of the stack, printing it.
  OP_PRINT,

  OP_DEFINE_GLOBAL,
  OP_SET_GLOBAL,
  OP_GET_GLOBAL,

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
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);

int addConstant(Chunk* chunk, Value value);

#endif
