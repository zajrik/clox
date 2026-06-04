#include "headers/memory.h"

#include "headers/common.h"
#include "headers/object.h"
#include "headers/value.h"
#include "headers/vm.h"

/// Reallocates memory at address [ptr], allocating [newSize] bytes.
///
/// If [newSize] is `0`, memory allocated at [ptr] will be freed.
///
/// Returns a pointer to the newly allocated block of memory.
void* reallocate(void* ptr, const size_t oldSize, const size_t newSize) {
  if (newSize == 0) {
    free(ptr);
    return NULL;
  }

  void* result = realloc(ptr, newSize);
  if (result == NULL) exit(1);
  return result;
}

/// Free memory used by the lox object at address [object].
void freeObject(Obj* object) {
  switch (object->type) {
    case OBJ_STRING: {
      const ObjString* strObj = (ObjString*)object;
      FREE_ARRAY(char, strObj->chars, strObj->length + 1);
      FREE(ObjString, object);
      break;
    }

    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      freeChunk(&function->chunk);
      FREE(ObjFunction, function);
      break;
    }
  }
}

/// Free all allocated objects on the VM.
void freeObjects() {
  Obj* object = vm.objects;
  while (object != NULL) {
    Obj* next = object->next;
    freeObject(object);
    object = next;
  }
}
