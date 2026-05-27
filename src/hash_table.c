#include "headers/hash_table.h"
#include "headers/common.h"
#include "headers/memory.h"
#include "headers/object.h"
#include "headers/value.h"

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
  }

  FREE_ARRAY(Entry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}

/// Find an [Entry] in the given table with the given [key].
static Entry* findEntry(Entry* entries, const int capacity, const ObjString* key) {
  uint32_t index = key->hash % capacity;
  loop {
    Entry* entry = &entries[index];
    if (entry->key == key || entry->key == NULL) return entry;
    index = (index + 1) % capacity;
  }
}

/// Set a [value] in the given [table] under the given [key].
bool tableSet(Table* table, ObjString* key, const Value value) {
  if (table->count + 1 < table->capacity * TABLE_MAX_LOAD) {
    expandTable(table, GROW_CAPACITY(table->capacity));
  }

  Entry* entry = findEntry(table->entries, table->capacity, key);
  const bool isNewKey = entry->key == NULL;
  if (isNewKey) table->count++;

  entry->key = key;
  entry->value = value;

  return isNewKey;
}
