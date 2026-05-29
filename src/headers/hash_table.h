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
void tableAddAll(const Table* from, Table* to);
bool tableGet(const Table* table, const ObjString* key, Value* valuePtr);
bool tableHasKey(const Table* table, const ObjString* key);
bool tableDelete(const Table* table, const ObjString* key);

ObjString* tableFindString(const Table* table, const char* chars, int length, uint32_t hash);

static Entry* findEntry(Entry* entries, int capacity, const ObjString* key);

#endif
