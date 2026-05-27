#include <string.h>

#include "headers/object.h"
#include "headers/memory.h"
#include "headers/value.h"
#include "headers/vm.h"

/// Allocate a block of memory for a lox object sized for the given [type] and
/// returns a pointer to it.
///
/// The object will have the given [objectType] type marker.
#define ALLOCATE_OBJ(type, objectType) \
  (type*)allocateObject(sizeof(type), objectType)

/// Allocates a new lox object of [size] and [type] and returns a pointer to it.
Obj* allocateObject(const size_t size, const ObjType type) {
  Obj* object = reallocate(NULL, 0, size);
  // ReSharper disable once CppDFANullDereference
  // SAFETY: reallocate will exit if it fails to allocate the memory
  object->type = type;

  // Link vm objects for cleaning up when vm closes.
  object->next = vm.objects;
  vm.objects = object;

  return object;
}

/// Allocates a new lox string object and returns a pointer to it.
ObjString* allocateString(char* chars, const int length) {
  ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  return string;
}

/// Copy the given c-string into a freshly allocated block of memory and wrap
/// it into an [ObjString].
///
/// Returns a pointer to the allocated [ObjString].
///
/// Constant c-strings must be copied to ensure programs can't free memory used
/// by the original compile-time constant strings.
ObjString* copyString(const char* chars, const int length) {
  char* heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length);
}

/// Print the given lox object value.
void printObject(const Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_STRING: DO(printf("%s", AS_CSTRING(value)));
  }
}
