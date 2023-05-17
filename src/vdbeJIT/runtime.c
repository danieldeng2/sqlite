#include "runtime.h"

#include "inMemorySort.h"
#include "time.h"

#define isSorter(x) ((x)->eCurType == CURTYPE_SORTER)

typedef int (*jitProgram)();

__attribute__((optnone))
int sqlite3VdbeExecJIT(Vdbe *p) {
  int rc;

  if (p->jitCode == NULL) {
    do {
      rc = sqlite3VdbeExec(p);
    } while (rc == 2000);

    return rc;
  }

  do {
    rc = ((jitProgram)p->jitCode)();
    if (rc > 100000) printf("TODO: Implement OP %d\n", rc - 100000);
    if (rc > 1000) rc = sqlite3VdbeExec(p);
  } while (rc > 1000);
  
  return rc;
}

void execOpAdd(Mem *pIn1, Mem *pIn2, Mem *pOut) {
  u16 flag;
  if ((pIn1->flags & pIn2->flags & MEM_Int) != 0) {
    pOut->u.i = pIn1->u.i + pIn2->u.i;
    flag = MEM_Int;
  } else {
    double p1 = (pIn1->flags & MEM_Real) ? pIn1->u.r : (double)pIn1->u.i;
    double p2 = (pIn2->flags & MEM_Real) ? pIn2->u.r : (double)pIn2->u.i;
    pOut->u.r = p2 + p1;
    flag = MEM_Real;
  }
  pOut->flags = (pOut->flags & ~(MEM_TypeMask | MEM_Zero)) | flag;
}
void execOpSubtract(Mem *pIn1, Mem *pIn2, Mem *pOut) {
  u16 flag;
  if ((pIn1->flags & pIn2->flags & MEM_Int) != 0) {
    pOut->u.i = pIn2->u.i - pIn1->u.i;
    flag = MEM_Int;
  } else {
    double p1 = (pIn1->flags & MEM_Real) ? pIn1->u.r : (double)pIn1->u.i;
    double p2 = (pIn2->flags & MEM_Real) ? pIn2->u.r : (double)pIn2->u.i;
    pOut->u.r = p2 - p1;
    flag = MEM_Real;
  }
  pOut->flags = (pOut->flags & ~(MEM_TypeMask | MEM_Zero)) | flag;
}

void execOpMultiply(Mem *pIn1, Mem *pIn2, Mem *pOut) {
  u16 flag;
  if ((pIn1->flags & pIn2->flags & MEM_Int) != 0) {
    pOut->u.i = pIn1->u.i * pIn2->u.i;
    flag = MEM_Int;
  } else {
    double p1 = (pIn1->flags & MEM_Real) ? pIn1->u.r : (double)pIn1->u.i;
    double p2 = (pIn2->flags & MEM_Real) ? pIn2->u.r : (double)pIn2->u.i;
    pOut->u.r = p2 * p1;
    flag = MEM_Real;
  }
  pOut->flags = (pOut->flags & ~(MEM_TypeMask | MEM_Zero)) | flag;
}

int execOpenReadWrite(Vdbe *p, Op *pOp) {
  int nField;
  KeyInfo *pKeyInfo;
  u32 p2;
  int iDb;
  int wrFlag;
  Btree *pX;
  VdbeCursor *pCur;
  Db *pDb;
  int rc = 0;
  sqlite3 *db = p->db;
  Mem *aMem = p->aMem;

  nField = 0;
  pKeyInfo = 0;
  p2 = (u32)pOp->p2;
  iDb = pOp->p3;
  assert(iDb >= 0 && iDb < db->nDb);
  assert(DbMaskTest(p->btreeMask, iDb));
  pDb = &db->aDb[iDb];
  pX = pDb->pBt;
  assert(pX != 0);
  if (pOp->opcode == OP_OpenWrite) {
    assert(OPFLAG_FORDELETE == BTREE_FORDELETE);
    wrFlag = BTREE_WRCSR | (pOp->p5 & OPFLAG_FORDELETE);
    assert(sqlite3SchemaMutexHeld(db, iDb, 0));
    if (pDb->pSchema->file_format < p->minWriteFileFormat) {
      p->minWriteFileFormat = pDb->pSchema->file_format;
    }
  } else {
    wrFlag = 0;
  }
  if (pOp->p5 & OPFLAG_P2ISREG) {
    assert(p2 > 0);
    assert(p2 <= (u32)(p->nMem + 1 - p->nCursor));
    assert(pOp->opcode == OP_OpenWrite);
    Mem *pIn2 = &aMem[p2];
    assert(memIsValid(pIn2));
    assert((pIn2->flags & MEM_Int) != 0);
    sqlite3VdbeMemIntegerify(pIn2);
    p2 = (int)pIn2->u.i;
    /* The p2 value always comes from a prior OP_CreateBtree opcode and
    ** that opcode will always set the p2 value to 2 or more or else fail.
    ** If there were a failure, the prepared statement would have halted
    ** before reaching this instruction. */
    assert(p2 >= 2);
  }
  if (pOp->p4type == P4_KEYINFO) {
    pKeyInfo = pOp->p4.pKeyInfo;
    assert(pKeyInfo->enc == ENC(db));
    assert(pKeyInfo->db == db);
    nField = pKeyInfo->nAllField;
  } else if (pOp->p4type == P4_INT32) {
    nField = pOp->p4.i;
  }
  assert(pOp->p1 >= 0);
  assert(nField >= 0);
  testcase(nField == 0); /* Table with INTEGER PRIMARY KEY and nothing else */
  pCur = allocateCursor(p, pOp->p1, nField, CURTYPE_BTREE);
  pCur->iDb = iDb;
  pCur->nullRow = 1;
  pCur->isOrdered = 1;
  pCur->pgnoRoot = p2;
#ifdef SQLITE_DEBUG
  pCur->wrFlag = wrFlag;
#endif
  rc = sqlite3BtreeCursor(pX, p2, wrFlag, pKeyInfo, pCur->uc.pCursor);
  pCur->pKeyInfo = pKeyInfo;
  /* Set the VdbeCursor.isTable variable. Previous versions of
  ** SQLite used to check if the root-page flags were sane at this point
  ** and report database corruption if they were not, but this check has
  ** since moved into the btree layer.  */
  pCur->isTable = pOp->p4type != P4_KEYINFO;

open_cursor_set_hints:
  assert(OPFLAG_BULKCSR == BTREE_BULKLOAD);
  assert(OPFLAG_SEEKEQ == BTREE_SEEK_EQ);
  testcase(pOp->p5 & OPFLAG_BULKCSR);
  testcase(pOp->p2 & OPFLAG_SEEKEQ);
  sqlite3BtreeCursorHintFlags(pCur->uc.pCursor,
                              (pOp->p5 & (OPFLAG_BULKCSR | OPFLAG_SEEKEQ)));
  return rc;
}

int execOpRewind(Vdbe *p, Op *pOp) {
  VdbeCursor *pC;
  BtCursor *pCrsr;
  int res;
  int rc;

  assert(pOp->p1 >= 0 && pOp->p1 < p->nCursor);
  assert(pOp->p5 == 0);
  pC = p->apCsr[pOp->p1];
  assert(pC != 0);
  res = 1;
#ifdef SQLITE_DEBUG
  pC->seekOp = OP_Rewind;
#endif
  if (isSorter(pC)) {
    rc = sqlite3InMemSorterRewind(pC, &res);
  } else {
    assert(pC->eCurType == CURTYPE_BTREE);
    pCrsr = pC->uc.pCursor;
    assert(pCrsr);
    rc = sqlite3BtreeFirst(pCrsr, &res);
    pC->deferredMoveto = 0;
    pC->cacheStatus = CACHE_STALE;
  }
  pC->nullRow = (u8)res;
  return res;
}

int execOpColumn(Vdbe *p, Op *pOp) {
  u32 p2;            /* column number to retrieve */
  VdbeCursor *pC;    /* The VDBE cursor */
  BtCursor *pCrsr;   /* The B-Tree cursor corresponding to pC */
  u32 *aOffset;      /* aOffset[i] is offset to start of data for i-th column */
  int len;           /* The length of the serialized data for the column */
  int i;             /* Loop counter */
  Mem *pDest;        /* Where to write the extracted value */
  Mem sMem;          /* For storing the record being decoded */
  const u8 *zData;   /* Part of the record being decoded */
  const u8 *zHdr;    /* Next unparsed byte of the header */
  const u8 *zEndHdr; /* Pointer to first byte after the header */
  u64 offset64;      /* 64-bit offset */
  u32 t;             /* A type code from the record header */
  Mem *pReg;         /* PseudoTable input register */
  Mem *aMem = p->aMem;
  int rc = 0;
  sqlite3 *db = p->db;
  u8 encoding = ENC(db);

  pC = p->apCsr[pOp->p1];
  p2 = (u32)pOp->p2;

op_column_restart:
  aOffset = pC->aOffset;

  if (pC->cacheStatus != p->cacheCtr) { /*OPTIMIZATION-IF-FALSE*/
    // branch 0: pC->cacheStatus != p->cacheCtr
    p->traces[(int)(pOp - p->aOp)][0]++;

    if (pC->nullRow) {
      if (pC->eCurType == CURTYPE_PSEUDO && pC->seekResult > 0) {
        /* For the special case of as pseudo-cursor, the seekResult field
        ** identifies the register that holds the record */
        pReg = &aMem[pC->seekResult];
        assert(pReg->flags & MEM_Blob);
        assert(memIsValid(pReg));
        pC->payloadSize = pC->szRow = pReg->n;
        pC->aRow = (u8 *)pReg->z;
      } else {
        pDest = &aMem[pOp->p3];
        sqlite3VdbeMemSetNull(pDest);
        return rc;
      }
    } else {
      pCrsr = pC->uc.pCursor;
      if (pC->deferredMoveto) {
        u32 iMap;
        if (pC->ub.aAltMap && (iMap = pC->ub.aAltMap[1 + p2]) > 0) {
          pC = pC->pAltCursor;
          p2 = iMap - 1;
          goto op_column_restart;
        }
        rc = sqlite3VdbeFinishMoveto(pC);
      } else if (sqlite3BtreeCursorHasMoved(pCrsr)) {
        rc = sqlite3VdbeHandleMovedCursor(pC);
        goto op_column_restart;
      }
      pC->payloadSize = sqlite3BtreePayloadSize(pCrsr);
      pC->aRow = (const u8 *)sqlite3BtreePayloadFetch(pCrsr, &pC->szRow);
    }
    pC->cacheStatus = p->cacheCtr;
    if ((aOffset[0] = pC->aRow[0]) < 0x80) {
      pC->iHdrOffset = 1;
    } else {
      pC->iHdrOffset = sqlite3GetVarint32(pC->aRow, aOffset);
    }
    pC->nHdrParsed = 0;

    if (pC->szRow < aOffset[0]) { /*OPTIMIZATION-IF-FALSE*/
      /* pC->aRow does not have to hold the entire row, but it does at least
      ** need to cover the header of the record.  If pC->aRow does not contain
      ** the complete header, then set it to zero, forcing the header to be
      ** dynamically allocated. */
      pC->aRow = 0;
      pC->szRow = 0;

      /* Make sure a corrupt database has not given us an oversize header.
      ** Do this now to avoid an oversize memory allocation.
      **
      ** Type entries can be between 1 and 5 bytes each.  But 4 and 5 byte
      ** types use so much data space that there can only be 4096 and 32 of
      ** them, respectively.  So the maximum header length results from a
      ** 3-byte type for each of the maximum of 32768 columns plus three
      ** extra bytes for the header length itself.  32768*3 + 3 = 98307.
      */
      if (aOffset[0] > 98307 || aOffset[0] > pC->payloadSize) {
        return rc;
      }
    } else {
      /* This is an optimization.  By skipping over the first few tests
      ** (ex: pC->nHdrParsed<=p2) in the next section, we achieve a
      ** measurable performance gain.
      **
      ** This branch is taken even if aOffset[0]==0.  Such a record is never
      ** generated by SQLite, and could be considered corruption, but we
      ** accept it for historical reasons.  When aOffset[0]==0, the code this
      ** branch jumps to reads past the end of the record, but never more
      ** than a few bytes.  Even if the record occurs at the end of the page
      ** content area, the "page header" comes after the page content and so
      ** this overread is harmless.  Similar overreads can occur for a corrupt
      ** database file.
      */
      zData = pC->aRow;
      assert(pC->nHdrParsed <= p2); /* Conditional skipped */
      testcase(aOffset[0] == 0);
      goto op_column_read_header;
    }
  }

  /* Make sure at least the first p2+1 entries of the header have been
  ** parsed and valid information is in aOffset[] and pC->aType[].
  */
  if (pC->nHdrParsed <= p2) {
    // 2nd branch: pC->nHdrParsed <= p2
    p->traces[(int)(pOp - p->aOp)][2]++;

    /* If there is more header available for parsing in the record, try
    ** to extract additional fields up through the p2+1-th field
    */
    if (pC->iHdrOffset < aOffset[0]) {
      /* Make sure zData points to enough of the record to cover the header. */
      if (pC->aRow == 0) {
        memset(&sMem, 0, sizeof(sMem));
        rc = sqlite3VdbeMemFromBtreeZeroOffset(pC->uc.pCursor, aOffset[0],
                                               &sMem);
        zData = (u8 *)sMem.z;
      } else {
        zData = pC->aRow;
      }

      /* Fill in pC->aType[i] and aOffset[i] values through the p2-th field. */
    op_column_read_header:
      i = pC->nHdrParsed;
      offset64 = aOffset[i];
      zHdr = zData + pC->iHdrOffset;
      zEndHdr = zData + aOffset[0];
      testcase(zHdr >= zEndHdr);
      do {
        if ((pC->aType[i] = t = zHdr[0]) < 0x80) {
          zHdr++;
          offset64 += sqlite3VdbeOneByteSerialTypeLen(t);
        } else {
          zHdr += sqlite3GetVarint32(zHdr, &t);
          pC->aType[i] = t;
          offset64 += sqlite3VdbeSerialTypeLen(t);
        }
        aOffset[++i] = (u32)(offset64 & 0xffffffff);
      } while ((u32)i <= p2 && zHdr < zEndHdr);

      /* The record is corrupt if any of the following are true:
      ** (1) the bytes of the header extend past the declared header size
      ** (2) the entire header was used but not all data was used
      ** (3) the end of the data extends beyond the end of the record.
      */
      if ((zHdr >= zEndHdr &&
           (zHdr > zEndHdr || offset64 != pC->payloadSize)) ||
          (offset64 > pC->payloadSize)) {
        if (aOffset[0] == 0) {
          i = 0;
          zHdr = zEndHdr;
        } else {
          if (pC->aRow == 0) sqlite3VdbeMemRelease(&sMem);
          return rc;
        }
      }

      pC->nHdrParsed = i;
      pC->iHdrOffset = (u32)(zHdr - zData);
      if (pC->aRow == 0) sqlite3VdbeMemRelease(&sMem);
    } else {
      t = 0;
    }

    /* If after trying to extract new entries from the header, nHdrParsed is
    ** still not up to p2, that means that the record has fewer than p2
    ** columns.  So the result will be either the default value or a NULL.
    */
    if (pC->nHdrParsed <= p2) {
      pDest = &aMem[pOp->p3];
      if (pOp->p4type == P4_MEM) {
        sqlite3VdbeMemShallowCopy(pDest, pOp->p4.pMem, MEM_Static);
      } else {
        sqlite3VdbeMemSetNull(pDest);
      }
      return rc;
    }
  } else {
    // branch 3: pC->nHdrParsed > p2
    p->traces[(int)(pOp - p->aOp)][3]++;
    t = pC->aType[p2];
  }

  /* Extract the content for the p2+1-th column.  Control can only
  ** reach this point if aOffset[p2], aOffset[p2+1], and pC->aType[p2] are
  ** all valid.
  */
  pDest = &aMem[pOp->p3];
  if (VdbeMemDynamic(pDest)) {
    sqlite3VdbeMemSetNull(pDest);
  }

  // branch 4: pC->szRow >= aOffset[p2 + 1]
  p->traces[(int)(pOp - p->aOp)][4]++;

  /* This is the common case where the desired content fits on the original
  ** page - where the content is not on an overflow page */
  zData = pC->aRow + aOffset[p2];
  if (t < 12) {
    sqlite3VdbeSerialGet(zData, t, pDest);
  } else {
    /* If the column value is a string, we need a persistent value, not
    ** a MEM_Ephem value.  This branch is a fast short-cut that is equivalent
    ** to calling sqlite3VdbeSerialGet() and sqlite3VdbeDeephemeralize().
    */
    static const u16 aFlag[] = {MEM_Blob, MEM_Str | MEM_Term};
    pDest->n = len = (t - 12) / 2;
    pDest->enc = encoding;
    if (pDest->szMalloc < len + 2) {
      pDest->flags = MEM_Null;
      sqlite3VdbeMemGrow(pDest, len + 2, 0);
    } else {
      pDest->z = pDest->zMalloc;
    }
    memcpy(pDest->z, zData, len);
    pDest->z[len] = 0;
    pDest->z[len + 1] = 0;
    pDest->flags = aFlag[t & 1];
  }

  return rc;
}

int execOpFunction(Vdbe *p, Op *pOp) {
  int i;
  sqlite3_context *pCtx;
  int rc = 0;

  assert(pOp->p4type == P4_FUNCCTX);
  pCtx = pOp->p4.pCtx;

  /* If this function is inside of a trigger, the register array in aMem[]
  ** might change from one evaluation to the next.  The next block of code
  ** checks to see if the register array has changed, and if so it
  ** reinitializes the relavant parts of the sqlite3_context object */
  Mem *pOut = &p->aMem[pOp->p3];
  if (pCtx->pOut != pOut) {
    pCtx->pVdbe = p;
    pCtx->pOut = pOut;
    pCtx->enc = p->db->enc;
    for (i = pCtx->argc - 1; i >= 0; i--) pCtx->argv[i] = &p->aMem[pOp->p2 + i];
  }
  assert(pCtx->pVdbe == p);

#ifdef SQLITE_DEBUG
  for (i = 0; i < pCtx->argc; i++) {
    assert(memIsValid(pCtx->argv[i]));
    REGISTER_TRACE(pOp->p2 + i, pCtx->argv[i]);
  }
#endif
  MemSetTypeFlag(pOut, MEM_Null);
  assert(pCtx->isError == 0);
  (*pCtx->pFunc->xSFunc)(pCtx, pCtx->argc, pCtx->argv); /* IMP: R-24505-23230 */

  /* If the function returned an error, throw an exception */
  if (pCtx->isError) {
    if (pCtx->isError > 0) {
      sqlite3VdbeError(p, "%s", sqlite3_value_text(pOut));
      rc = pCtx->isError;
    }
    sqlite3VdbeDeleteAuxData(p->db, &p->pAuxData, pCtx->iOp, pOp->p1);
    pCtx->isError = 0;
  }

  assert((pOut->flags & MEM_Str) == 0 || pOut->enc == p->db->enc ||
         db->mallocFailed);
  assert(!sqlite3VdbeMemTooBig(pOut));
  return rc;
}

void execAggrStepOne(Vdbe *p, Op *pOp) {
  int i;
  sqlite3_context *pCtx;
  Mem *pMem;

  assert(pOp->p4type == P4_FUNCCTX);
  pCtx = pOp->p4.pCtx;
  pMem = &p->aMem[pOp->p3];

  /* If this function is inside of a trigger, the register array in aMem[]
  ** might change from one evaluation to the next.  The next block of code
  ** checks to see if the register array has changed, and if so it
  ** reinitializes the relavant parts of the sqlite3_context object */
  if (pCtx->pMem != pMem) {
    pCtx->pMem = pMem;
    for (i = pCtx->argc - 1; i >= 0; i--) pCtx->argv[i] = &p->aMem[pOp->p2 + i];
  }

  pMem->n++;
  (pCtx->pFunc->xSFunc)(pCtx, pCtx->argc, pCtx->argv);
}

void execOpMakeRecord(Vdbe *p, Op *pOp) {
  Mem *pRec;       /* The new record */
  u64 nData;       /* Number of bytes of data space */
  int nHdr;        /* Number of bytes of header space */
  i64 nByte;       /* Data space required for this record */
  i64 nZero;       /* Number of zero bytes at the end of the record */
  int nVarint;     /* Number of bytes in a varint */
  u32 serial_type; /* Type field */
  Mem *pData0;     /* First field to be combined into the record */
  Mem *pLast;      /* Last field of the record */
  int nField;      /* Number of fields in the record */
  char *zAffinity; /* The affinity string for the record */
  u32 len;         /* Length of a field */
  u8 *zHdr;        /* Where to write next byte of the header */
  u8 *zPayload;    /* Where to write next byte of the payload */

  /* Assuming the record contains N fields, the record format looks
  ** like this:
  **
  ** ------------------------------------------------------------------------
  ** | hdr-size | type 0 | type 1 | ... | type N-1 | data0 | ... | data N-1 |
  ** ------------------------------------------------------------------------
  **
  ** Data(0) is taken from register P1.  Data(1) comes from register P1+1
  ** and so forth.
  **
  ** Each type field is a varint representing the serial type of the
  ** corresponding data element (see sqlite3VdbeSerialType()). The
  ** hdr-size field is also a varint which is the offset from the beginning
  ** of the record to data0.
  */
  nData = 0; /* Number of bytes of data space */
  nHdr = 0;  /* Number of bytes of header space */
  nZero = 0; /* Number of zero bytes at the end of the record */
  nField = pOp->p1;
  zAffinity = pOp->p4.z;
  assert(nField > 0 && pOp->p2 > 0 &&
         pOp->p2 + nField <= (p->nMem + 1 - p->nCursor) + 1);
  pData0 = &p->aMem[nField];
  nField = pOp->p2;
  pLast = &pData0[nField - 1];

  /* Identify the output register */
  assert(pOp->p3 < pOp->p1 || pOp->p3 >= pOp->p1 + pOp->p2);
  Mem *pOut = &p->aMem[pOp->p3];

  /* Loop through the elements that will make up the record to figure
  ** out how much space is required for the new record.  After this loop,
  ** the Mem.uTemp field of each term should hold the serial-type that will
  ** be used for that term in the generated record:
  **
  **   Mem.uTemp value    type
  **   ---------------    ---------------
  **      0               NULL
  **      1               1-byte signed integer
  **      2               2-byte signed integer
  **      3               3-byte signed integer
  **      4               4-byte signed integer
  **      5               6-byte signed integer
  **      6               8-byte signed integer
  **      7               IEEE float
  **      8               Integer constant 0
  **      9               Integer constant 1
  **     10,11            reserved for expansion
  **    N>=12 and even    BLOB
  **    N>=13 and odd     text
  **
  ** The following additional values are computed:
  **     nHdr        Number of bytes needed for the record header
  **     nData       Number of bytes of data space needed for the record
  **     nZero       Zero bytes at the end of the record
  */
  pRec = pLast;
  do {
    assert(memIsValid(pRec));
    if (pRec->flags & MEM_Null) {
      if (pRec->flags & MEM_Zero) {
        /* Values with MEM_Null and MEM_Zero are created by xColumn virtual
        ** table methods that never invoke sqlite3_result_xxxxx() while
        ** computing an unchanging column value in an UPDATE statement.
        ** Give such values a special internal-use-only serial-type of 10
        ** so that they can be passed through to xUpdate and have
        ** a true sqlite3_value_nochange(). */
#ifndef SQLITE_ENABLE_NULL_TRIM
        assert(pOp->p5 == OPFLAG_NOCHNG_MAGIC || CORRUPT_DB);
#endif
        pRec->uTemp = 10;
      } else {
        pRec->uTemp = 0;
      }
      nHdr++;
    } else if (pRec->flags & (MEM_Int | MEM_IntReal)) {
      /* Figure out whether to use 1, 2, 4, 6 or 8 bytes. */
      i64 i = pRec->u.i;
      u64 uu;
      testcase(pRec->flags & MEM_Int);
      testcase(pRec->flags & MEM_IntReal);
      if (i < 0) {
        uu = ~i;
      } else {
        uu = i;
      }
      nHdr++;
      testcase(uu == 127);
      testcase(uu == 128);
      testcase(uu == 32767);
      testcase(uu == 32768);
      testcase(uu == 8388607);
      testcase(uu == 8388608);
      testcase(uu == 2147483647);
      testcase(uu == 2147483648LL);
      testcase(uu == 140737488355327LL);
      testcase(uu == 140737488355328LL);
      if (uu <= 127) {
        if ((i & 1) == i && p->minWriteFileFormat >= 4) {
          pRec->uTemp = 8 + (u32)uu;
        } else {
          nData++;
          pRec->uTemp = 1;
        }
      } else if (uu <= 32767) {
        nData += 2;
        pRec->uTemp = 2;
      } else if (uu <= 8388607) {
        nData += 3;
        pRec->uTemp = 3;
      } else if (uu <= 2147483647) {
        nData += 4;
        pRec->uTemp = 4;
      } else if (uu <= 140737488355327LL) {
        nData += 6;
        pRec->uTemp = 5;
      } else {
        nData += 8;
        if (pRec->flags & MEM_IntReal) {
          /* If the value is IntReal and is going to take up 8 bytes to store
          ** as an integer, then we might as well make it an 8-byte floating
          ** point value */
          pRec->u.r = (double)pRec->u.i;
          pRec->flags &= ~MEM_IntReal;
          pRec->flags |= MEM_Real;
          pRec->uTemp = 7;
        } else {
          pRec->uTemp = 6;
        }
      }
    } else if (pRec->flags & MEM_Real) {
      nHdr++;
      nData += 8;
      pRec->uTemp = 7;
    } else {
      assert(db->mallocFailed || pRec->flags & (MEM_Str | MEM_Blob));
      assert(pRec->n >= 0);
      len = (u32)pRec->n;
      serial_type = (len * 2) + 12 + ((pRec->flags & MEM_Str) != 0);
      if (pRec->flags & MEM_Zero) {
        serial_type += pRec->u.nZero * 2;
        if (nData) {
          len += pRec->u.nZero;
        } else {
          nZero += pRec->u.nZero;
        }
      }
      nData += len;
      nHdr += sqlite3VarintLen(serial_type);
      pRec->uTemp = serial_type;
    }
    if (pRec == pData0) break;
    pRec--;
  } while (1);

  /* EVIDENCE-OF: R-22564-11647 The header begins with a single varint
  ** which determines the total number of bytes in the header. The varint
  ** value is the size of the header in bytes including the size varint
  ** itself. */
  testcase(nHdr == 126);
  testcase(nHdr == 127);
  if (nHdr <= 126) {
    /* The common case */
    nHdr += 1;
  } else {
    /* Rare case of a really large header */
    nVarint = sqlite3VarintLen(nHdr);
    nHdr += nVarint;
    if (nVarint < sqlite3VarintLen(nHdr)) nHdr++;
  }
  nByte = nHdr + nData;

  /* Make sure the output register has a buffer large enough to store
  ** the new record. The output register (pOp->p3) is not allowed to
  ** be one of the input registers (because the following call to
  ** sqlite3VdbeMemClearAndResize() could clobber the value before it is used).
  */
  if (nByte + nZero <= pOut->szMalloc) {
    pOut->z = pOut->zMalloc;
  }
  pOut->n = (int)nByte;
  pOut->flags = MEM_Blob;
  if (nZero) {
    pOut->u.nZero = nZero;
    pOut->flags |= MEM_Zero;
  }
  zHdr = (u8 *)pOut->z;
  zPayload = zHdr + nHdr;

  /* Write the record */
  if (nHdr < 0x80) {
    *(zHdr++) = nHdr;
  } else {
    zHdr += sqlite3PutVarint(zHdr, nHdr);
  }
  assert(pData0 <= pLast);
  pRec = pData0;
  while (1 /*exit-by-break*/) {
    serial_type = pRec->uTemp;
    /* EVIDENCE-OF: R-06529-47362 Following the size varint are one or more
    ** additional varints, one per column.
    ** EVIDENCE-OF: R-64536-51728 The values for each column in the record
    ** immediately follow the header. */
    if (serial_type <= 7) {
      *(zHdr++) = serial_type;
      if (serial_type == 0) {
        /* NULL value.  No change in zPayload */
      } else {
        u64 v;
        u32 i;
        if (serial_type == 7) {
          assert(sizeof(v) == sizeof(pRec->u.r));
          memcpy(&v, &pRec->u.r, sizeof(v));
          swapMixedEndianFloat(v);
        } else {
          v = pRec->u.i;
        }
        len = i = sqlite3SmallTypeSizes[serial_type];
        assert(i > 0);
        while (1 /*exit-by-break*/) {
          zPayload[--i] = (u8)(v & 0xFF);
          if (i == 0) break;
          v >>= 8;
        }
        zPayload += len;
      }
    } else if (serial_type < 0x80) {
      *(zHdr++) = serial_type;
      if (serial_type >= 14 && pRec->n > 0) {
        assert(pRec->z != 0);
        memcpy(zPayload, pRec->z, pRec->n);
        zPayload += pRec->n;
      }
    } else {
      zHdr += sqlite3PutVarint(zHdr, serial_type);
      if (pRec->n) {
        assert(pRec->z != 0);
        memcpy(zPayload, pRec->z, pRec->n);
        zPayload += pRec->n;
      }
    }
    if (pRec == pLast) break;
    pRec++;
  }
}

void execOpMove(Vdbe *p, Op *pOp) {
  int n = pOp->p3;
  int p1 = pOp->p1;
  int p2 = pOp->p2;

  Mem *pIn1 = &p->aMem[p1];
  Mem *pOut = &p->aMem[p2];
  do {
    sqlite3VdbeMemMove(pOut, pIn1);
    if (((pOut)->flags & MEM_Ephem) != 0 && sqlite3VdbeMemMakeWriteable(pOut)) {
    }

    pIn1++;
    pOut++;
  } while (--n);
}

int execOpCompare(Vdbe *p, Op *pOp, u32 *aPermute) {
  const KeyInfo *pKeyInfo = pOp->p4.pKeyInfo;
  int p1 = pOp->p1;
  int p2 = pOp->p2;
  int iCompare = 0;

  for (int i = 0; i < pOp->p3; i++) {
    int idx = aPermute ? aPermute[i] : (u32)i;
    CollSeq *pColl = pKeyInfo->aColl[i];
    int bRev = (pKeyInfo->aSortFlags[i] & KEYINFO_ORDER_DESC);
    iCompare = sqlite3MemCompare(&p->aMem[p1 + idx], &p->aMem[p2 + idx], pColl);
    if (iCompare) {
      if ((pKeyInfo->aSortFlags[i] & KEYINFO_ORDER_BIGNULL) &&
          ((p->aMem[p1 + idx].flags & MEM_Null) ||
           (p->aMem[p2 + idx].flags & MEM_Null))) {
        iCompare = -iCompare;
      }
      if (bRev) iCompare = -iCompare;
      break;
    }
  }
  return iCompare;
}

void execDeferredSeek(Vdbe *p, Op *pOp) {
  VdbeCursor *pC;      /* The P1 index cursor */
  VdbeCursor *pTabCur; /* The P2 table cursor (OP_DeferredSeek only) */
  i64 rowid;           /* Rowid that P1 current points to */

  pC = p->apCsr[pOp->p1];

  int rc = sqlite3VdbeCursorRestore(pC);

  if (!pC->nullRow) {
    rowid = 0; /* Not needed.  Only used to silence a warning. */
    rc = sqlite3VdbeIdxRowid(p->db, pC->uc.pCursor, &rowid);
    pTabCur = p->apCsr[pOp->p3];
    pTabCur->nullRow = 0;
    pTabCur->movetoTarget = rowid;
    pTabCur->deferredMoveto = 1;
    pTabCur->cacheStatus = CACHE_STALE;
    pTabCur->ub.aAltMap = pOp->p4.ai;
    pTabCur->pAltCursor = pC;
  } else {
    assert(pOp->opcode == OP_IdxRowid);
    sqlite3VdbeMemSetNull(&p->aMem[pOp->p2]);
  }
}

// true for jump, false for not jump
Bool execSeekRowid(Vdbe *p, Op *pOp) {
  VdbeCursor *pC;
  BtCursor *pCrsr;
  int res;
  u64 iKey;

  Mem *pIn3 = &p->aMem[pOp->p3];

  if ((pIn3->flags & (MEM_Int | MEM_IntReal)) == 0) {
    Mem x = pIn3[0];
    applyAffinity(&x, SQLITE_AFF_NUMERIC, p->db->enc);
    if ((x.flags & MEM_Int) == 0) return 1;
    iKey = x.u.i;
  } else {
    iKey = pIn3->u.i;
  }

  pC = p->apCsr[pOp->p1];
  pCrsr = pC->uc.pCursor;
  res = 0;
  int rc = sqlite3BtreeTableMoveto(pCrsr, iKey, 0, &res);
  pC->movetoTarget = iKey; /* Used by OP_Delete */
  pC->nullRow = 0;
  pC->cacheStatus = CACHE_STALE;
  pC->deferredMoveto = 0;
  pC->seekResult = res;
  if (res != 0) {
    if (pOp->p2 == 0) {
      rc = SQLITE_CORRUPT_BKPT;
    } else {
      return 1;
    }
  }
  return 0;
}

// __attribute__((optnone))
void execOpRowid(Vdbe *p, Op *pOp) {
  VdbeCursor *pC;
  i64 v;
  sqlite3_vtab *pVtab;
  const sqlite3_module *pModule;

  Mem *pOut = &p->aMem[pOp->p2];
  pOut->flags = MEM_Int;

  assert(pOp->p1 >= 0 && pOp->p1 < p->nCursor);
  pC = p->apCsr[pOp->p1];
  assert(pC != 0);
  assert(pC->eCurType != CURTYPE_PSEUDO || pC->nullRow);
  if (pC->nullRow) {
    pOut->flags = MEM_Null;
    return;
  } else if (pC->deferredMoveto) {
    v = pC->movetoTarget;
#ifndef SQLITE_OMIT_VIRTUALTABLE
  } else if (pC->eCurType == CURTYPE_VTAB) {
    assert(pC->uc.pVCur != 0);
    pVtab = pC->uc.pVCur->pVtab;
    pModule = pVtab->pModule;
    assert(pModule->xRowid);
    pModule->xRowid(pC->uc.pVCur, &v);
    sqlite3VtabImportErrmsg(p, pVtab);
#endif /* SQLITE_OMIT_VIRTUALTABLE */
  } else {
    assert(pC->eCurType == CURTYPE_BTREE);
    assert(pC->uc.pCursor != 0);
    sqlite3VdbeCursorRestore(pC);
    if (pC->nullRow) {
      pOut->flags = MEM_Null;
      return;
    }
    v = sqlite3BtreeIntegerKey(pC->uc.pCursor);
  }
  pOut->u.i = v;
}

void execOpAffinity(Vdbe *p, Op *pOp) {
  const char *zAffinity; /* The affinity to be applied */
  zAffinity = pOp->p4.z;
  Mem *pIn1 = &p->aMem[pOp->p1];
  while (1) {
    applyAffinity(pIn1, zAffinity[0], p->db->enc);
    if (zAffinity[0] == SQLITE_AFF_REAL && (pIn1->flags & MEM_Int) != 0) {
      if (pIn1->u.i <= 140737488355327LL && pIn1->u.i >= -140737488355328LL) {
        pIn1->flags |= MEM_IntReal;
        pIn1->flags &= ~MEM_Int;
      } else {
        pIn1->u.r = (double)pIn1->u.i;
        pIn1->flags |= MEM_Real;
        pIn1->flags &= ~MEM_Int;
      }
    }
    zAffinity++;
    if (zAffinity[0] == 0) return;
    pIn1++;
  }
  return;
}

int execSeekComparisons(Vdbe *p, Op *pOp) {
  int res;          /* Comparison result */
  int oc;           /* Opcode */
  VdbeCursor *pC;   /* The cursor to seek */
  UnpackedRecord r; /* The key to seek for */
  int nField;       /* Number of columns or fields in the key */
  i64 iKey;         /* The rowid we are to seek to */
  int eqOnly;       /* Only interested in == results */
  int rc;

  pC = p->apCsr[pOp->p1];
  oc = pOp->opcode;
  eqOnly = 0;
  pC->nullRow = 0;

  pC->deferredMoveto = 0;
  pC->cacheStatus = CACHE_STALE;
  if (pC->isTable) {
    u16 flags3, newType;
    /* The OPFLAG_SEEKEQ/BTREE_SEEK_EQ flag is only set on index cursors */
    assert(sqlite3BtreeCursorHasHint(pC->uc.pCursor, BTREE_SEEK_EQ) == 0 ||
           CORRUPT_DB);

    /* The input value in P3 might be of any type: integer, real, string,
    ** blob, or NULL.  But it needs to be an integer before we can do
    ** the seek, so convert it. */
    Mem *pIn3 = &p->aMem[pOp->p3];
    flags3 = pIn3->flags;
    if ((flags3 & (MEM_Int | MEM_Real | MEM_IntReal | MEM_Str)) == MEM_Str) {
      applyNumericAffinity(pIn3, 0);
    }
    iKey = sqlite3VdbeIntValue(pIn3); /* Get the integer key value */
    newType = pIn3->flags; /* Record the type after applying numeric affinity */
    pIn3->flags = flags3;  /* But convert the type back to its original */

    /* If the P3 value could not be converted into an integer without
    ** loss of information, then special processing is required... */
    if ((newType & (MEM_Int | MEM_IntReal)) == 0) {
      int c;
      if ((newType & MEM_Real) == 0) {
        if ((newType & MEM_Null) || oc >= OP_SeekGE) {
          return 1;
        } else {
          rc = sqlite3BtreeLast(pC->uc.pCursor, &res);
          goto seek_not_found;
        }
      }
      c = sqlite3IntFloatCompare(iKey, pIn3->u.r);

      /* If the approximation iKey is larger than the actual real search
      ** term, substitute >= for > and < for <=. e.g. if the search term
      ** is 4.9 and the integer approximation 5:
      **
      **        (x >  4.9)    ->     (x >= 5)
      **        (x <= 4.9)    ->     (x <  5)
      */
      if (c > 0) {
        if ((oc & 0x0001) == (OP_SeekGT & 0x0001)) oc--;
      }

      /* If the approximation iKey is smaller than the actual real search
      ** term, substitute <= for < and > for >=.  */
      else if (c < 0) {
        if ((oc & 0x0001) == (OP_SeekLT & 0x0001)) oc++;
      }
    }
    rc = sqlite3BtreeTableMoveto(pC->uc.pCursor, (u64)iKey, 0, &res);
    pC->movetoTarget = iKey; /* Used by OP_Delete */
  } else {
    /* For a cursor with the OPFLAG_SEEKEQ/BTREE_SEEK_EQ hint, only the
    ** OP_SeekGE and OP_SeekLE opcodes are allowed, and these must be
    ** immediately followed by an OP_IdxGT or OP_IdxLT opcode, respectively,
    ** with the same key.
    */
    if (sqlite3BtreeCursorHasHint(pC->uc.pCursor, BTREE_SEEK_EQ)) {
      eqOnly = 1;
    }

    nField = pOp->p4.i;
    r.pKeyInfo = pC->pKeyInfo;
    r.nField = (u16)nField;

    /* The next line of code computes as follows, only faster:
    **   if( oc==OP_SeekGT || oc==OP_SeekLE ){
    **     r.default_rc = -1;
    **   }else{
    **     r.default_rc = +1;
    **   }
    */
    r.default_rc = ((1 & (oc - OP_SeekLT)) ? -1 : +1);

    r.aMem = &p->aMem[pOp->p3];
    r.eqSeen = 0;
    rc = sqlite3BtreeIndexMoveto(pC->uc.pCursor, &r, &res);
    if (eqOnly && r.eqSeen == 0) {
      assert(res != 0);
      goto seek_not_found;
    }
  }
  if (oc >= OP_SeekGE) {
    assert(oc == OP_SeekGE || oc == OP_SeekGT);
    if (res < 0 || (res == 0 && oc == OP_SeekGT)) {
      res = 0;
      rc = sqlite3BtreeNext(pC->uc.pCursor, 0);
      if (rc != SQLITE_OK) {
        if (rc == SQLITE_DONE) {
          rc = SQLITE_OK;
          res = 1;
        }
      }
    } else {
      res = 0;
    }
  } else {
    assert(oc == OP_SeekLT || oc == OP_SeekLE);
    if (res > 0 || (res == 0 && oc == OP_SeekLT)) {
      res = 0;
      rc = sqlite3BtreePrevious(pC->uc.pCursor, 0);
      if (rc != SQLITE_OK) {
        if (rc == SQLITE_DONE) {
          rc = SQLITE_OK;
          res = 1;
        }
      }
    } else {
      /* res might be negative because the table is empty.  Check to
      ** see if this is the case.
      */
      res = sqlite3BtreeEof(pC->uc.pCursor);
    }
  }
seek_not_found:
  if (res) return 1;

  /* Skip the OP_IdxLt or OP_IdxGT that follows */
  if (eqOnly) return 2;

  return 0;
}

void execOpOpenEphemeral(Vdbe *p, Op *pOp) {
  VdbeCursor *pCx;
  KeyInfo *pKeyInfo;

  static const int vfsFlags =
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_EXCLUSIVE |
      SQLITE_OPEN_DELETEONCLOSE | SQLITE_OPEN_TRANSIENT_DB;

  if (pOp->p3 > 0) {
    /* Make register reg[P3] into a value that can be used as the data
    ** form sqlite3BtreeInsert() where the length of the data is zero. */
    assert(pOp->p2 == 0); /* Only used when number of columns is zero */
    assert(pOp->opcode == OP_OpenEphemeral);
    assert(aMem[pOp->p3].flags & MEM_Null);
    p->aMem[pOp->p3].n = 0;
    p->aMem[pOp->p3].z = "";
  }
  pCx = p->apCsr[pOp->p1];
  if (pCx && !pCx->noReuse && ALWAYS(pOp->p2 <= pCx->nField)) {
    /* If the ephermeral table is already open and has no duplicates from
    ** OP_OpenDup, then erase all existing content so that the table is
    ** empty again, rather than creating a new table. */
    assert(pCx->isEphemeral);
    pCx->seqCount = 0;
    pCx->cacheStatus = CACHE_STALE;
    sqlite3BtreeClearTable(pCx->ub.pBtx, pCx->pgnoRoot, 0);
  } else {
    pCx = allocateCursor(p, pOp->p1, pOp->p2, CURTYPE_BTREE);
    pCx->isEphemeral = 1;
    int rc =
        sqlite3BtreeOpen(p->db->pVfs, 0, p->db, &pCx->ub.pBtx,
                         BTREE_OMIT_JOURNAL | BTREE_SINGLE | pOp->p5, vfsFlags);
    if (rc == SQLITE_OK) {
      rc = sqlite3BtreeBeginTrans(pCx->ub.pBtx, 1, 0);
      if (rc == SQLITE_OK) {
        /* If a transient index is required, create it by calling
        ** sqlite3BtreeCreateTable() with the BTREE_BLOBKEY flag before
        ** opening it. If a transient table is required, just use the
        ** automatically created table with root-page 1 (an BLOB_INTKEY table).
        */
        if ((pCx->pKeyInfo = pKeyInfo = pOp->p4.pKeyInfo) != 0) {
          assert(pOp->p4type == P4_KEYINFO);
          rc = sqlite3BtreeCreateTable(pCx->ub.pBtx, &pCx->pgnoRoot,
                                       BTREE_BLOBKEY | pOp->p5);
          if (rc == SQLITE_OK) {
            assert(pCx->pgnoRoot == SCHEMA_ROOT + 1);
            assert(pKeyInfo->db == db);
            assert(pKeyInfo->enc == ENC(db));
            rc = sqlite3BtreeCursor(pCx->ub.pBtx, pCx->pgnoRoot, BTREE_WRCSR,
                                    pKeyInfo, pCx->uc.pCursor);
          }
          pCx->isTable = 0;
        } else {
          pCx->pgnoRoot = SCHEMA_ROOT;
          rc = sqlite3BtreeCursor(pCx->ub.pBtx, SCHEMA_ROOT, BTREE_WRCSR, 0,
                                  pCx->uc.pCursor);
          pCx->isTable = 1;
        }
      }
      pCx->isOrdered = (pOp->p5 != BTREE_UNORDERED);
      if (rc) {
        sqlite3BtreeClose(pCx->ub.pBtx);
      }
    }
  }
  pCx->nullRow = 1;
}

// TODO: inline
void execOpIdxInsert(Vdbe *p, Op *pOp) {
  BtreePayload x;
  Mem *pIn2 = &p->aMem[pOp->p2];

  x.nKey = pIn2->n;
  x.pKey = pIn2->z;
  x.aMem = p->aMem + pOp->p3;
  x.nMem = (u16)pOp->p4.i;

  VdbeCursor *pC = p->apCsr[pOp->p1];
  sqlite3BtreeInsert(
      pC->uc.pCursor, &x,
      (pOp->p5 & (OPFLAG_APPEND | OPFLAG_SAVEPOSITION | OPFLAG_PREFORMAT)),
      ((pOp->p5 & OPFLAG_USESEEKRESULT) ? pC->seekResult : 0));
  pC->cacheStatus = CACHE_STALE;
}

void execOpNullRow(Vdbe *p, Op *pOp) {
  VdbeCursor *pC;
  pC = p->apCsr[pOp->p1];
  if (pC == 0) {
    /* If the cursor is not already open, create a special kind of
    ** pseudo-cursor that always gives null rows. */
    pC = allocateCursor(p, pOp->p1, 1, CURTYPE_PSEUDO);
    pC->seekResult = 0;
    pC->isTable = 1;
    pC->noReuse = 1;
    pC->uc.pCursor = sqlite3BtreeFakeValidCursor();
  }
  pC->nullRow = 1;
  pC->cacheStatus = CACHE_STALE;
  if (pC->eCurType == CURTYPE_BTREE) {
    sqlite3BtreeClearCursor(pC->uc.pCursor);
  }
}

int execIdxComparisons(Vdbe *p, Op *pOp) {
  VdbeCursor *pC;
  int res;
  UnpackedRecord r;

  pC = p->apCsr[pOp->p1];
  r.pKeyInfo = pC->pKeyInfo;
  r.nField = (u16)pOp->p4.i;
  if (pOp->opcode < OP_IdxLT) {
    r.default_rc = -1;
  } else {
    r.default_rc = 0;
  }
  r.aMem = &p->aMem[pOp->p3];

  /* Inlined version of sqlite3VdbeIdxKeyCompare() */
  {
    i64 nCellKey = 0;
    BtCursor *pCur;
    Mem m;

    assert(pC->eCurType == CURTYPE_BTREE);
    pCur = pC->uc.pCursor;
    assert(sqlite3BtreeCursorIsValid(pCur));
    nCellKey = sqlite3BtreePayloadSize(pCur);
    sqlite3VdbeMemInit(&m, p->db, 0);
    sqlite3VdbeMemFromBtreeZeroOffset(pCur, (u32)nCellKey, &m);
    res = sqlite3VdbeRecordCompareWithSkip(m.n, m.z, &r, 0);
    sqlite3VdbeMemReleaseMalloc(&m);
  }
  /* End of inlined sqlite3VdbeIdxKeyCompare() */

  if ((pOp->opcode & 1) == (OP_IdxLT & 1)) {
    res = -res;
  } else {
    res++;
  }
  return (res > 0);
}