#include <stdio.h>

#include "debug.h"
#include "object.h"
#include "value.h"

/// Print the contents (instructions, values, etc.) of the chunk at address [chunk].
void disassembleChunk(const Chunk* chunk, const char* name) {
  printf("== %s ==\n", name);

  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }

  for (int i = 0; i < chunk->constants.count; i++) {
    printf("Constant %d: ", i);
    printValue(chunk->constants.values[i]);
    printf("\n");
  }
}

/// Prints the instruction from the chunk at address [chunk], at [offset] in the
/// instructions array.
///
/// Returns the offset of the next instruction.
int disassembleInstruction(const Chunk* chunk, const int offset) {
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

    case OP_EQUAL: return simpleInstruction("OP_EQUAL", offset);
    case OP_GREATER: return simpleInstruction("OP_GREATER", offset);
    case OP_LESS: return simpleInstruction("OP_LESS", offset);

    case OP_ADD: return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT: return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY: return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE: return simpleInstruction("OP_DIVIDE", offset);
    case OP_NEGATE: return simpleInstruction("OP_NEGATE", offset);

    case OP_COPY: return simpleInstruction("OP_COPY", offset);
    case OP_POP: return simpleInstruction("OP_POP", offset);
    case OP_POP_N: return byteInstruction("OP_POP_N", chunk, offset);
    case OP_CLOSE_UPVALUE: return simpleInstruction("OP_CLOSE_UPVALUE", offset);

    case OP_DEFINE_GLOBAL: return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);

    case OP_SET_VALUE: return variableInstruction("OP_SET_VALUE", chunk, offset);
    case OP_GET_VALUE: return variableInstruction("OP_GET_VALUE", chunk, offset);

    case OP_PRINT: return simpleInstruction("OP_PRINT", offset);

    case OP_JUMP_IF_FALSE: return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
    case OP_JUMP_IF_TRUE: return jumpInstruction("OP_JUMP_IF_TRUE", 1, chunk, offset);
    case OP_JUMP_IF_NOT_NIL: return jumpInstruction("OP_JUMP_IF_NOT_NIL", 1, chunk, offset);
    case OP_JUMP: return jumpInstruction("OP_JUMP", 1, chunk, offset);
    case OP_LOOP: return jumpInstruction("OP_LOOP", -1, chunk, offset);

    case OP_CALL: return byteInstruction("OP_CALL", chunk, offset);
    case OP_INVOKE: return invokeInstruction("OP_INVOKE", chunk, offset);

    case OP_CLOSURE: {
      int newOffset = offset + 1;
      const uint8_t constant = chunk->instructions[newOffset++];
      const Value funConstant = chunk->constants.values[constant];
      printf("%-16s %4d ", "OP_CLOSURE", constant);
      printValue(funConstant);
      printf("\n");

      const ObjFunction* function = AS_FUNCTION(funConstant);
      for (int i = 0; i < function->upvalueCount; i++) {
        const int isLocal = chunk->instructions[newOffset++];
        const int upvalue = chunk->instructions[newOffset++];
        printf(
          "%04d      |                     %s %d\n",
          newOffset - 2,
          isLocal ? "local" : "upvalue",
          upvalue
        );
      }

      return newOffset;
    }

    case OP_CLASS: return constantInstruction("OP_CLASS", chunk, offset);
    case OP_METHOD: return constantInstruction("OP_METHOD", chunk, offset);

    case OP_RETURN: return simpleInstruction("OP_RETURN", offset);

    default:
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}

/// Print the [name] of a simple instruction at [offset] in the instructions array.
///
/// Returns the offset of the next instruction.
static int simpleInstruction(const char* name, const int offset) {
  printf("%s\n", name);

  return offset + 1;
}

/// Prints the [name] of a constant instruction at [offset] in the instructions
/// array, as well the value of the constant operand following the instruction.
///
/// Returns the offset of the next instruction (skipping over the constant operand).
static int constantInstruction(const char* name, const Chunk* chunk, const int offset) {
  // Get the constant value byte after the instruction byte
  const uint8_t constant = chunk->instructions[offset + 1];

  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");

  return offset + 2;
}

/// Prints the [name] of a variable instruction at [offset] in the given [chunk]'s
/// instructions array, followed by the variable offset and the [VariableKind] from
/// the following operands.
///
/// Returns the offset of the next instruction (skipping over the operands).
static int variableInstruction(const char* name, const Chunk* chunk, const int offset) {
  const VariableKind kind = chunk->instructions[offset + 1];
  const uint8_t varOffset = chunk->instructions[offset + 2];

  const char* kindStr = "UNKNOWN";
  switch (kind) {
    case VAR_GLOBAL: DO(kindStr = "GLOBAL");
    case VAR_LOCAL: DO(kindStr = "LOCAL");
    case VAR_UPVALUE: DO(kindStr = "UPVALUE");
    case VAR_PROPERTY: DO(kindStr = "PROPERTY");
  }

  printf("%-16s %4d (%s)\n", name, varOffset, kindStr);
  return offset + 3;
}

/// Prints the [name] of the given invoke instruction, as well as the two operands
/// following it.
///
/// Returns the offset of the next instruction (skipping over the operands).
static int invokeInstruction(const char* name, const Chunk* chunk, const int offset) {
  const uint8_t constant = chunk->instructions[offset + 1];
  const uint8_t argCount = chunk->instructions[offset + 2];

  printf("%-16s (%d args) %4d '", name, argCount, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");

  return offset + 3;
}

/// Print the [name] of an instruction accepting a single operand, along with the
/// raw byte value of its operand.
static int byteInstruction(const char* name, const Chunk* chunk, const int offset) {
  const uint8_t byte = chunk->instructions[offset + 1];
  printf("%-16s %4d\n", name, byte);

  return offset + 2;
}

/// Print a jump instruction and its operand.
static int jumpInstruction(
  const char* name,
  const int sign,
  const Chunk* chunk,
  const int offset
) {
  uint16_t jump = (uint16_t)(chunk->instructions[offset + 1] << 8);
  jump |= chunk->instructions[offset + 2];
  printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
  return offset + 3;
}
