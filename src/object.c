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

/// Hashes a [string] of the given [length] using the FNV-1a hashing algorithm.
uint32_t hashString(const char* string, const int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)string[i];
    hash *= 16777619;
  }
  return hash;
}

/// Allocates a new lox string object and returns a pointer to it.
///
/// Allocated strings will be interned in the VM to ensure identical strings all
/// point to the same object at runtime.
static ObjString* allocateString(char* chars, const int length, const uint32_t hash) {
  ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  // Intern newly allocated string
  tableSet(&vm.strings, string, NIL_VAL);

  return string;
}

/// Copy the given c-string into a freshly allocated block of memory and wrap
/// it into an [ObjString].
///
/// Returns a pointer to the allocated [ObjString].
///
/// Constant c-strings must be copied to ensure programs can't free memory used
/// by the lox source code string.
ObjString* copyString(const char* chars, const int length) {
  const uint32_t hash = hashString(chars, length);

  // Check for matching interned string before allocating a new one
  ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) return interned;

  char* heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';

  return allocateString(heapChars, length, hash);
}

/// Take ownership of the given string data, allocating a new [ObjString] to
/// wrap it.
///
/// This should only be used for strings created dynamically at runtime since we
/// know the memory for those strings is safe to manipulate. Use [copyString] to
/// create a safe copy of a compile-time constant string.
///
/// Returns a pointer to the new [ObjString].
ObjString* takeString(char* chars, const int length) {
  const uint32_t hash = hashString(chars, length);

  // Check for matching interned string before allocating a new one
  ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

  return allocateString(chars, length, hash);
}

/// Print the given lox object value.
void printObject(const Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_STRING: DO(printf("%s", AS_CSTRING(value)));
  }
}
