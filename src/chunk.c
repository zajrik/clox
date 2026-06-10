#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

/// Initialize a chunk at address [chunk].
void initChunk(Chunk* chunk) {
  chunk->capacity = 0;
  chunk->count = 0;
  chunk->instructions = NULL;
  chunk->lines = NULL;
  initValueArray(&chunk->constants);
  initTable(&chunk->strings);
}

/// Free a chunk at address [chunk].
///
/// The chunk will be zeroed-out for re-use.
void freeChunk(Chunk* chunk) {
  FREE_ARRAY(uint8_t, chunk->instructions, chunk->capacity);
  FREE_ARRAY(int, chunk->lines, chunk->capacity);
  freeValueArray(&chunk->constants);
  freeTable(&chunk->strings);
  initChunk(chunk);
}

/// Write a [byte] to the instructions array of the chunk at address [chunk].
///
/// The chunk will be resized and reallocated to fit as needed.
void writeChunk(Chunk* chunk, const uint8_t byte, const int line) {
  if (chunk->capacity < chunk->count + 1) {
    const int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->instructions = GROW_ARRAY(uint8_t, chunk->instructions, oldCapacity, chunk->capacity);
    chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
  }

  chunk->instructions[chunk->count] = byte;
  chunk->lines[chunk->count] = line;
  chunk->count++;
}

/// Add the given constant [value] to the constants value-array of the chunk
/// pointed to by [chunk].
///
/// Returns the offset of the added constant value within the constants array.
int addConstant(Chunk* chunk, const Value value) {
  // Push value to stack to keep GC from reclaiming it. We'll pop it when it's
  // safely tucked away in the constants array
  push(value);

  // If value is not a string we can just add it to the chunk constants
  if (!IS_STRING(value)) {
    writeValueArray(&chunk->constants, value);
    pop();
    return chunk->constants.count - 1;
  }

  // If the value IS a string, we need to check if we already have an interned
  // constant index for that string. We'll return that index if it exists or add
  // the new string constant to the chunk and intern its offset.

  ObjString* string = AS_STRING(value);

  if (tableHasKey(&chunk->strings, string)) {
    Value stringIndex;
    tableGet(&chunk->strings, string, &stringIndex);
    pop();
    return AS_NUMBER(stringIndex);
  }

  writeValueArray(&chunk->constants, value);
  const int constantOffset = chunk->constants.count - 1;
  tableSet(&chunk->strings, string, NUMBER_VAL(constantOffset));
  pop();
  return constantOffset;
}
