#ifndef CLOX_DEBUG_H
#define CLOX_DEBUG_H

#include "chunk.h"

void disassembleChunk(const Chunk* chunk, const char* name);
int disassembleInstruction(const Chunk* chunk, int offset);

static int simpleInstruction(const char* name, int offset);
static int constantInstruction(const char* name, const Chunk* chunk, int offset);
static int byteInstruction(const char* name, const Chunk* chunk, int offset);

#endif
