#pragma once

#include "sqliteInt.h"
#include "vdbeInt.h"

#ifdef __cplusplus
extern "C" {
#endif

int sqlite3InMemSorterInit(sqlite3 *, int, VdbeCursor *);
void sqlite3InMemSorterReset(sqlite3 *, VdbeSorter *);
void sqlite3InMemSorterClose(sqlite3 *, VdbeCursor *);
int sqlite3InMemSorterRowkey(const VdbeCursor *, Mem *);
int sqlite3InMemSorterNext(sqlite3 *, const VdbeCursor *);
int sqlite3InMemSorterRewind(const VdbeCursor *, int *);
int sqlite3InMemSorterWrite(const VdbeCursor *, Mem *);
int sqlite3InMemSorterCompare(const VdbeCursor *, Mem *, int, int *);

#ifdef __cplusplus
}
#endif