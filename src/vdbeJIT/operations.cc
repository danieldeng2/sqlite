#include "operations.hh"

#include "runtime.h"

static void genBranchTo(wasmblr::CodeGenerator &cg, Vdbe *p,
                        std::vector<uint32_t> &branchTable, int from, int to,
                        int offset = 0, bool brif = false) {
  int fromBlock = branchTable[from];
  int toBlock = branchTable[to];

  int br_destination;
  if (toBlock > fromBlock) {
    br_destination = toBlock - fromBlock - 1;
  } else {
    cg.i32.const_((int32_t)&p->pc);
    cg.i32.const_(to);
    cg.i32.store();
    br_destination = branchTable[branchTable.size() - 1] - fromBlock;
  }
  br_destination += offset;
  if (brif)
    cg.br_if(br_destination);
  else
    cg.br(br_destination);
}

void genOpInit(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp,
               std::vector<uint32_t> &branchTable, int currPos) {
  // For self altering instructions, treat the address as parameter
  cg.i32.const_((int)&pOp->p1);
  cg.i32.const_(1);
  cg.i32.store();
  genBranchTo(cg, p, branchTable, currPos, pOp->p2);
}

void genOpGoto(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp,
               std::vector<uint32_t> &branchTable, int currPos) {
  genBranchTo(cg, p, branchTable, currPos, pOp->p2);
}

// Begin a transaction on database P1
void genOpTransaction(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp,
                      uint32_t stackAlloc) {
  sqlite3 *db = p->db;
  Db *pDb = &db->aDb[pOp->p1];
  Btree *pBt = pDb->pBt;

  cg.i32.const_((int)pBt);
  cg.i32.const_((int)pOp->p2);
  cg.i32.const_(4);
  // iMeta
  cg.call(stackAlloc);
  cg.i32.const_(reinterpret_cast<intptr_t>(&sqlite3BtreeBeginTrans));
  cg.call_indirect({cg.i32, cg.i32, cg.i32}, {cg.i32});
  cg.drop();
}

/* r[P2]=P1 */
void genOpInteger(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp) {
  Mem *pOut = &p->aMem[pOp->p2];

  cg.i32.const_((int)&pOut->u.i);
  cg._i64.const_((i64)pOp->p1);
  cg._i64.store();
  cg.i32.const_((int)&pOut->flags);
  cg.i32.const_(MEM_Int);
  cg.i32.store16();
}

// r[P2]=P4
void genOpReal(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp) {
  Mem *pOut = &p->aMem[pOp->p2];

  cg.i32.const_((int)&pOut->u.r);
  cg.i32.const_((int)pOp->p4.pReal);
  cg.f64.load();
  cg.f64.store();
  cg.i32.const_((int)&pOut->flags);
  cg.i32.const_(MEM_Real);
  cg.i32.store16();
}

void genOpNull(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp) {
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

void genOpOnce(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp,
               std::vector<uint32_t> &branchTable, int currPos) {
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
  { genBranchTo(cg, p, branchTable, currPos, pOp->p2, 1); }
  cg.end();
}

void genOpReadOpWrite(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp) {
  cg.i32.const_((int)p);
  cg.i32.const_((int)pOp);
  cg.i32.const_(reinterpret_cast<intptr_t>(&execOpenReadWrite));
  cg.call_indirect({cg.i32, cg.i32}, {cg.i32});
  cg.drop();
}

void genOpRewind(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp,
                 std::vector<uint32_t> &branchTable, int currPos) {
  // Goto beginning of table

  // For now, call helper function to achieve goal
  cg.i32.const_((int)p);
  cg.i32.const_((int)pOp);
  cg.i32.const_(reinterpret_cast<intptr_t>(&execOpRewind));
  cg.call_indirect({cg.i32, cg.i32}, {cg.i32});

  cg.if_(cg.void_);
  {
    cg.i32.const_((int32_t)&p->pc);
    cg.i32.const_(pOp->p2);
    cg.i32.store(2U, 0U);
    genBranchTo(cg, p, branchTable, currPos, pOp->p2, 1);
  }
  cg.end();
}

// output=r[P1@P2]
void genOpResultRow(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp) {
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
void genOpCopy(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp) {
  // int i
  int i_index = cg.locals().size();
  int increment = 40;  // size of Mem
  cg.local(cg.i32);

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
    cg.i32.const_(reinterpret_cast<intptr_t>(&sqlite3VdbeMemShallowCopy));
    cg.call_indirect({cg.i32, cg.i32, cg.i32}, {});

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

void genOpIf(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp,
             std::vector<uint32_t> &branchTable, int currPos) {
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
    { cg.i32.const_(pOp->p3); }
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
  genBranchTo(cg, p, branchTable, currPos, pOp->p2, 0, true);
}

void genOpDecrJumpZero(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp,
                       std::vector<uint32_t> &branchTable, int currPos) {
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
  genBranchTo(cg, p, branchTable, currPos, pOp->p2, 0, true);
}

// assume not blob
void genOpString(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp) {
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

void genComparisons(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp,
                    std::vector<uint32_t> &branchTable, int currPos) {
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
    cg.i32.const_(reinterpret_cast<intptr_t>(&sqlite3MemCompare));
    cg.call_indirect({cg.i32, cg.i32, cg.i32}, {cg.i32});
    cg.i32.const_(0);
    genComparisonOpCode(cg, pOp->opcode);
  }
  cg.end();

  genBranchTo(cg, p, branchTable, currPos, pOp->p2, 0, true);
}

void genOpAggFinal(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp) {
  Mem *pMem = &p->aMem[pOp->p1];

  if (pOp->p3) {
    cg.i32.const_((intptr_t)pMem);
    cg.i32.const_((intptr_t)&p->aMem[pOp->p3]);
    cg.i32.const_((intptr_t)pOp->p4.pFunc);
    cg.i32.const_(reinterpret_cast<intptr_t>(&sqlite3VdbeMemAggValue));
    cg.call_indirect({cg.i32, cg.i32, cg.i32}, {cg.i32});
    cg.drop();
    cg.i32.const_((intptr_t)&p->aMem[pOp->p3]);
  } else {
    cg.i32.const_((intptr_t)pMem);
    cg.i32.const_((intptr_t)pOp->p4.pFunc);
    cg.i32.const_(reinterpret_cast<intptr_t>(&sqlite3VdbeMemFinalize));
    cg.call_indirect({cg.i32, cg.i32}, {cg.i32});
    cg.drop();
    cg.i32.const_((intptr_t)pMem);
  }
  cg.i32.const_((intptr_t)p->db->enc);
  cg.i32.const_(reinterpret_cast<intptr_t>(&sqlite3VdbeChangeEncoding));
  cg.call_indirect({cg.i32, cg.i32}, {cg.i32});
  cg.drop();
}

// r[P3]=PX cursor P1 column P2
void genOpColumn(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp) {
  cg.i32.const_((int)p);
  cg.i32.const_((int)pOp);
  cg.i32.const_(reinterpret_cast<intptr_t>(&execOpColumn));
  cg.call_indirect({cg.i32, cg.i32}, {cg.i32});
  cg.drop();
}

void genOpFunction(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp) {
  cg.i32.const_((int)p);
  cg.i32.const_((int)pOp);
  cg.i32.const_(reinterpret_cast<intptr_t>(&execOpFunction));
  cg.call_indirect({cg.i32, cg.i32}, {cg.i32});
  cg.drop();
}

void genMathOps(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp) {
  cg.i32.const_((int)&p->aMem[pOp->p1]);
  cg.i32.const_((int)&p->aMem[pOp->p2]);
  cg.i32.const_((int)&p->aMem[pOp->p3]);

  switch (pOp->opcode) {
    case OP_Add:
      cg.i32.const_(reinterpret_cast<intptr_t>(&execOpAdd));
      break;
    case OP_Subtract:
      cg.i32.const_(reinterpret_cast<intptr_t>(&execOpSubtract));
      break;
    case OP_Multiply:
      cg.i32.const_(reinterpret_cast<intptr_t>(&execOpMultiply));
      break;
  }

  cg.call_indirect({cg.i32, cg.i32, cg.i32}, {});
}

void genAggrStepZero(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp) {
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
  genAggrStepOne(cg, p, pOp);
}

void genAggrStepOne(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp) {
  cg.i32.const_((int)p);
  cg.i32.const_((int)pOp);
  cg.i32.const_(reinterpret_cast<intptr_t>(&execAggrStepOne));
  cg.call_indirect({cg.i32, cg.i32}, {});
}

void genOpNext(wasmblr::CodeGenerator &cg, Vdbe *p, Op *pOp,
               std::vector<uint32_t> &branchTable, int currPos) {
  cg.i32.const_((int)p);
  cg.i32.const_((int)pOp);
  cg.i32.const_(reinterpret_cast<intptr_t>(&execOpNext));
  cg.call_indirect({cg.i32, cg.i32}, {cg.i32});
  genBranchTo(cg, p, branchTable, currPos, pOp->p2, 0, true);
}