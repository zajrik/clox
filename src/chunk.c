#include <stdlib.h>

#include "headers/chunk.h"
#include "headers/memory.h"

/// Initialize a chunk at address [chunk].
void initChunk(Chunk* chunk) {
  chunk->capacity = 0;
  chunk->count = 0;
  chunk->instructions = NULL;
  chunk->lines = NULL;
  initValueArray(&chunk->constants);
}

/// Free a chunk at address [chunk].
///
/// The chunk will be zeroed-out for re-use.
void freeChunk(Chunk* chunk) {
  FREE_ARRAY(uint8_t, chunk->instructions, chunk->capacity);
  FREE_ARRAY(int, chunk->lines, chunk->capacity);
  freeValueArray(&chunk->constants);
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
  writeValueArray(&chunk->constants, value);
  return chunk->constants.count - 1;
}
