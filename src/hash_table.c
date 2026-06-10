#include <string.h>

#include "hash_table.h"
#include "common.h"
#include "memory.h"
#include "object.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

/// Initialize the given [table].
void initTable(Table* table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

/// Free resources used by the given [table].
///
/// The table will be zeroed out for re-use.
void freeTable(Table* table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}

/// Reallocate the given [table] to the given [capacity].
///
/// Existing entries will be re-inserted.
static void expandTable(Table* table, const int capacity) {
  table->count = 0;

  // Allocate a new entries array
  Entry* entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }

  // Move existing entries to the newly allocated array
  for (int i = 0; i < table->capacity; i++) {
    const Entry* entry = &table->entries[i];
    if (entry->key == NULL) continue;

    Entry* dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    table->count++;
  }

  FREE_ARRAY(Entry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}

/// Find an [Entry] in the given table with the given [key].
static Entry* findEntry(Entry* entries, const int capacity, const ObjString* key) {
  uint32_t index = key->hash % capacity;
  Entry* tombstone = NULL;

  loop {
    Entry* entry = &entries[index];

    // If entry key is NULL but value is nil, it's an empty slot. If the value isn't
    // nil then it's a tombstone, and we'll set that aside and move to the next
    // slot.
    //
    // If we hit an empty slot again, but we have a tombstone set aside then the
    // tombstone is a valid entry slot so we return the tombstone.
    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) return tombstone ? : entry;
      if (tombstone == NULL) tombstone = entry;
    }

    // Otherwise check if key matches and move on to the next slot if not. Thanks
    // to string interning via the vm.strings table, string objects are guaranteed
    // to point to the same location in memory, so this comparison is safe.
    else if (entry->key == key) return entry;

    index = (index + 1) % capacity;
  }
}

/// Set a [value] in the given [table] under the given [key].
///
/// Returns whether the key existed before this value was set.
bool tableSet(Table* table, ObjString* key, const Value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    expandTable(table, GROW_CAPACITY(table->capacity));
  }

  Entry* entry = findEntry(table->entries, table->capacity, key);
  const bool isNewKey = entry->key == NULL;

  // Only increase count if we're not overwriting a tombstone
  if (isNewKey && IS_NIL(entry->value)) table->count++;

  entry->key = key;
  entry->value = value;

  return isNewKey;
}

/// Add all entries from table [a] to table [b].
void tableAddAll(const Table* from, Table* to) {
  for (int i = 0; i < from->capacity; i++) {
    const Entry* entryA = &from->entries[i];
    if (entryA->key == NULL) continue;
    tableSet(to, entryA->key, entryA->value);
  }
}

/// Get a value from the given [table] by the given [key].
///
/// If the value exists it will be stored at the given [valuePtr].
///
/// Returns whether the key was found in the given table.
bool tableGet(const Table* table, const ObjString* key, Value* valuePtr) {
  if (table->count == 0) return false;

  const Entry* entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  *valuePtr = entry->value;
  return true;
}

/// Returns whether the given [table] has the given [key].
bool tableHasKey(const Table* table, const ObjString* key) {
  if (table->count == 0) return false;
  return findEntry(table->entries, table->capacity, key)->key != NULL;
}

/// Delete the value for the given [key] from the given [table] if it exists.
///
/// Returns whether the value existed and was deleted.
bool tableDelete(const Table* table, const ObjString* key) {
  if (table->count == 0) return false;

  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  // Leave tombstone value in this entry slot.
  entry->key = NULL;
  entry->value = BOOL_VAL(true);

  return true;
}

/// Find a string key from the given [table] matching the given string data.
///
/// Returns a pointer to the string object, or `NULL` if it was not found.
ObjString* tableFindString(const Table* table, const char* chars, const int length, const uint32_t hash) {
  if (table->count == 0) return NULL;

  uint32_t index = hash % table->capacity;
  loop {
    const Entry* entry = &table->entries[index];

    // If the entry is empty, continue if it's a tombstone, otherwise return null
    // because this indicates that the string does not exist in the table
    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) return NULL;
      index = (index + 1) % table->capacity;
      continue;
    }

    const bool stringsEqual = entry->key->length == length
      && entry->key->hash == hash
      && memcmp(entry->key->chars, chars, length) == 0;

    // If the strings match, return the string key
    if (stringsEqual) return entry->key;

    index = (index + 1) % table->capacity;
  }
}

/// Mark the objects in the given table for the GC.
void markTable(const Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    const Entry* entry = &table->entries[i];
    markObject((Obj*)entry->key);
    markValue(entry->value);
  }
}

/// Remove objects that the GC has not marked as alive.
void tableRemoveUnmarked(const Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    const Entry* entry = &table->entries[i];
    if (entry->key != NULL && !entry->key->object.isAlive) {
      tableDelete(table, entry->key);
    }
  }
}
