#include <stdio.h>
#include <stdlib.h>

#include "headers/chunk.h"
#include "headers/vm.h"

static void repl() {
  char line[1024];

  loop {
    printf("> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    interpret(line);
  }
}

/// Read the contents of the file at the given file [path].
///
/// Returns the contents of the file as an array of chars, terminated by `\0`.
static char* readFile(const char* path) {
  FILE* file = fopen(path, "rb");

  if (file == NULL) {
    fprintf(stderr, "Could not open file '%s'.\n", path);
    exit(74);
  }

  // Get file size by moving file cursor to end of file and getting its position
  fseek(file, 0L, SEEK_END);
  const size_t fileSize = ftell(file);
  rewind(file);

  // Allocate buffer
  char* buffer = malloc(fileSize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read '%s'.\n", path);
    exit(74);
  }

  // Read file into buffer
  const size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file '%s'.\n", path);
    exit(74);
  }

  // Terminate buffer
  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

/// Interpret the file at the given file [path].
static void runFile(const char* path) {
  char* source = readFile(path);
  const InterpretResult result = interpret(source);
  free(source);

  if (result == INTERPRET_COMPILE_ERROR) exit(65);
  if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(const int argc, const char* argv[]) {
  initVm();

  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    runFile(argv[1]);
  } else {
    fprintf(stderr, "Usage: clox [path]\n");
    exit(64);
  }

  freeVm();

  return 0;
}
