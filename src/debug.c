#include <stdio.h>

#include "headers/debug.h"
#include "headers/value.h"

/// Print the contents (instructions, values, etc.) of the chunk at address [chunk].
void disassembleChunk(const Chunk* chunk, const char* name) {
  printf("== %s ==\n", name);

  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

/// Prints the instruction from the chunk at address [chunk], at [offset] in the
/// instructions array.
///
/// Returns the offset of the next instruction.
int disassembleInstruction(const Chunk* chunk, int offset) {
  printf("%04d ", offset);

  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4d ", chunk->lines[offset]);
  }

  const uint8_t instruction = chunk->instructions[offset];

  switch (instruction) {
    case OP_CONSTANT: return constantInstruction("OP_CONSTANT", chunk, offset);

    case OP_NIL: return simpleInstruction("OP_NIL", offset);
    case OP_TRUE: return simpleInstruction("OP_TRUE", offset);
    case OP_FALSE: return simpleInstruction("OP_FALSE", offset);
    case OP_NOT: return simpleInstruction("OP_NOT", offset);

    case OP_ADD: return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT: return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY: return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE: return simpleInstruction("OP_DIVIDE", offset);
    case OP_NEGATE: return simpleInstruction("OP_NEGATE", offset);

    case OP_RETURN: return simpleInstruction("OP_RETURN", offset);

    default:
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}

/// Print the [name] of a simple instruction at [offset] in the instructions array.
///
/// Returns the offset of the next instruction.
static int simpleInstruction(const char* name, int offset) {
  printf("%s\n", name);

  return offset + 1;
}

/// Prints the [name] of a constant instruction at [offset] in the instructions
/// array, as well the value of the constant operand following the instruction.
///
/// Returns the offset of the next instruction (skipping over the constant operand).
static int constantInstruction(const char* name, const Chunk* chunk, int offset) {
  // Get the constant value byte after the instruction byte
  const uint8_t constant = chunk->instructions[offset + 1];

  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");

  return offset + 2;
}
