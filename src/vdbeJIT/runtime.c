#include "runtime.h"

#define isSorter(x) ((x)->eCurType == CURTYPE_SORTER)

typedef int (*jitProgram)();

__attribute__((optnone)) int sqlite3VdbeExecJIT(Vdbe *p) {
  if (p->jitCode == NULL) return sqlite3VdbeExec(p);

  int rc = ((jitProgram)p->jitCode)();

  if (rc > 100000) {
    printf("TODO: Implement OP %d \n", rc - 100000);
    rc = sqlite3VdbeExec(p);
  }
  return rc;
}

// TODO: replace with assembly
void execOpAdd(Mem *pIn1, Mem *pIn2, Mem *pOut) {
  u16 flag;
  if ((pIn1->flags & pIn2->flags & MEM_Int) != 0) {
    pOut->u.i = pIn1->u.i + pIn2->u.i;
    flag = MEM_Int;
  } else {
    pOut->u.r = sqlite3VdbeRealValue(pIn1) + sqlite3VdbeRealValue(pIn2);
    flag = MEM_Real;
  }
  pOut->flags = (pOut->flags & ~(MEM_TypeMask | MEM_Zero)) | flag;
}
void execOpSubtract(Mem *pIn1, Mem *pIn2, Mem *pOut) {
  u16 flag;
  if ((pIn1->flags & pIn2->flags & MEM_Int) != 0) {
    pOut->u.i = pIn1->u.i - pIn2->u.i;
    flag = MEM_Int;
  } else {
    pOut->u.r = sqlite3VdbeRealValue(pIn2) - sqlite3VdbeRealValue(pIn1);
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
    pOut->u.r = sqlite3VdbeRealValue(pIn2) * sqlite3VdbeRealValue(pIn1);
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

  assert(pOp->opcode == OP_OpenWrite || pOp->p5 == 0 ||
         pOp->p5 == OPFLAG_SEEKEQ);
  assert(pOp->opcode == OP_OpenRead || pOp->opcode == OP_ReopenIdx ||
         p->readOnly == 0);

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
    rc = sqlite3VdbeSorterRewind(pC, &res);
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

  assert(pOp->p1 >= 0 && pOp->p1 < p->nCursor);
  assert(pOp->p3 > 0 && pOp->p3 <= (p->nMem + 1 - p->nCursor));
  pC = p->apCsr[pOp->p1];
  p2 = (u32)pOp->p2;

op_column_restart:
  assert(pC != 0);
  assert(p2 < (u32)pC->nField ||
         (pC->eCurType == CURTYPE_PSEUDO && pC->seekResult == 0));
  aOffset = pC->aOffset;
  assert(aOffset == pC->aType + pC->nField);
  assert(pC->eCurType != CURTYPE_VTAB);
  assert(pC->eCurType != CURTYPE_PSEUDO || pC->nullRow);
  assert(pC->eCurType != CURTYPE_SORTER);

  if (pC->cacheStatus != p->cacheCtr) { /*OPTIMIZATION-IF-FALSE*/
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
        goto op_column_out;
      }
    } else {
      pCrsr = pC->uc.pCursor;
      if (pC->deferredMoveto) {
        u32 iMap;
        assert(!pC->isEphemeral);
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
      assert(pC->eCurType == CURTYPE_BTREE);
      assert(pCrsr);
      assert(sqlite3BtreeCursorIsValid(pCrsr));
      pC->payloadSize = sqlite3BtreePayloadSize(pCrsr);
      pC->aRow = (const u8 *)sqlite3BtreePayloadFetch(pCrsr, &pC->szRow);
      assert(pC->szRow <= pC->payloadSize);
      assert(pC->szRow <= 65536); /* Maximum page size is 64KiB */
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
        goto op_column_corrupt;
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
  } else if (sqlite3BtreeCursorHasMoved(pC->uc.pCursor)) {
    rc = sqlite3VdbeHandleMovedCursor(pC);
    goto op_column_restart;
  }

  /* Make sure at least the first p2+1 entries of the header have been
  ** parsed and valid information is in aOffset[] and pC->aType[].
  */
  if (pC->nHdrParsed <= p2) {
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
          goto op_column_corrupt;
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
      goto op_column_out;
    }
  } else {
    t = pC->aType[p2];
  }

  /* Extract the content for the p2+1-th column.  Control can only
  ** reach this point if aOffset[p2], aOffset[p2+1], and pC->aType[p2] are
  ** all valid.
  */
  assert(p2 < pC->nHdrParsed);
  assert(rc == SQLITE_OK);
  pDest = &aMem[pOp->p3];
  assert(sqlite3VdbeCheckMemInvariants(pDest));
  if (VdbeMemDynamic(pDest)) {
    sqlite3VdbeMemSetNull(pDest);
  }
  assert(t == pC->aType[p2]);
  if (pC->szRow >= aOffset[p2 + 1]) {
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
  } else {
    pDest->enc = encoding;
    /* This branch happens only when content is on overflow pages */
    if (((pOp->p5 & (OPFLAG_LENGTHARG | OPFLAG_TYPEOFARG)) != 0 &&
         ((t >= 12 && (t & 1) == 0) || (pOp->p5 & OPFLAG_TYPEOFARG) != 0)) ||
        (len = sqlite3VdbeSerialTypeLen(t)) == 0) {
      /* Content is irrelevant for
      **    1. the typeof() function,
      **    2. the length(X) function if X is a blob, and
      **    3. if the content length is zero.
      ** So we might as well use bogus content rather than reading
      ** content from disk.
      **
      ** Although sqlite3VdbeSerialGet() may read at most 8 bytes from the
      ** buffer passed to it, debugging function VdbeMemPrettyPrint() may
      ** read more.  Use the global constant sqlite3CtypeMap[] as the array,
      ** as that array is 256 bytes long (plenty for VdbeMemPrettyPrint())
      ** and it begins with a bunch of zeros.
      */
      sqlite3VdbeSerialGet((u8 *)sqlite3CtypeMap, t, pDest);
    } else {
      rc = sqlite3VdbeMemFromBtree(pC->uc.pCursor, aOffset[p2], len, pDest);
      sqlite3VdbeSerialGet((const u8 *)pDest->z, t, pDest);
      pDest->flags &= ~MEM_Ephem;
    }
  }

op_column_out:
  return rc;

op_column_corrupt:
  return rc;
}

int execOpNext(Vdbe *p, Op pOp) {
  VdbeCursor *pC = p->apCsr[pOp.p1];

  int rc = sqlite3BtreeNext(pC->uc.pCursor, pOp.p3);

  pC->cacheStatus = CACHE_STALE;
  if (rc == SQLITE_OK) {
    pC->nullRow = 0;
    p->aCounter[pOp.p5]++;
    p->pc = pOp.p2;
    return rc;
  }
  rc = SQLITE_OK;
  pC->nullRow = 1;
  p->pc++;
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

Bool execComparison(Vdbe *p, Op *pOp) {
  int res, res2;    /* Result of the comparison of pIn1 against pIn3 */
  char affinity;    /* Affinity to use for comparison */
  u16 flags1;       /* Copy of initial value of pIn1->flags */
  u16 flags3;       /* Copy of initial value of pIn3->flags */
  int iCompare = 0; /* Result of last comparison */

  Mem *pIn1 = &p->aMem[pOp->p1];
  Mem *pIn3 = &p->aMem[pOp->p3];

  flags1 = pIn1->flags;
  flags3 = pIn3->flags;
  if ((flags1 & flags3 & MEM_Int) != 0) {
    /* Common case of comparison of two integers */
    if (pIn3->u.i > pIn1->u.i) {
      if (sqlite3aGTb[pOp->opcode]) {
        goto jump_to_p2;
      }
      iCompare = +1;
      VVA_ONLY(iCompareIsInit = 1;)
    } else if (pIn3->u.i < pIn1->u.i) {
      if (sqlite3aLTb[pOp->opcode]) {
        goto jump_to_p2;
      }
      iCompare = -1;
      VVA_ONLY(iCompareIsInit = 1;)
    } else {
      if (sqlite3aEQb[pOp->opcode]) {
        goto jump_to_p2;
      }
      iCompare = 0;
      VVA_ONLY(iCompareIsInit = 1;)
    }
    p->pc++;
    return 0;
  }
  if ((flags1 | flags3) & MEM_Null) {
    /* One or both operands are NULL */
    if (pOp->p5 & SQLITE_NULLEQ) {
      /* If SQLITE_NULLEQ is set (which will only happen if the operator is
      ** OP_Eq or OP_Ne) then take the jump or not depending on whether
      ** or not both operands are null.
      */
      assert((flags1 & MEM_Cleared) == 0);
      assert((pOp->p5 & SQLITE_JUMPIFNULL) == 0 || CORRUPT_DB);
      testcase((pOp->p5 & SQLITE_JUMPIFNULL) != 0);
      if ((flags1 & flags3 & MEM_Null) != 0 && (flags3 & MEM_Cleared) == 0) {
        res = 0; /* Operands are equal */
      } else {
        res = ((flags3 & MEM_Null) ? -1 : +1); /* Operands are not equal */
      }
    } else {
      /* SQLITE_NULLEQ is clear and at least one operand is NULL,
      ** then the result is always NULL.
      ** The jump is taken if the SQLITE_JUMPIFNULL bit is set.
      */
      if (pOp->p5 & SQLITE_JUMPIFNULL) {
        goto jump_to_p2;
      }
      iCompare = 1; /* Operands are not equal */
      VVA_ONLY(iCompareIsInit = 1;)
      p->pc++;
      return 0;
    }
  } else {
    /* Neither operand is NULL and we couldn't do the special high-speed
    ** integer comparison case.  So do a general-case comparison. */
    affinity = pOp->p5 & SQLITE_AFF_MASK;
    if (affinity >= SQLITE_AFF_NUMERIC) {
      if ((flags1 | flags3) & MEM_Str) {
        if ((flags1 & (MEM_Int | MEM_IntReal | MEM_Real | MEM_Str)) ==
            MEM_Str) {
          applyNumericAffinity(pIn1, 0);
          testcase(flags3 == pIn3->flags);
          flags3 = pIn3->flags;
        }
        if ((flags3 & (MEM_Int | MEM_IntReal | MEM_Real | MEM_Str)) ==
            MEM_Str) {
          applyNumericAffinity(pIn3, 0);
        }
      }
    } else if (affinity == SQLITE_AFF_TEXT &&
               ((flags1 | flags3) & MEM_Str) != 0) {
      if ((flags1 & MEM_Str) == 0 &&
          (flags1 & (MEM_Int | MEM_Real | MEM_IntReal)) != 0) {
        testcase(pIn1->flags & MEM_Int);
        testcase(pIn1->flags & MEM_Real);
        testcase(pIn1->flags & MEM_IntReal);
        sqlite3VdbeMemStringify(pIn1, p->db->enc, 1);
        testcase((flags1 & MEM_Dyn) != (pIn1->flags & MEM_Dyn));
        flags1 = (pIn1->flags & ~MEM_TypeMask) | (flags1 & MEM_TypeMask);
        if (NEVER(pIn1 == pIn3)) flags3 = flags1 | MEM_Str;
      }
      if ((flags3 & MEM_Str) == 0 &&
          (flags3 & (MEM_Int | MEM_Real | MEM_IntReal)) != 0) {
        testcase(pIn3->flags & MEM_Int);
        testcase(pIn3->flags & MEM_Real);
        testcase(pIn3->flags & MEM_IntReal);
        sqlite3VdbeMemStringify(pIn3, p->db->enc, 1);
        testcase((flags3 & MEM_Dyn) != (pIn3->flags & MEM_Dyn));
        flags3 = (pIn3->flags & ~MEM_TypeMask) | (flags3 & MEM_TypeMask);
      }
    }
    assert(pOp->p4type == P4_COLLSEQ || pOp->p4.pColl == 0);
    res = sqlite3MemCompare(pIn3, pIn1, pOp->p4.pColl);
  }

  /* At this point, res is negative, zero, or positive if reg[P1] is
  ** less than, equal to, or greater than reg[P3], respectively.  Compute
  ** the answer to this operator in res2, depending on what the comparison
  ** operator actually is.  The next block of code depends on the fact
  ** that the 6 comparison operators are consecutive integers in this
  ** order:  NE, EQ, GT, LE, LT, GE */
  assert(OP_Eq == OP_Ne + 1);
  assert(OP_Gt == OP_Ne + 2);
  assert(OP_Le == OP_Ne + 3);
  assert(OP_Lt == OP_Ne + 4);
  assert(OP_Ge == OP_Ne + 5);
  if (res < 0) {
    res2 = sqlite3aLTb[pOp->opcode];
  } else if (res == 0) {
    res2 = sqlite3aEQb[pOp->opcode];
  } else {
    res2 = sqlite3aGTb[pOp->opcode];
  }
  iCompare = res;
  VVA_ONLY(iCompareIsInit = 1;)

  /* Undo any changes made by applyAffinity() to the input registers. */
  assert((pIn3->flags & MEM_Dyn) == (flags3 & MEM_Dyn));
  pIn3->flags = flags3;
  assert((pIn1->flags & MEM_Dyn) == (flags1 & MEM_Dyn));
  pIn1->flags = flags1;

  if (res2) {
    goto jump_to_p2;
  }
  p->pc++;
  return 0;

jump_to_p2:
  p->pc = pOp->p2;
  return 1;
}

void execAggrStepOne(Vdbe *p, Op *pOp) {
  int i;
  sqlite3_context *pCtx;
  Mem *pMem;

  assert(pOp->p4type == P4_FUNCCTX);
  pCtx = pOp->p4.pCtx;
  pMem = &p->aMem[pOp->p3];

#ifdef SQLITE_DEBUG
  if (pOp->p1) {
    /* This is an OP_AggInverse call.  Verify that xStep has always
    ** been called at least once prior to any xInverse call. */
    assert(pMem->uTemp == 0x1122e0e3);
  } else {
    /* This is an OP_AggStep call.  Mark it as such. */
    pMem->uTemp = 0x1122e0e3;
  }
#endif

  /* If this function is inside of a trigger, the register array in aMem[]
  ** might change from one evaluation to the next.  The next block of code
  ** checks to see if the register array has changed, and if so it
  ** reinitializes the relavant parts of the sqlite3_context object */
  if (pCtx->pMem != pMem) {
    pCtx->pMem = pMem;
    for (i = pCtx->argc - 1; i >= 0; i--) pCtx->argv[i] = &p->aMem[pOp->p2 + i];
  }

#ifdef SQLITE_DEBUG
  for (i = 0; i < pCtx->argc; i++) {
    assert(memIsValid(pCtx->argv[i]));
    REGISTER_TRACE(pOp->p2 + i, pCtx->argv[i]);
  }
#endif

  pMem->n++;
  assert(pCtx->pOut->flags == MEM_Null);
  assert(pCtx->isError == 0);
  assert(pCtx->skipFlag == 0);
#ifndef SQLITE_OMIT_WINDOWFUNC
  if (pOp->p1) {
    (pCtx->pFunc->xInverse)(pCtx, pCtx->argc, pCtx->argv);
  } else
#endif
    (pCtx->pFunc->xSFunc)(pCtx, pCtx->argc,
                          pCtx->argv); /* IMP: R-24505-23230 */

  if (pCtx->isError) {
    if (pCtx->isError > 0) {
      sqlite3VdbeError(p, "%s", sqlite3_value_text(pCtx->pOut));
      int rc = pCtx->isError;
    }
    if (pCtx->skipFlag) {
      assert(pOp[-1].opcode == OP_CollSeq);
      i = pOp[-1].p1;
      if (i) sqlite3VdbeMemSetInt64(&p->aMem[i], 1);
      pCtx->skipFlag = 0;
    }
    sqlite3VdbeMemRelease(pCtx->pOut);
    pCtx->pOut->flags = MEM_Null;
    pCtx->isError = 0;
  }
  assert(pCtx->pOut->flags == MEM_Null);
  assert(pCtx->skipFlag == 0);
}