#ifdef DEBUG_LOG_GC
#include <stdio.h>
#endif

#include "memory.h"
#include "common.h"
#include "compiler.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define GC_HEAP_GROW_FACTOR 2

/// Reallocates memory at address [ptr], allocating [newSize] bytes.
///
/// If [newSize] is `0`, memory allocated at [ptr] will be freed.
///
/// Returns a pointer to the newly allocated block of memory.
void* reallocate(void* ptr, const size_t oldSize, const size_t newSize) {
  vm.bytesAllocated += newSize - oldSize;

  #ifdef DEBUG_STRESS_GC
  if (newSize > oldSize) {
    collectGarbage();
  }
  #endif

  if (vm.bytesAllocated > vm.nextGC) {
    collectGarbage();
  }

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
  // #ifdef DEBUG_LOG_GC
  printf("%p freed type %d\n", (void*)object, object->type);
  // #endif

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

    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
      FREE(ObjClosure, closure);
      break;
    }

    case OBJ_CLASS: {
      ObjClass* classObj = (ObjClass*)object;
      freeTable(&classObj->methods);
      FREE(ObjClass, classObj);
      break;
    }

    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)object;
      freeTable(&instance->fields);
      FREE(ObjInstance, instance);
      break;
    }

    case OBJ_METHOD: {
      ObjMethod* method = (ObjMethod*)object;
      FREE(ObjMethod, method);
      break;
    }

    case OBJ_UPVALUE: {
      FREE(ObjUpvalue, object);
      break;
    }

    case OBJ_NATIVE: {
      FREE(ObjNative, object);
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

  free(vm.gcStack);
}

/// Run the garbage collector.
///
/// The garbage collector will mark objects that are still in use and all unmarked
/// objects will be freed.
void collectGarbage() {
  #ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
  const size_t before = vm.bytesAllocated;
  #endif

  markRoots();
  traceReferences();
  tableRemoveUnmarked(&vm.strings);
  sweep();

  vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

  #ifdef DEBUG_LOG_GC
  printf("-- gc end\n");
  printf(
    "    collected %zu bytes (from %zu to %zu), next at %zu\n",
    before - vm.bytesAllocated,
    before,
    vm.bytesAllocated,
    vm.nextGC
  );
  #endif
}

/// Mark reachable objects for the GC, starting at the roots of any structures
/// that hold allocated object references.
static void markRoots() {
  // Mark stack values
  for (const Value* slot = vm.stack; slot < vm.stackTop; slot++) {
    markValue(*slot);
  }

  // Mark call frame closures
  for (int i = 0; i < vm.frameCount; i++) {
    markObject((Obj*)vm.frames[i].closure);
  }

  // Mark open upvalues
  for (
    ObjUpvalue* upvalue = vm.openUpvalues;
    upvalue != NULL;
    upvalue = upvalue->next
  ) {
    markObject((Obj*)upvalue);
  }

  markTable(&vm.globals);
  markCompilerRoots();
}

/// Trace through marked object references to mark any further reachable objects.
static void traceReferences() {
  while (vm.gcCount > 0) {
    Obj* obj = vm.gcStack[--vm.gcCount];
    visitObject(obj);
  }
}

/// Free all objects that were not marked by the GC tracing pass.
static void sweep() {
  Obj* obj = NULL;
  Obj* nextObj = vm.objects;

  // Walk VM objects list
  while (nextObj != NULL) {
    // Skip over marked objects and unmark them
    if (nextObj->isAlive) {
      nextObj->isAlive = false;

      obj = nextObj;
      nextObj = nextObj->next;
    }

    // Free unmarked objects, removing them from the VM's linked list of objects
    else {
      Obj* unmarked = nextObj;
      nextObj = nextObj->next;

      if (obj != NULL) {
        obj->next = nextObj;
      } else {
        vm.objects = nextObj;
      }

      freeObject(unmarked);
    }
  }
}

/// Mark the given [value] to allow it to live through a GC cycle.
void markValue(const Value value) {
  if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

/// Mark the given object to allow it to live through a GC cycle.
void markObject(Obj* obj) {
  // Can't mark a null pointer or already-marked objects
  if (obj == NULL || obj->isAlive) return;

  #ifdef DEBUG_LOG_GC
  printf("%p marked ", (void*)obj);
  printObjectValue(OBJ_VAL(obj));
  printf("\n");
  #endif

  obj->isAlive = true;

  // Reallocate GC stack if necessary
  if (vm.gcCapacity < vm.gcCount + 1) {
    vm.gcCapacity = GROW_CAPACITY(vm.gcCapacity);

    Obj** gcStack = realloc(vm.gcStack, sizeof(Obj*) * vm.gcCapacity);
    if (gcStack == NULL) exit(1);

    vm.gcStack = gcStack;
  }

  // Add object to GC stack
  vm.gcStack[vm.gcCount++] = obj;
}

/// Mark all values in the given [ValueArray] to allow them to live through a
/// GC cycle.
static void markArray(const ValueArray* array) {
  for (int i = 0; i < array->count; i++) {
    markValue(array->values[i]);
  }
}

/// Visit all object references reachable from the given object, marking them
/// as alive for the current GC cycle.
static void visitObject(Obj* obj) {
  #ifdef DEBUG_LOG_GC
  printf("%p visit ", (void*)obj);
  printValue(OBJ_VAL(obj));
  printf("\n");
  #endif

  switch (obj->type) {
    case OBJ_NATIVE:
    case OBJ_STRING:
      break;

    case OBJ_CLASS: {
      const ObjClass* classObj = (ObjClass*)obj;
      markObject((Obj*)classObj->identifier);
      markTable(&classObj->methods);
      break;
    }

    case OBJ_INSTANCE: {
      const ObjInstance* instance = (ObjInstance*)obj;
      markObject((Obj*)instance->classObj);
      markTable(&instance->fields);
    }

    case OBJ_METHOD: {
      const ObjMethod* method = (ObjMethod*)obj;
      markValue(method->receiver);
      markObject((Obj*)method->closure);
    }

    case OBJ_CLOSURE: {
      const ObjClosure* closure = (ObjClosure*)obj;
      markObject((Obj*)closure->function);
      for (int i = 0; i < closure->upvalueCount; i++) {
        markObject((Obj*)closure->upvalues[i]);
      }
      break;
    }

    case OBJ_FUNCTION: {
      const ObjFunction* function = (ObjFunction*)obj;
      markObject((Obj*)function->identifier);
      markArray(&function->chunk.constants);
      break;
    }

    case OBJ_UPVALUE: DO(markValue(((ObjUpvalue*)obj)->closed));
  }
}
