#include "operations.h"

#include <stddef.h>

#include "inMemorySort.h"
#include "runtime.h"

#define CURSOR_VALID 0

static void genBranchTo(CompilerContext *ctx, int from, int to, int offset = 0,
                        bool brif = false) {
  wasmblr::CodeGenerator &cg = ctx->cg;

  int fromBlock = ctx->branchTable[from];
  int toBlock = ctx->branchTable[to];

  int br_destination;
  if (toBlock > fromBlock) {
    br_destination = toBlock - fromBlock - 1;
  } else {
    cg.i32.const_(to);
    cg.local.set(0);
    br_destination = ctx->branchTable[ctx->branchTable.size() - 1] - fromBlock;
  }
  br_destination += offset;
  if (brif)
    cg.br_if(br_destination);
  else
    cg.br(br_destination);
}

void genOpInit(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;

  // For self altering instructions, treat the address as parameter
  cg.i32.const_((int)&pOp->p1);
  cg.i32.const_(1);
  cg.i32.store();
  genBranchTo(ctx, currPos, pOp->p2);
}

void genOpGoto(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;

  genBranchTo(ctx, currPos, pOp->p2);
}

void genOpGoSub(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  Mem *pIn1 = &p->aMem[pOp->p1];
  cg.i32.const_((intptr_t)&pIn1->flags);
  cg.i32.const_(MEM_Int);
  cg.i32.store16();
  cg.i32.const_((intptr_t)&pIn1->u.i);
  cg.i32.const_(currPos);
  cg.i32.store();
  genBranchTo(ctx, currPos, pOp->p2);
}

void genOpReturn(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  Mem *pIn1 = &p->aMem[pOp->p1];

  // if( pIn1->flags & MEM_Int )
  cg.i32.const_((int32_t)&pIn1->flags);
  cg.i32.load16_u();
  cg.i32.const_(MEM_Int);
  cg.i32.and_();

  cg.if_(cg.void_);
  {
    // GOTO pIn1->u.i + 1
    cg.i32.const_((int32_t)&pIn1->u.i);
    cg.i32.load();
    cg.i32.const_(1);
    cg.i32.add();
    cg.local.set(0);
    cg.br(ctx->branchTable[ctx->branchTable.size() - 1] -
          ctx->branchTable[currPos] + 1);
  }
  cg.end();
}

void genOpSorterOpen(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  int pCx_idx = 0;
  cg.i32.const_((intptr_t)p);
  cg.i32.const_((intptr_t)pOp->p1);
  cg.i32.const_((intptr_t)pOp->p2);
  cg.i32.const_((intptr_t)CURTYPE_SORTER);
  cg.call(ctx->imports["allocateCursor"]);
  cg.local.tee(pCx_idx);
  cg.i32.const_((intptr_t)pOp->p4.pKeyInfo);
  cg.i32.store(1U, offsetof(VdbeCursor, pKeyInfo));
  cg.i32.const_((intptr_t)p->db);
  cg.i32.const_((intptr_t)pOp->p3);
  cg.local.get(pCx_idx);
  cg.call(ctx->imports["sqlite3InMemSorterInit"]);
  cg.drop();
}

// Begin a transaction on database P1
void genOpTransaction(CompilerContext *ctx, Op *pOp) {
  Vdbe *p = ctx->p;
  wasmblr::CodeGenerator &cg = ctx->cg;

  sqlite3 *db = p->db;
  Db *pDb = &db->aDb[pOp->p1];
  Btree *pBt = pDb->pBt;

  cg.i32.const_((int)pBt);
  cg.i32.const_((int)pOp->p2);
  cg.i32.const_(4);
  // iMeta
  cg.call(ctx->imports["stackAlloc"]);
  cg.call(ctx->imports["sqlite3BtreeBeginTrans"]);
  cg.drop();
}

/* r[P2]=P1 */
void genOpInteger(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  Mem *pOut = &p->aMem[pOp->p2];

  cg.i32.const_((int)&pOut->u.i);
  cg._i64.const_((i64)pOp->p1);
  cg._i64.store();
  cg.i32.const_((int)&pOut->flags);
  cg.i32.const_(MEM_Int);
  cg.i32.store16();
}

// r[P2]=P4
void genOpReal(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  Mem *pOut = &p->aMem[pOp->p2];

  cg.i32.const_((int)&pOut->u.r);
  cg.i32.const_((int)pOp->p4.pReal);
  cg.f64.load();
  cg.f64.store();
  cg.i32.const_((int)&pOut->flags);
  cg.i32.const_(MEM_Real);
  cg.i32.store16();
}

void genOpNull(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  u16 nullFlag = pOp->p1 ? (MEM_Null | MEM_Cleared) : MEM_Null;

  for (int j = pOp->p2; j <= pOp->p3; j++) {
    Mem *pOut = &p->aMem[j];
    cg.i32.const_((int)&pOut->n);
    cg.i32.const_(0);
    cg.i32.store();

    cg.i32.const_((int)&pOut->flags);
    cg.i32.const_(nullFlag);
    cg.i32.store16();
  }
}

void genOpOnce(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  // assuming no pFrame

  // dynamic parameters
  cg.i32.const_((intptr_t)&pOp->p1);
  cg.i32.load();
  cg.i32.const_((intptr_t)&p->aOp[0].p1);
  cg.i32.load();
  cg.i32.sub();

  // if not equal
  cg.if_(cg.void_);
  {
    cg.i32.const_((intptr_t)&pOp->p1);
    cg.i32.const_((intptr_t)&p->aOp[0].p1);
    cg.i32.load();
    cg.i32.store();
  }
  cg.else_();
  { genBranchTo(ctx, currPos, pOp->p2, 1); }
  cg.end();
}

void genOpReadOpWrite(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  cg.i32.const_((int)p);
  cg.i32.const_((int)pOp);
  cg.call(ctx->imports["execOpenReadWrite"]);
  cg.drop();
}

void genOpRewind(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  // Goto beginning of table

  // For now, call helper function to achieve goal
  cg.i32.const_((int)p);
  cg.i32.const_((int)pOp);
  cg.call(ctx->imports["execOpRewind"]);
  genBranchTo(ctx, currPos, pOp->p2, 0, true);
}

// output=r[P1@P2]
void genOpResultRow(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  // p->cacheCtr = (p->cacheCtr + 2)|1;
  cg.i32.const_((int32_t)&p->cacheCtr);
  cg.i32.const_((int32_t)&p->cacheCtr);
  cg.i32.load(2U, 0U);
  cg.i32.const_(2);
  cg.i32.add();
  cg.i32.const_(1);
  cg.i32.or_();
  cg.i32.store();

  // p->pResultRow = &aMem[pOp->p1];
  cg.i32.const_((int32_t)&p->pResultRow);
  cg.i32.const_((int32_t)&p->aMem[pOp->p1]);
  cg.i32.store();
}

// r[P2@P3+1]=r[P1@P3+1]
void genOpCopy(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  // int i
  int i_index = 0;
  int increment = 40;  // size of Mem

  // i = 0
  cg.i32.const_(0);
  cg.local.set(i_index);

  cg.loop(cg.void_);
  {
    cg.local.get(i_index);
    cg.i32.const_((intptr_t)&p->aMem[pOp->p2]);
    cg.i32.add();

    cg.local.get(i_index);
    cg.i32.const_((intptr_t)&p->aMem[pOp->p1]);
    cg.i32.add();

    cg.i32.const_(MEM_Ephem);
    cg.call(ctx->imports["sqlite3VdbeMemShallowCopy"]);

    // i++
    cg.local.get(i_index);
    cg.i32.const_(increment);
    cg.i32.add();

    // i < pOp->p3?
    cg.local.tee(i_index);
    cg.i32.const_((pOp->p3 + 1) * increment);
    cg.i32.ne();
    cg.br_if(0);
  }
  cg.end();
}

void genOpSCopy(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  Mem *pIn1 = &p->aMem[pOp->p1];
  Mem *pOut = &p->aMem[pOp->p2];
  cg.i32.const_((intptr_t)pOut);
  cg.i32.const_((intptr_t)pIn1);
  cg.i32.const_(MEM_Ephem);
  cg.call(ctx->imports["sqlite3VdbeMemShallowCopy"]);
}

void genOpIf(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  bool ifNot = pOp->opcode == OP_IfNot;
  Mem *pMem = &p->aMem[pOp->p1];
  cg.i32.const_((intptr_t)&pMem->flags);
  cg.i32.load16_u();
  cg.i32.const_(MEM_Int | MEM_IntReal);
  cg.i32.and_();
  cg.if_(cg.i32);
  {
    cg.i32.const_((intptr_t)&pMem->u.i);
    cg.i32.load();
  }
  cg.else_();
  {
    cg.i32.const_((intptr_t)&pMem->flags);
    cg.i32.load16_u();
    cg.i32.const_(MEM_Null);
    cg.i32.and_();
    cg.if_(cg.i32);
    { cg.i32.const_(ifNot ? !pOp->p3 : pOp->p3); }
    cg.else_();
    {
      cg.i32.const_((intptr_t)&pMem->u.r);
      cg.f64.load();
      cg.f64.const_(0.0);
      cg.f64.ne();
    }
    cg.end();
  }
  cg.end();
  if (ifNot) cg.i32.eqz();
  genBranchTo(ctx, currPos, pOp->p2, 0, true);
}

void genOpDecrJumpZero(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  cg.i32.const_((int32_t)&p->aMem[pOp->p1].u.i);
  cg.i32.const_((int32_t)&p->aMem[pOp->p1].u.i);
  cg.i32.load(2U, 0U);
  cg.i32.const_(1);
  cg.i32.sub();
  cg.i32.store();

  cg.i32.const_((int32_t)&p->aMem[pOp->p1].u.i);
  cg.i32.load(2U, 0U);

  // jump equals zero
  cg.i32.eqz();
  genBranchTo(ctx, currPos, pOp->p2, 0, true);
}

// assume not blob
void genOpString(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  Mem *pOut = &p->aMem[pOp->p2];

  // pOut->flags = MEM_Str|MEM_Static|MEM_Term;
  cg.i32.const_((int)&pOut->flags);
  cg.i32.const_(MEM_Str | MEM_Static | MEM_Term);
  cg.i32.store16();

  // pOut->z = pOp->p4.z;
  cg.i32.const_((int)&pOut->z);
  cg.i32.const_((int)pOp->p4.z);
  cg.i32.store();

  // pOut->n = pOp->p1;
  cg.i32.const_((int)&pOut->n);
  cg.i32.const_(pOp->p1);
  cg.i32.store();

  // pOut->enc = p->db->enc;
  cg.i32.const_((int)&pOut->enc);
  cg.i32.const_(p->db->enc);
  cg.i32.store();
}

static void genComparisonOpCode(wasmblr::CodeGenerator &cg, int opcode) {
  switch (opcode) {
    case OP_Eq:
      cg.i32.eq();
      break;
    case OP_Ne:
      cg.i32.ne();
      break;
    case OP_Lt:
      cg.i32.lt_s();
      break;
    case OP_Le:
      cg.i32.le_s();
      break;
    case OP_Gt:
      cg.i32.gt_s();
      break;
    case OP_Ge:
      cg.i32.ge_s();
      break;
  }
}

void genComparisons(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  Mem *pIn1 = &p->aMem[pOp->p1];
  Mem *pIn3 = &p->aMem[pOp->p3];

  // pIn1->flags & pIn3->flags & MEM_Int
  cg.i32.const_((intptr_t)&pIn1->flags);
  cg.i32.load16_u();
  cg.i32.const_((intptr_t)&pIn3->flags);
  cg.i32.load16_u();
  cg.i32.and_();
  cg.i32.const_(MEM_Int);
  cg.i32.and_();

  cg.if_(cg.i32);
  {
    cg.i32.const_((intptr_t)&pIn3->u.i);
    cg.i32.load();
    cg.i32.const_((intptr_t)&pIn1->u.i);
    cg.i32.load();
    genComparisonOpCode(cg, pOp->opcode);
  }
  cg.else_();
  {
    cg.i32.const_((intptr_t)pIn3);
    cg.i32.const_((intptr_t)pIn1);
    cg.i32.const_((intptr_t)pOp->p4.pColl);
    cg.call(ctx->imports["sqlite3MemCompare"]);
    cg.i32.const_(0);
    genComparisonOpCode(cg, pOp->opcode);
  }
  cg.end();

  genBranchTo(ctx, currPos, pOp->p2, 0, true);
}

void genOpAggFinal(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  Mem *pMem = &p->aMem[pOp->p1];

  if (pOp->p3) {
    cg.i32.const_((intptr_t)pMem);
    cg.i32.const_((intptr_t)&p->aMem[pOp->p3]);
    cg.i32.const_((intptr_t)pOp->p4.pFunc);
    cg.call(ctx->imports["sqlite3VdbeMemAggValue"]);
    cg.drop();
    cg.i32.const_((intptr_t)&p->aMem[pOp->p3]);
  } else {
    cg.i32.const_((intptr_t)pMem);
    cg.i32.const_((intptr_t)pOp->p4.pFunc);
    cg.call(ctx->imports["sqlite3VdbeMemFinalize"]);
    cg.drop();
    cg.i32.const_((intptr_t)pMem);
  }
  cg.i32.const_((intptr_t)p->db->enc);
  cg.call(ctx->imports["sqlite3VdbeChangeEncoding"]);
  cg.drop();
}

// r[P3]=PX cursor P1 column P2
void genOpColumn(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  cg.i32.const_((int)p);
  cg.i32.const_((int)pOp);
  cg.call(ctx->imports["execOpColumn"]);
  cg.drop();
}

void genOpFunction(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  cg.i32.const_((int)p);
  cg.i32.const_((int)pOp);
  cg.call(ctx->imports["execOpFunction"]);
  cg.drop();
}

void genMathOps(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  cg.i32.const_((int)&p->aMem[pOp->p1]);
  cg.i32.const_((int)&p->aMem[pOp->p2]);
  cg.i32.const_((int)&p->aMem[pOp->p3]);

  switch (pOp->opcode) {
    case OP_Add:
      cg.call(ctx->imports["execOpAdd"]);
      break;
    case OP_Subtract:
      cg.call(ctx->imports["execOpSubtract"]);
      break;
    case OP_Multiply:
      cg.call(ctx->imports["execOpMultiply"]);
      break;
  }

}

void genMakeRecord(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  cg.i32.const_((intptr_t)p);
  cg.i32.const_((intptr_t)pOp);
  cg.call(ctx->imports["execOpMakeRecord"]);
}

void genAggrStepZero(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  int n;
  sqlite3_context *pCtx;

  assert(pOp->p4type == P4_FUNCDEF);
  n = pOp->p5;
  assert(pOp->p3 > 0 && pOp->p3 <= (p->nMem + 1 - p->nCursor));
  assert(n == 0 ||
         (pOp->p2 > 0 && pOp->p2 + n <= (p->nMem + 1 - p->nCursor) + 1));
  assert(pOp->p3 < pOp->p2 || pOp->p3 >= pOp->p2 + n);
  pCtx = (sqlite3_context *)sqlite3DbMallocRawNN(
      p->db, n * sizeof(sqlite3_value *) +
                 (sizeof(pCtx[0]) + sizeof(Mem) - sizeof(sqlite3_value *)));
  pCtx->pMem = 0;
  pCtx->pOut = (Mem *)&(pCtx->argv[n]);
  sqlite3VdbeMemInit(pCtx->pOut, p->db, MEM_Null);
  pCtx->pFunc = pOp->p4.pFunc;
  pCtx->iOp = (int)(pOp - p->aOp);
  pCtx->pVdbe = p;
  pCtx->skipFlag = 0;
  pCtx->isError = 0;
  pCtx->enc = p->db->enc;
  pCtx->argc = n;
  pOp->p4type = P4_FUNCCTX;
  pOp->p4.pCtx = pCtx;

  /* OP_AggInverse must have P1==1 and OP_AggStep must have P1==0 */
  assert(pOp->p1 == (pOp->opcode == OP_AggInverse));

  pOp->opcode = OP_AggStep1;
  genAggrStepOne(ctx, pOp);
}

void genAggrStepOne(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  cg.i32.const_((int)p);
  cg.i32.const_((int)pOp);
  cg.call(ctx->imports["execAggrStepOne"]);
}

void genNextTail(CompilerContext *ctx, Op *pOp, int currPos, int pC) {
  wasmblr::CodeGenerator &cg = ctx->cg;

  cg.i32.eqz();
  cg.if_(cg.void_);
  {
    cg.local.get(pC);
    cg.i32.const_(CACHE_STALE);
    cg.i32.store(1U, offsetof(VdbeCursor, cacheStatus));
    // pC->nullRow = 0
    cg.local.get(pC);
    cg.i32.const_(0);
    cg.i32.store8(0U, offsetof(VdbeCursor, nullRow));
    genBranchTo(ctx, currPos, pOp->p2, 1, false);
  }
  cg.else_();
  {
    cg.local.get(pC);
    cg.i32.const_(CACHE_STALE);
    cg.i32.store(1U, offsetof(VdbeCursor, cacheStatus));
    // pC->nullRow = 0
    cg.local.get(pC);
    cg.i32.const_(1);
    cg.i32.store8(0U, offsetof(VdbeCursor, nullRow));
  }
  cg.end();
}

void genOpNext(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  VdbeCursor **pC_ptr = &p->apCsr[pOp->p1];
  int pC = 0;

  // pC->uc.pCursor
  cg.i32.const_((intptr_t)pC_ptr);
  cg.i32.load();
  cg.local.tee(pC);
  cg.i32.const_((int)offsetof(VdbeCursor, uc));
  cg.i32.add();
  cg.i32.load();

  cg.i32.const_((intptr_t)pOp->p3);

  cg.call(ctx->imports["sqlite3BtreeNext"]);
  genNextTail(ctx, pOp, currPos, pC);
}

void genOpSorterNext(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  VdbeCursor **pC_ptr = &p->apCsr[pOp->p1];
  int pC = 0;

  cg.i32.const_((intptr_t)p->db);

  // pC->uc.pCursor
  cg.i32.const_((intptr_t)pC_ptr);
  cg.i32.load();
  cg.local.tee(pC);

  cg.call(ctx->imports["sqlite3InMemSorterNext"]);
  genNextTail(ctx, pOp, currPos, pC);
}

void genOpSorterInsert(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  VdbeCursor **pC_pointer = &p->apCsr[pOp->p1];
  Mem *pIn2 = &p->aMem[pOp->p2];

  cg.i32.const_((intptr_t)pC_pointer);
  cg.i32.load();
  cg.i32.const_((intptr_t)pIn2);
  cg.call(ctx->imports["sqlite3InMemSorterWrite"]);
  cg.drop();
}

void genOpenPseudo(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  int pCx_idx = 0;

  cg.i32.const_((intptr_t)p);
  cg.i32.const_((intptr_t)pOp->p1);
  cg.i32.const_((intptr_t)pOp->p3);
  cg.i32.const_((intptr_t)CURTYPE_PSEUDO);
  cg.call(ctx->imports["allocateCursor"]);
  cg.local.tee(pCx_idx);

  // pCx->nullRow = 1;
  cg.i32.const_(1);
  cg.i32.store8(0U, offsetof(VdbeCursor, nullRow));

  // pCx->seekResult = pOp->p2;
  cg.local.get(pCx_idx);
  cg.i32.const_(pOp->p2);
  cg.i32.store(1U, offsetof(VdbeCursor, seekResult));

  // pCx->isTable = 1;
  cg.local.get(pCx_idx);
  cg.i32.const_(1);
  cg.i32.store8(0U, offsetof(VdbeCursor, isTable));

  // pCx->uc.pCursor = sqlite3BtreeFakeValidCursor();
  static u8 fakeCursor = CURSOR_VALID;
  cg.local.get(pCx_idx);
  cg.i32.const_((intptr_t)&fakeCursor);
  cg.i32.store(1U, offsetof(VdbeCursor, uc));
}

void genSorterData(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  Mem *pOut = &p->aMem[pOp->p2];
  VdbeCursor **pC_ptr = &p->apCsr[pOp->p1];
  VdbeCursor **pC3_ptr = &p->apCsr[pOp->p3];

  cg.i32.const_((intptr_t)pC_ptr);
  cg.i32.load();
  cg.i32.const_((intptr_t)pOut);
  cg.call(ctx->imports["sqlite3InMemSorterRowkey"]);
  cg.drop();

  cg.i32.const_((intptr_t)pC3_ptr);
  cg.i32.load();
  cg.i32.const_(CACHE_STALE);
  cg.i32.store(1U, offsetof(VdbeCursor, cacheStatus));
}

void genOpCompare(CompilerContext *ctx, Op *pOp, u32 *aPermute) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  if ((pOp->p5 & OPFLAG_PERMUTE) == 0) {
    aPermute = 0;
  }
  cg.i32.const_((intptr_t)p);
  cg.i32.const_((intptr_t)pOp);
  cg.i32.const_((intptr_t)aPermute);
  cg.call(ctx->imports["execOpCompare"]);
  cg.local.set(0);
}

void genOpJump(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;

  cg.local.get(0);
  cg.i32.const_(0);
  cg.i32.lt_s();
  cg.if_(cg.void_);
  { genBranchTo(ctx, currPos, pOp->p1, 1); }
  cg.else_();
  {
    cg.local.get(0);
    cg.i32.eqz();
    cg.if_(cg.void_);
    { genBranchTo(ctx, currPos, pOp->p2, 2); }
    cg.else_();
    { genBranchTo(ctx, currPos, pOp->p3, 2); }
    cg.end();
  }
  cg.end();
}

void genOpMove(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  cg.i32.const_((int)p);
  cg.i32.const_((int)pOp);
  cg.call(ctx->imports["execOpMove"]);

  // // int i
  // int i_index = 0;
  // int increment = 40;  // size of Mem

  // // i = 0
  // cg.i32.const_(0);
  // cg.local.set(i_index);

  // cg.loop(cg.void_);
  // {
  //   cg.local.get(i_index);
  //   cg.i32.const_((intptr_t)&p->aMem[pOp->p2]);
  //   cg.i32.add();

  //   cg.local.get(i_index);
  //   cg.i32.const_((intptr_t)&p->aMem[pOp->p1]);
  //   cg.i32.add();

  //   cg.call(ctx->imports["sqlite3VdbeMemMove"]);
  //   cg.call_indirect({cg.i32, cg.i32}, {});

  //   // i++
  //   cg.local.get(i_index);
  //   cg.i32.const_(increment);
  //   cg.i32.add();

  //   // i < pOp->p3?
  //   cg.local.tee(i_index);
  //   cg.i32.const_((pOp->p3 + 1) * increment);
  //   cg.i32.ne();
  //   cg.br_if(0);
  // }
  // cg.end();
}

void genOpIfPos(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  Mem *pIn1 = &p->aMem[pOp->p1];
  cg.i32.const_((intptr_t)&pIn1->u.i);
  cg.i32.load();
  cg.i32.const_(0);
  cg.i32.gt_s();

  cg.if_(cg.void_);
  {
    // pIn1->u.i -= pOp->p3;
    cg.i32.const_((intptr_t)&pIn1->u.i);
    cg.i32.const_((intptr_t)&pIn1->u.i);
    cg.i32.load();
    cg.i32.const_(pOp->p3);
    cg.i32.sub();
    cg.i32.store();
    genBranchTo(ctx, currPos, pOp->p2, 1, false);
  }
  cg.end();
}

void genDeferredSeek(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  cg.i32.const_((intptr_t)p);
  cg.i32.const_((intptr_t)pOp);
  cg.call(ctx->imports["execDeferredSeek"]);
}

void genOpSeekRowid(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  cg.i32.const_((intptr_t)p);
  cg.i32.const_((intptr_t)pOp);
  cg.call(ctx->imports["execSeekRowid"]);
  genBranchTo(ctx, currPos, pOp->p2, 0, true);
}

void genOpRowid(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  cg.i32.const_((intptr_t)p);
  cg.i32.const_((intptr_t)pOp);
  cg.call(ctx->imports["execOpRowid"]);
}

void genOpAffinity(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  cg.i32.const_((intptr_t)p);
  cg.i32.const_((intptr_t)pOp);
  cg.call(ctx->imports["execOpAffinity"]);
}

void genSeekComparisons(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  cg.i32.const_((intptr_t)p);
  cg.i32.const_((intptr_t)pOp);
  cg.call(ctx->imports["execSeekComparisons"]);
  cg.local.tee(0);
  cg.i32.const_(1);
  cg.i32.eq();
  cg.if_(cg.void_);
  { genBranchTo(ctx, currPos, pOp->p2, 1, false); }
  cg.else_();
  {
    cg.local.get(0);
    cg.i32.const_(2);
    cg.i32.eq();
    genBranchTo(ctx, currPos, currPos + 2, 1, true);
  }
  cg.end();
}

void genOpCast(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  Mem *pIn1 = &p->aMem[pOp->p1];
  cg.i32.const_((intptr_t)pIn1);
  cg.i32.const_((intptr_t)pOp->p2);
  cg.i32.const_((intptr_t)p->db->enc);
  cg.call(ctx->imports["sqlite3VdbeMemCast"]);
  cg.drop();
}

void genOpenEphemeral(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  cg.i32.const_((intptr_t)p);
  cg.i32.const_((intptr_t)pOp);
  cg.call(ctx->imports["execOpOpenEphemeral"]);
}

void genOpNullRow(CompilerContext *ctx, Op *pOp) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;
  cg.i32.const_((intptr_t)p);
  cg.i32.const_((intptr_t)pOp);
  cg.call(ctx->imports["execOpNullRow"]);
}

void genOpIdxInsert(CompilerContext *ctx, Op *pOp) {
  Vdbe *p = ctx->p;
  wasmblr::CodeGenerator &cg = ctx->cg;

  cg.i32.const_((intptr_t)p);
  cg.i32.const_((intptr_t)pOp);
  cg.call(ctx->imports["execOpIdxInsert"]);

  // int pC_idx = 0;
  // int x_idx = 1;
  // Mem *pIn2 = &p->aMem[pOp->p2];

  // cg.i32.const_(sizeof(BtreePayload));
  // cg.call(ctx->imports["stackAlloc"]);

  // cg.local.tee(x_idx);
  // cg.i32.const_(pIn2->n);
  // cg.i32.store(1U, offsetof(BtreePayload, nKey));

  // cg.local.get(x_idx);
  // cg.i32.const_((intptr_t)pIn2->z);
  // cg.i32.store(1U, offsetof(BtreePayload, pKey));

  // cg.local.get(x_idx);
  // cg.i32.const_((intptr_t)(p->aMem + pOp->p3));
  // cg.i32.store(1U, offsetof(BtreePayload, aMem));

  // cg.local.get(x_idx);
  // cg.i32.const_(pOp->p4.i);
  // cg.i32.store16(1U, offsetof(BtreePayload, nMem));

  // VdbeCursor **pC_ptr = &p->apCsr[pOp->p1];
  // cg.i32.const_((intptr_t)pC_ptr);
  // cg.i32.load();
  // cg.local.tee(pC_idx);

  // // pC->uc.pCursor
  // cg.i32.const_(offsetof(VdbeCursor, uc));
  // cg.i32.add();

  // // &x
  // cg.local.get(x_idx);

  // cg.i32.const_((pOp->p5 & (OPFLAG_APPEND | OPFLAG_SAVEPOSITION |
  // OPFLAG_PREFORMAT)));

  // // ((pOp->p5 & OPFLAG_USESEEKRESULT) ? pC->seekResult : 0)
  // if (pOp->p5 & OPFLAG_USESEEKRESULT) {
  //   cg.local.get(pC_idx);
  //   cg.i32.load(1U, offsetof(VdbeCursor, seekResult));
  // } else {
  //   cg.i32.const_(0);
  // }

  // cg.call(ctx->imports["sqlite3BtreeInsert"]);
  // cg.call_indirect({cg.i32, cg.i32, cg.i32, cg.i32}, {cg.i32});
  // cg.drop();

  // // pC->cacheStatus = CACHE_STALE
  // cg.local.get(pC_idx);
  // cg.i32.const_(CACHE_STALE);
  // cg.i32.store16(1U, offsetof(VdbeCursor, cacheStatus));
}

void genOpIsNull(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  Mem *pIn1 = &p->aMem[pOp->p1];
  cg.i32.const_(pIn1->flags);
  cg.i32.const_(MEM_Null);
  cg.i32.and_();
  genBranchTo(ctx, currPos, pOp->p2, 0, true);
}

void genIdxComparisons(CompilerContext *ctx, Op *pOp, int currPos) {
  wasmblr::CodeGenerator &cg = ctx->cg;
  Vdbe *p = ctx->p;

  cg.i32.const_((intptr_t)p);
  cg.i32.const_((intptr_t)pOp);
  cg.call(ctx->imports["execIdxComparisons"]);
  genBranchTo(ctx, currPos, pOp->p2, 0, true);
}