#ifndef CLOX_HASH_TABLE_H
#define CLOX_HASH_TABLE_H

#include "value.h"

typedef struct Entry {
  ObjString* key;
  Value value;
} Entry;

typedef struct Table {
  int count;
  int capacity;
  Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);

bool tableSet(Table* table, ObjString* key, Value value);
Value tableGet(Table* table, ObjString* key);

static Entry* findEntry(Entry* entries, int capacity, const ObjString* key);

#endif
