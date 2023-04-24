/*
** 2011-07-09
** Here is the (internal, non-API) interface between this module and the
** rest of the SQLite system:
**
**    sqlite3VdbeSorterInit()       Create a new VdbeSorter object.
**
**    sqlite3VdbeSorterWrite()      Add a single new row to the VdbeSorter
**                                  object.  The row is a binary blob in the
**                                  OP_MakeRecord format that contains both
**                                  the ORDER BY key columns and result columns
**                                  in the case of a SELECT w/ ORDER BY, or
**                                  the complete record for an index entry
**                                  in the case of a CREATE INDEX.
**
**    sqlite3VdbeSorterRewind()     Sort all content previously added.
**                                  Position the read cursor on the
**                                  first sorted element.
**
**    sqlite3VdbeSorterNext()       Advance the read cursor to the next sorted
**                                  element.
**
**    sqlite3VdbeSorterRowkey()     Return the complete binary blob for the
**                                  row currently under the read cursor.
**
**    sqlite3VdbeSorterCompare()    Compare the binary blob for the row
**                                  currently under the read cursor against
**                                  another binary blob X and report if
**                                  X is strictly less than the read cursor.
**                                  Used to enforce uniqueness in a
**                                  CREATE UNIQUE INDEX statement.
**
**    sqlite3VdbeSorterClose()      Close the VdbeSorter object and reclaim
**                                  all resources.
**
**    sqlite3VdbeSorterReset()      Refurbish the VdbeSorter for reuse.  This
**                                  is like Close() followed by Init() only
**                                  much faster.
*/
#include "sqliteInt.h"
#include "vdbeInt.h"

/*
** Private objects used by the sorter
*/
typedef struct SorterRecord SorterRecord; /* A record being sorted */
typedef struct SortSubtask SortSubtask;   /* A sub-task in the sort process */
typedef struct SorterList SorterList;     /* In-memory list of records */

/*
** An in-memory list of objects to be sorted.
**
** If aMemory==0 then each object is allocated separately and the objects
** are connected using SorterRecord.u.pNext.  If aMemory!=0 then all objects
** are stored in the aMemory[] bulk memory, one right after the other, and
** are connected using SorterRecord.u.iNext.
*/
struct SorterList {
  SorterRecord *pList; /* Linked list of records */
  u8 *aMemory;         /* If non-NULL, bulk memory to hold pList */
};

/*
** This object represents a single thread of control in a sort operation.
** Exactly VdbeSorter.nTask instances of this object are allocated
** as part of each VdbeSorter object. Instances are never allocated any
** other way. VdbeSorter.nTask is set to the number of worker threads allowed
** (see SQLITE_CONFIG_WORKER_THREADS) plus one (the main thread).  Thus for
** single-threaded operation, there is exactly one instance of this object
** and for multi-threaded operation there are two or more instances.
**
** Essentially, this structure contains all those fields of the VdbeSorter
** structure for which each thread requires a separate instance. For example,
** each thread requries its own UnpackedRecord object to unpack records in
** as part of comparison operations.
**
** Before a background thread is launched, variable bDone is set to 0. Then,
** right before it exits, the thread itself sets bDone to 1. This is used for
** two purposes:
**
**   1. When flushing the contents of memory to a level-0 PMA on disk, to
**      attempt to select a SortSubtask for which there is not already an
**      active background thread (since doing so causes the main thread
**      to block until it finishes).
**
**   2. If SQLITE_DEBUG_SORTER_THREADS is defined, to determine if a call
**      to sqlite3ThreadJoin() is likely to block. Cases that are likely to
**      block provoke debugging output.
**
** In both cases, the effects of the main thread seeing (bDone==0) even
** after the thread has finished are not dire. So we don't worry about
** memory barriers and such here.
*/
typedef int (*SorterCompare)(SortSubtask *, int *, const void *, int,
                             const void *, int);
struct SortSubtask {
  SQLiteThread *pThread;     /* Background thread, if any */
  int bDone;                 /* Set if thread is finished but not joined */
  VdbeSorter *pSorter;       /* Sorter that owns this sub-task */
  UnpackedRecord *pUnpacked; /* Space to unpack a record */
  SorterList list;           /* List for thread to write to a PMA */
  SorterCompare xCompare;    /* Compare function to use */
};

/*
** Main sorter structure. A single instance of this is allocated for each
** sorter cursor created by the VDBE.
**
** mxKeysize:
**   As records are added to the sorter by calls to sqlite3VdbeSorterWrite(),
**   this variable is updated so as to be set to the size on disk of the
**   largest record in the sorter.
*/
struct VdbeSorter {
  int mnPmaSize;             /* Minimum PMA size, in bytes */
  int mxPmaSize;             /* Maximum PMA size, in bytes.  0==no limit */
  int mxKeysize;             /* Largest serialized key seen so far */
  int pgsz;                  /* Main database page size */
  sqlite3 *db;               /* Database connection */
  KeyInfo *pKeyInfo;         /* How to compare records */
  UnpackedRecord *pUnpacked; /* Used by VdbeSorterCompare() */
  SorterList list;           /* List of in-memory records */
  int iMemory;               /* Offset of free space in list.aMemory */
  int nMemory;               /* Size of list.aMemory allocation in bytes */
  u8 bUseThreads;            /* True to use background threads */
  u8 iPrev;                  /* Previous thread used to flush PMA */
  u8 nTask;                  /* Size of aTask[] array */
  u8 typeMask;
  SortSubtask aTask[1]; /* One or more subtasks */
};

#define SORTER_TYPE_INTEGER 0x01
#define SORTER_TYPE_TEXT 0x02

/*
** This object is the header on a single record while that record is being
** held in memory and prior to being written out as part of a PMA.
**
** How the linked list is connected depends on how memory is being managed
** by this module. If using a separate allocation for each in-memory record
** (VdbeSorter.list.aMemory==0), then the list is always connected using the
** SorterRecord.u.pNext pointers.
**
** Or, if using the single large allocation method (VdbeSorter.list.aMemory!=0),
** then while records are being accumulated the list is linked using the
** SorterRecord.u.iNext offset. This is because the aMemory[] array may
** be sqlite3Realloc()ed while records are being accumulated. Once the VM
** has finished passing records to the sorter, or when the in-memory buffer
** is full, the list is sorted. As part of the sorting process, it is
** converted to use the SorterRecord.u.pNext pointers. See function
** vdbeSorterSort() for details.
*/
struct SorterRecord {
  int nVal; /* Size of the record in bytes */
  union {
    SorterRecord *pNext; /* Pointer to next record in list */
    int iNext;           /* Offset within aMemory of next record */
  } u;
  /* The data for the record immediately follows this header */
};

/* Return a pointer to the buffer containing the record data for SorterRecord
** object p. Should be used as if:
**
**   void *SRVAL(SorterRecord *p) { return (void*)&p[1]; }
*/
#define SRVAL(p) ((void *)((SorterRecord *)(p) + 1))

/* Maximum number of PMAs that a single MergeEngine can merge */
#define SORTER_MAX_MERGE_COUNT 16

static int vdbeSorterCompareTail(
    SortSubtask *pTask,           /* Subtask context (for pKeyInfo) */
    int *pbKey2Cached,            /* True if pTask->pUnpacked is pKey2 */
    const void *pKey1, int nKey1, /* Left side of comparison */
    const void *pKey2, int nKey2  /* Right side of comparison */
) {
  UnpackedRecord *r2 = pTask->pUnpacked;
  if (*pbKey2Cached == 0) {
    sqlite3VdbeRecordUnpack(pTask->pSorter->pKeyInfo, nKey2, pKey2, r2);
    *pbKey2Cached = 1;
  }
  return sqlite3VdbeRecordCompareWithSkip(nKey1, pKey1, r2, 1);
}

/*
** Compare key1 (buffer pKey1, size nKey1 bytes) with key2 (buffer pKey2,
** size nKey2 bytes). Use (pTask->pKeyInfo) for the collation sequences
** used by the comparison. Return the result of the comparison.
**
** If IN/OUT parameter *pbKey2Cached is true when this function is called,
** it is assumed that (pTask->pUnpacked) contains the unpacked version
** of key2. If it is false, (pTask->pUnpacked) is populated with the unpacked
** version of key2 and *pbKey2Cached set to true before returning.
**
** If an OOM error is encountered, (pTask->pUnpacked->error_rc) is set
** to SQLITE_NOMEM.
*/
static int vdbeSorterCompare(
    SortSubtask *pTask,           /* Subtask context (for pKeyInfo) */
    int *pbKey2Cached,            /* True if pTask->pUnpacked is pKey2 */
    const void *pKey1, int nKey1, /* Left side of comparison */
    const void *pKey2, int nKey2  /* Right side of comparison */
) {
  UnpackedRecord *r2 = pTask->pUnpacked;
  if (!*pbKey2Cached) {
    sqlite3VdbeRecordUnpack(pTask->pSorter->pKeyInfo, nKey2, pKey2, r2);
    *pbKey2Cached = 1;
  }
  return sqlite3VdbeRecordCompare(nKey1, pKey1, r2);
}

/*
** A specially optimized version of vdbeSorterCompare() that assumes that
** the first field of each key is a TEXT value and that the collation
** sequence to compare them with is BINARY.
*/
static int vdbeSorterCompareText(
    SortSubtask *pTask,           /* Subtask context (for pKeyInfo) */
    int *pbKey2Cached,            /* True if pTask->pUnpacked is pKey2 */
    const void *pKey1, int nKey1, /* Left side of comparison */
    const void *pKey2, int nKey2  /* Right side of comparison */
) {
  const u8 *const p1 = (const u8 *const)pKey1;
  const u8 *const p2 = (const u8 *const)pKey2;
  const u8 *const v1 = &p1[p1[0]]; /* Pointer to value 1 */
  const u8 *const v2 = &p2[p2[0]]; /* Pointer to value 2 */

  int n1;
  int n2;
  int res;

  getVarint32NR(&p1[1], n1);
  getVarint32NR(&p2[1], n2);
  res = memcmp(v1, v2, (MIN(n1, n2) - 13) / 2);
  if (res == 0) {
    res = n1 - n2;
  }

  if (res == 0) {
    if (pTask->pSorter->pKeyInfo->nKeyField > 1) {
      res = vdbeSorterCompareTail(pTask, pbKey2Cached, pKey1, nKey1, pKey2,
                                  nKey2);
    }
  } else {
    assert(!(pTask->pSorter->pKeyInfo->aSortFlags[0] & KEYINFO_ORDER_BIGNULL));
    if (pTask->pSorter->pKeyInfo->aSortFlags[0]) {
      res = res * -1;
    }
  }

  return res;
}

/*
** A specially optimized version of vdbeSorterCompare() that assumes that
** the first field of each key is an INTEGER value.
*/
static int vdbeSorterCompareInt(
    SortSubtask *pTask,           /* Subtask context (for pKeyInfo) */
    int *pbKey2Cached,            /* True if pTask->pUnpacked is pKey2 */
    const void *pKey1, int nKey1, /* Left side of comparison */
    const void *pKey2, int nKey2  /* Right side of comparison */
) {
  const u8 *const p1 = (const u8 *const)pKey1;
  const u8 *const p2 = (const u8 *const)pKey2;
  const int s1 = p1[1];            /* Left hand serial type */
  const int s2 = p2[1];            /* Right hand serial type */
  const u8 *const v1 = &p1[p1[0]]; /* Pointer to value 1 */
  const u8 *const v2 = &p2[p2[0]]; /* Pointer to value 2 */
  int res;                         /* Return value */

  assert((s1 > 0 && s1 < 7) || s1 == 8 || s1 == 9);
  assert((s2 > 0 && s2 < 7) || s2 == 8 || s2 == 9);

  if (s1 == s2) {
    /* The two values have the same sign. Compare using memcmp(). */
    static const u8 aLen[] = {0, 1, 2, 3, 4, 6, 8, 0, 0, 0};
    const u8 n = aLen[s1];
    int i;
    res = 0;
    for (i = 0; i < n; i++) {
      if ((res = v1[i] - v2[i]) != 0) {
        if (((v1[0] ^ v2[0]) & 0x80) != 0) {
          res = v1[0] & 0x80 ? -1 : +1;
        }
        break;
      }
    }
  } else if (s1 > 7 && s2 > 7) {
    res = s1 - s2;
  } else {
    if (s2 > 7) {
      res = +1;
    } else if (s1 > 7) {
      res = -1;
    } else {
      res = s1 - s2;
    }
    assert(res != 0);

    if (res > 0) {
      if (*v1 & 0x80) res = -1;
    } else {
      if (*v2 & 0x80) res = +1;
    }
  }

  if (res == 0) {
    if (pTask->pSorter->pKeyInfo->nKeyField > 1) {
      res = vdbeSorterCompareTail(pTask, pbKey2Cached, pKey1, nKey1, pKey2,
                                  nKey2);
    }
  } else if (pTask->pSorter->pKeyInfo->aSortFlags[0]) {
    assert(!(pTask->pSorter->pKeyInfo->aSortFlags[0] & KEYINFO_ORDER_BIGNULL));
    res = res * -1;
  }

  return res;
}

/*
** Initialize the temporary index cursor just opened as a sorter cursor.
**
** Usually, the sorter module uses the value of (pCsr->pKeyInfo->nKeyField)
** to determine the number of fields that should be compared from the
** records being sorted. However, if the value passed as argument nField
** is non-zero and the sorter is able to guarantee a stable sort, nField
** is used instead. This is used when sorting records for a CREATE INDEX
** statement. In this case, keys are always delivered to the sorter in
** order of the primary key, which happens to be make up the final part
** of the records being sorted. So if the sort is stable, there is never
** any reason to compare PK fields and they can be ignored for a small
** performance boost.
**
** The sorter can guarantee a stable sort when running in single-threaded
** mode, but not in multi-threaded mode.
**
** SQLITE_OK is returned if successful, or an SQLite error code otherwise.
*/
int sqlite3InMemSorterInit(
    sqlite3 *db,     /* Database connection (for malloc()) */
    int nField,      /* Number of key fields in each record */
    VdbeCursor *pCsr /* Cursor that holds the new sorter */
) {
  int pgsz;            /* Page size of main database */
  int i;               /* Used to iterate through aTask[] */
  VdbeSorter *pSorter; /* The new sorter */
  KeyInfo *pKeyInfo;   /* Copy of pCsr->pKeyInfo with db==0 */
  int szKeyInfo;       /* Size of pCsr->pKeyInfo in bytes */
  int sz;              /* Size of pSorter in bytes */
  int rc = SQLITE_OK;
#define nWorker 0

  assert(pCsr->pKeyInfo);
  assert(!pCsr->isEphemeral);
  assert(pCsr->eCurType == CURTYPE_SORTER);
  szKeyInfo =
      sizeof(KeyInfo) + (pCsr->pKeyInfo->nKeyField - 1) * sizeof(CollSeq *);
  sz = sizeof(VdbeSorter) + nWorker * sizeof(SortSubtask);

  pSorter = (VdbeSorter *)sqlite3DbMallocZero(db, sz + szKeyInfo);
  pCsr->uc.pSorter = pSorter;
  if (pSorter == 0) {
    rc = SQLITE_NOMEM_BKPT;
  } else {
    Btree *pBt = db->aDb[0].pBt;
    pSorter->pKeyInfo = pKeyInfo = (KeyInfo *)((u8 *)pSorter + sz);
    memcpy(pKeyInfo, pCsr->pKeyInfo, szKeyInfo);
    pKeyInfo->db = 0;
    if (nField && nWorker == 0) {
      pKeyInfo->nKeyField = nField;
    }
    sqlite3BtreeEnter(pBt);
    pSorter->pgsz = pgsz = sqlite3BtreeGetPageSize(pBt);
    sqlite3BtreeLeave(pBt);
    pSorter->nTask = nWorker + 1;
    pSorter->iPrev = (u8)(nWorker - 1);
    pSorter->bUseThreads = (pSorter->nTask > 1);
    pSorter->db = db;
    for (i = 0; i < pSorter->nTask; i++) {
      SortSubtask *pTask = &pSorter->aTask[i];
      pTask->pSorter = pSorter;
    }

    if (!sqlite3TempInMemory(db)) {
      i64 mxCache; /* Cache size in bytes*/
      u32 szPma = sqlite3GlobalConfig.szPma;
      pSorter->mnPmaSize = szPma * pgsz;

      mxCache = db->aDb[0].pSchema->cache_size;
      if (mxCache < 0) {
        /* A negative cache-size value C indicates that the cache is abs(C)
        ** KiB in size.  */
        mxCache = mxCache * -1024;
      } else {
        mxCache = mxCache * pgsz;
      }
      mxCache = MIN(mxCache, (1 << 29));
      pSorter->mxPmaSize = MAX(pSorter->mnPmaSize, (int)mxCache);

      pSorter->nMemory = pgsz;
      pSorter->list.aMemory = (u8 *)sqlite3Malloc(pgsz);
      if (!pSorter->list.aMemory) rc = SQLITE_NOMEM_BKPT;
    }

    if (pKeyInfo->nAllField < 13 &&
        (pKeyInfo->aColl[0] == 0 || pKeyInfo->aColl[0] == db->pDfltColl) &&
        (pKeyInfo->aSortFlags[0] & KEYINFO_ORDER_BIGNULL) == 0) {
      pSorter->typeMask = SORTER_TYPE_INTEGER | SORTER_TYPE_TEXT;
    }
  }

  return rc;
}
#undef nWorker /* Defined at the top of this function */

/*
** Free the list of sorted records starting at pRecord.
*/
static void vdbeSorterRecordFree(sqlite3 *db, SorterRecord *pRecord) {
  SorterRecord *p;
  SorterRecord *pNext;
  for (p = pRecord; p; p = pNext) {
    pNext = p->u.pNext;
    sqlite3DbFree(db, p);
  }
}

/*
** Free all resources owned by the object indicated by argument pTask. All
** fields of *pTask are zeroed before returning.
*/
static void vdbeSortSubtaskCleanup(sqlite3 *db, SortSubtask *pTask) {
  sqlite3DbFree(db, pTask->pUnpacked);
  {
    assert(pTask->list.aMemory == 0);
    vdbeSorterRecordFree(0, pTask->list.pList);
  }
  memset(pTask, 0, sizeof(SortSubtask));
}

#define vdbeSorterWorkDebug(x, y)
#define vdbeSorterRewindDebug(y)
#define vdbeSorterPopulateDebug(x, y)
#define vdbeSorterBlockDebug(x, y, z)
#define vdbeSorterJoinAll(x, rcin) (rcin)
#define vdbeSorterJoinThread(pTask) SQLITE_OK

/*
** Reset a sorting cursor back to its original empty state.
*/
void sqlite3InMemSorterReset(sqlite3 *db, VdbeSorter *pSorter) {
  int i;
  (void)vdbeSorterJoinAll(pSorter, SQLITE_OK);

  for (i = 0; i < pSorter->nTask; i++) {
    SortSubtask *pTask = &pSorter->aTask[i];
    vdbeSortSubtaskCleanup(db, pTask);
    pTask->pSorter = pSorter;
  }
  if (pSorter->list.aMemory == 0) {
    vdbeSorterRecordFree(0, pSorter->list.pList);
  }
  pSorter->list.pList = 0;
  pSorter->iMemory = 0;
  pSorter->mxKeysize = 0;
  sqlite3DbFree(db, pSorter->pUnpacked);
  pSorter->pUnpacked = 0;
}

/*
** Free any cursor components allocated by sqlite3VdbeSorterXXX routines.
*/
void sqlite3InMemSorterClose(sqlite3 *db, VdbeCursor *pCsr) {
  VdbeSorter *pSorter;
  assert(pCsr->eCurType == CURTYPE_SORTER);
  pSorter = pCsr->uc.pSorter;
  if (pSorter) {
    sqlite3VdbeSorterReset(db, pSorter);
    sqlite3_free(pSorter->list.aMemory);
    sqlite3DbFree(db, pSorter);
    pCsr->uc.pSorter = 0;
  }
}

/*
** If it has not already been allocated, allocate the UnpackedRecord
** structure at pTask->pUnpacked. Return SQLITE_OK if successful (or
** if no allocation was required), or SQLITE_NOMEM otherwise.
*/
static int vdbeSortAllocUnpacked(SortSubtask *pTask) {
  if (pTask->pUnpacked == 0) {
    pTask->pUnpacked = sqlite3VdbeAllocUnpackedRecord(pTask->pSorter->pKeyInfo);
    if (pTask->pUnpacked == 0) return SQLITE_NOMEM_BKPT;
    pTask->pUnpacked->nField = pTask->pSorter->pKeyInfo->nKeyField;
    pTask->pUnpacked->errCode = 0;
  }
  return SQLITE_OK;
}

/*
** Merge the two sorted lists p1 and p2 into a single list.
*/
static SorterRecord *vdbeSorterMerge(
    SortSubtask *pTask, /* Calling thread context */
    SorterRecord *p1,   /* First list to merge */
    SorterRecord *p2    /* Second list to merge */
) {
  SorterRecord *pFinal = 0;
  SorterRecord **pp = &pFinal;
  int bCached = 0;

  assert(p1 != 0 && p2 != 0);
  for (;;) {
    int res;
    res = pTask->xCompare(pTask, &bCached, SRVAL(p1), p1->nVal, SRVAL(p2),
                          p2->nVal);

    if (res <= 0) {
      *pp = p1;
      pp = &p1->u.pNext;
      p1 = p1->u.pNext;
      if (p1 == 0) {
        *pp = p2;
        break;
      }
    } else {
      *pp = p2;
      pp = &p2->u.pNext;
      p2 = p2->u.pNext;
      bCached = 0;
      if (p2 == 0) {
        *pp = p1;
        break;
      }
    }
  }
  return pFinal;
}

/*
** Return the SorterCompare function to compare values collected by the
** sorter object passed as the only argument.
*/
static SorterCompare vdbeSorterGetCompare(VdbeSorter *p) {
  if (p->typeMask == SORTER_TYPE_INTEGER) {
    return vdbeSorterCompareInt;
  } else if (p->typeMask == SORTER_TYPE_TEXT) {
    return vdbeSorterCompareText;
  }
  return vdbeSorterCompare;
}

/*
** Sort the linked list of records headed at pTask->pList. Return
** SQLITE_OK if successful, or an SQLite error code (i.e. SQLITE_NOMEM) if
** an error occurs.
*/
static int vdbeSorterSort(SortSubtask *pTask, SorterList *pList) {
  int i;
  SorterRecord *p;
  int rc;
  SorterRecord *aSlot[64];

  rc = vdbeSortAllocUnpacked(pTask);
  if (rc != SQLITE_OK) return rc;

  p = pList->pList;
  pTask->xCompare = vdbeSorterGetCompare(pTask->pSorter);
  memset(aSlot, 0, sizeof(aSlot));

  while (p) {
    SorterRecord *pNext;
    if ((u8 *)p == pList->aMemory) {
      pNext = 0;
    } else {
      assert(p->u.iNext < sqlite3MallocSize(pList->aMemory));
      pNext = (SorterRecord *)&pList->aMemory[p->u.iNext];
    }

    p->u.pNext = 0;
    for (i = 0; aSlot[i]; i++) {
      p = vdbeSorterMerge(pTask, p, aSlot[i]);
      aSlot[i] = 0;
    }
    aSlot[i] = p;
    p = pNext;
  }

  p = 0;
  for (i = 0; i < ArraySize(aSlot); i++) {
    if (aSlot[i] == 0) continue;
    p = p ? vdbeSorterMerge(pTask, p, aSlot[i]) : aSlot[i];
  }
  pList->pList = p;

  assert(pTask->pUnpacked->errCode == SQLITE_OK ||
         pTask->pUnpacked->errCode == SQLITE_NOMEM);
  return pTask->pUnpacked->errCode;
}

/*
** Add a record to the sorter.
*/
int sqlite3InMemSorterWrite(const VdbeCursor *pCsr, /* Sorter cursor */
                            Mem *pVal /* Memory cell containing record */
) {
  VdbeSorter *pSorter;
  int rc = SQLITE_OK; /* Return Code */
  SorterRecord *pNew; /* New list element */
  int nReq;           /* Bytes of memory required */
  int t;              /* serial type of first record field */

  assert(pCsr->eCurType == CURTYPE_SORTER);
  pSorter = pCsr->uc.pSorter;
  getVarint32NR((const u8 *)&pVal->z[1], t);
  if (t > 0 && t < 10 && t != 7) {
    pSorter->typeMask &= SORTER_TYPE_INTEGER;
  } else if (t > 10 && (t & 0x01)) {
    pSorter->typeMask &= SORTER_TYPE_TEXT;
  } else {
    pSorter->typeMask = 0;
  }

  assert(pSorter);

  /* Figure out whether or not the current contents of memory should be
  ** flushed to a PMA before continuing. If so, do so.
  **
  ** If using the single large allocation mode (pSorter->aMemory!=0), then
  ** flush the contents of memory to a new PMA if (a) at least one value is
  ** already in memory and (b) the new value will not fit in memory.
  **
  ** Or, if using separate allocations for each record, flush the contents
  ** of memory to a PMA if either of the following are true:
  **
  **   * The total memory allocated for the in-memory list is greater
  **     than (page-size * cache-size), or
  **
  **   * The total memory allocated for the in-memory list is greater
  **     than (page-size * 10) and sqlite3HeapNearlyFull() returns true.
  */
  nReq = pVal->n + sizeof(SorterRecord);

  int nMin = pSorter->iMemory + nReq;

  if (nMin > pSorter->nMemory) {
    u8 *aNew;
    sqlite3_int64 nNew = 2 * (sqlite3_int64)pSorter->nMemory;
    int iListOff = -1;
    if (pSorter->list.pList) {
      iListOff = (u8 *)pSorter->list.pList - pSorter->list.aMemory;
    }
    while (nNew < nMin) nNew = nNew * 2;
    if (nNew > pSorter->mxPmaSize) nNew = pSorter->mxPmaSize;
    if (nNew < nMin) nNew = nMin;
    aNew = sqlite3Realloc(pSorter->list.aMemory, nNew);
    if (!aNew) return SQLITE_NOMEM_BKPT;
    if (iListOff >= 0) {
      pSorter->list.pList = (SorterRecord *)&aNew[iListOff];
    }
    pSorter->list.aMemory = aNew;
    pSorter->nMemory = nNew;
  }

  pNew = (SorterRecord *)&pSorter->list.aMemory[pSorter->iMemory];
  pSorter->iMemory += ROUND8(nReq);
  if (pSorter->list.pList) {
    pNew->u.iNext = (int)((u8 *)(pSorter->list.pList) - pSorter->list.aMemory);
  }

  memcpy(SRVAL(pNew), pVal->z, pVal->n);
  pNew->nVal = pVal->n;
  pSorter->list.pList = pNew;

  return rc;
}

#define INCRINIT_NORMAL 0
#define INCRINIT_TASK 1
#define INCRINIT_ROOT 2

/*
** Once the sorter has been populated by calls to sqlite3VdbeSorterWrite,
** this function is called to prepare for iterating through the records
** in sorted order.
*/
int sqlite3InMemSorterRewind(const VdbeCursor *pCsr, int *pbEof) {
  VdbeSorter *pSorter;
  int rc = SQLITE_OK; /* Return code */

  pSorter = pCsr->uc.pSorter;
  /* If no data has been written to disk, then do not do so now. Instead,
  ** sort the VdbeSorter.pRecord list. The vdbe layer will read data directly
  ** from the in-memory list.  */
  if (pSorter->list.pList) {
    *pbEof = 0;
    rc = vdbeSorterSort(&pSorter->aTask[0], &pSorter->list);
  } else {
    *pbEof = 1;
  }
  return rc;
}

/*
** Advance to the next element in the sorter.  Return value:
**
**    SQLITE_OK     success
**    SQLITE_DONE   end of data
**    otherwise     some kind of error.
*/
int sqlite3InMemSorterNext(sqlite3 *db, const VdbeCursor *pCsr) {
  VdbeSorter *pSorter;
  int rc; /* Return code */

  assert(pCsr->eCurType == CURTYPE_SORTER);
  pSorter = pCsr->uc.pSorter;
  SorterRecord *pFree = pSorter->list.pList;
  pSorter->list.pList = pFree->u.pNext;
  pFree->u.pNext = 0;
  rc = pSorter->list.pList ? SQLITE_OK : SQLITE_DONE;
  return rc;
}

/*
** Return a pointer to a buffer owned by the sorter that contains the
** current key.
*/
static void *vdbeSorterRowkey(const VdbeSorter *pSorter, /* Sorter object */
                              int *pnKey /* OUT: Size of current key in bytes */
) {
  void *pKey;
  *pnKey = pSorter->list.pList->nVal;
  pKey = SRVAL(pSorter->list.pList);
  return pKey;
}

/*
** Copy the current sorter key into the memory cell pOut.
*/
int sqlite3InMemSorterRowkey(const VdbeCursor *pCsr, Mem *pOut) {
  VdbeSorter *pSorter;
  void *pKey;
  int nKey; /* Sorter key to copy into pOut */

  assert(pCsr->eCurType == CURTYPE_SORTER);
  pSorter = pCsr->uc.pSorter;
  pKey = vdbeSorterRowkey(pSorter, &nKey);
  if (sqlite3VdbeMemClearAndResize(pOut, nKey)) {
    return SQLITE_NOMEM_BKPT;
  }
  pOut->n = nKey;
  MemSetTypeFlag(pOut, MEM_Blob);
  memcpy(pOut->z, pKey, nKey);

  return SQLITE_OK;
}

/*
** Compare the key in memory cell pVal with the key that the sorter cursor
** passed as the first argument currently points to. For the purposes of
** the comparison, ignore the rowid field at the end of each record.
**
** If the sorter cursor key contains any NULL values, consider it to be
** less than pVal. Even if pVal also contains NULL values.
**
** If an error occurs, return an SQLite error code (i.e. SQLITE_NOMEM).
** Otherwise, set *pRes to a negative, zero or positive value if the
** key in pVal is smaller than, equal to or larger than the current sorter
** key.
**
** This routine forms the core of the OP_SorterCompare opcode, which in
** turn is used to verify uniqueness when constructing a UNIQUE INDEX.
*/
int sqlite3InMemSorterCompare(
    const VdbeCursor *pCsr, /* Sorter cursor */
    Mem *pVal,              /* Value to compare to current sorter key */
    int nKeyCol,            /* Compare this many columns */
    int *pRes               /* OUT: Result of comparison */
) {
  VdbeSorter *pSorter;
  UnpackedRecord *r2;
  KeyInfo *pKeyInfo;
  int i;
  void *pKey;
  int nKey; /* Sorter key to compare pVal with */

  assert(pCsr->eCurType == CURTYPE_SORTER);
  pSorter = pCsr->uc.pSorter;
  r2 = pSorter->pUnpacked;
  pKeyInfo = pCsr->pKeyInfo;
  if (r2 == 0) {
    r2 = pSorter->pUnpacked = sqlite3VdbeAllocUnpackedRecord(pKeyInfo);
    if (r2 == 0) return SQLITE_NOMEM_BKPT;
    r2->nField = nKeyCol;
  }
  assert(r2->nField == nKeyCol);

  pKey = vdbeSorterRowkey(pSorter, &nKey);
  sqlite3VdbeRecordUnpack(pKeyInfo, nKey, pKey, r2);
  for (i = 0; i < nKeyCol; i++) {
    if (r2->aMem[i].flags & MEM_Null) {
      *pRes = -1;
      return SQLITE_OK;
    }
  }

  *pRes = sqlite3VdbeRecordCompare(pVal->n, pVal->z, r2);
  return SQLITE_OK;
}
