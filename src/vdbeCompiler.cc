#include <map>
#include <set>

#include "sqliteInt.h"
#include "vdbeInt.h"
#include "wasmblr.h"

typedef int (*jitOp)();

// TODO: replace with assembly
void execOpAdd(Mem *pIn1, Mem *pIn2, Mem *pOut) {
  u16 flag;
  if ((pIn1->flags & pIn2->flags & MEM_Int) != 0) {
    pOut->u.i = pIn1->u.i + pIn2->u.i;
    flag = MEM_Int;
  } else {
    pOut->u.r = pIn1->u.r + pIn2->u.r;
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
    pOut->u.r = pIn2->u.r - pIn1->u.r;
    flag = MEM_Real;
  }
  pOut->flags = (pOut->flags & ~(MEM_TypeMask | MEM_Zero)) | flag;
}
void genOpRewind(wasmblr::CodeGenerator &cg, Vdbe *p, Op pOp, int stackAlloc,
                 int func_two_zero, int nextPc) {
  VdbeCursor *pC = p->apCsr[pOp.p1];
  BtCursor *pCrsr = pC->uc.pCursor;

  // assume currently not sorter
  cg.i32.const_((int)pCrsr);

  // int res;
  cg.i32.const_(4);
  cg.call(stackAlloc);
  cg.local(cg.i32);
  int pResPointer = cg.locals().size() - 1;
  cg.local.tee(pResPointer);

  // sqlite3BtreeFirst(pCrsr, &res)
  cg.i32.const_(reinterpret_cast<intptr_t>(&execBtreeFirst));
  cg.call_indirect(func_two_zero);

  // pC->nullRow = (u8)res;
  cg.i32.const_(pC->nullRow);
  cg.local.get(pResPointer);
  cg.i32.load(2U, 0U);
  cg.i32.store8(0U, 0U);

  // pC->deferredMoveto = 0;
  cg.i32.const_(0);
  cg.i32.const_(pC->deferredMoveto);
  cg.i32.store8(0U, 0U);

  // pC->cacheStatus = CACHE_STALE;
  cg.i32.const_(CACHE_STALE);
  cg.i32.const_(pC->cacheStatus);
  cg.i32.store(2U, 0U);

  // if( res ) goto jump_to_p2;
  cg.i32.const_((int32_t)&p->pc);
  cg.local.get(pResPointer);
  cg.i32.load(2U, 0U);

  cg.if_(cg.i32);
  { cg.i32.const_(pOp.p2); }
  cg.else_();
  { cg.i32.const_(nextPc); }
  cg.end();
  cg.i32.store(2U, 0U);
}

uint32_t genRelocations(wasmblr::CodeGenerator &cg, void *jit_address) {
  return cg.function({}, {}, [&]() {
    uint32_t count = 1;
    uint32_t base = 0;

    cg.local(cg.i32);
    cg.refNull(cg.funcRef);
    cg.i32.const_(count);
    cg.table.grow(0);
    cg.local.set(base);

    cg.local.get(base);
    cg.i32.const_(0);
    cg.i32.const_(count);
    cg.table.init(0, 0);

    cg.i32.const_(reinterpret_cast<intptr_t>(jit_address));
    cg.local.get(base);
    cg.i32.const_(0);
    cg.i32.add();
    cg.i32.store(2U);
  });
}

std::vector<uint8_t> genFunction(Vdbe *p) {
  wasmblr::CodeGenerator cg;
  cg.memory(0).import_("env", "memory");
  cg.table(0U, cg.funcRef).import_("env", "__indirect_function_table");
  auto stackSave = cg.function({}, {cg.i32}).import_("env", "stackSave");
  auto stackRestore = cg.function({cg.i32}, {}).import_("env", "stackRestore");
  auto stackAlloc =
      cg.function({cg.i32}, {cg.i32}).import_("env", "stackAlloc");

  auto func_two_zero = cg.type_def({cg.i32, cg.i32}, {});
  auto func_two_one = cg.type_def({cg.i32, cg.i32}, {cg.i32});
  auto func_three_zero = cg.type_def({cg.i32, cg.i32, cg.i32}, {});

  auto main_func = cg.function({}, {cg.i32}, [&]() {
    cg.call(stackSave);
    cg.loop(cg.void_);
    for (int i = 0; i < p->nOp; i++) {
      cg.block(cg.void_);
    }

    cg.i32.const_((int32_t)&p->pc);
    cg.i32.load(2U, 0U);
    std::vector<uint32_t> labelidxs;
    for (int i = 0; i < p->nOp; i++) labelidxs.emplace_back(i);
    cg.br_table(labelidxs, p->nOp);
    cg.end();

    for (int i = 0; i < p->nOp; i++) {
      Op *pOp = &p->aOp[i];
      int nextPc = i + 1;
      int returnValue = -1;
      bool conditionalJump = false;

      switch (pOp->opcode) {
        case OP_Init: {
          // For self altering instructions, treat the address as parameter
          cg.i32.const_((int)&pOp->p1);
          cg.i32.const_(1);
          cg.i32.store();
        }
        case OP_Goto: {  // GOTO P2
          nextPc = pOp->p2;
          break;
        }
        case OP_Halt: {  // return code P1
          // returnValue = pOp->p1;
          returnValue = SQLITE_DONE;
          break;
        }
        case OP_Transaction: {  // Begin a transaction on database P1
          int iMeta = 0;
          sqlite3 *db = p->db;
          Db *pDb = &db->aDb[pOp->p1];
          Btree *pBt = pDb->pBt;

          cg.i32.const_((int)pBt);
          cg.i32.const_((int)pOp->p2);
          cg.i32.const_(reinterpret_cast<intptr_t>(&beginTransaction));
          cg.call_indirect(func_two_zero);
          break;
        }
        case OP_Integer: {  // r[P2]=P1
          Mem *pOut = &p->aMem[pOp->p2];

          cg.i32.const_((int)&pOut->u.i);
          cg._i64.const_((i64)pOp->p1);
          cg._i64.store();
          cg.i32.const_((int)&pOut->flags);
          cg.i32.const_(MEM_Int);
          cg.i32.store16();
          break;
        }
        case OP_Real: {  // r[P2]=P4
          Mem *pOut = &p->aMem[pOp->p2];

          cg.i32.const_((int)&pOut->u.r);
          cg.i32.const_((int)pOp->p4.pReal);
          cg.f64.load();
          cg.f64.store();
          cg.i32.const_((int)&pOut->flags);
          cg.i32.const_(MEM_Real);
          cg.i32.store16();
          break;
        }
        case OP_Null: {
          u16 nullFlag = pOp->p1 ? (MEM_Null|MEM_Cleared) : MEM_Null;

          for (int j = pOp->p2; j <= pOp->p3; j++){
            Mem *pOut = &p->aMem[j];
            cg.i32.const_((int)&pOut->n);
            cg.i32.const_(0);
            cg.i32.store();

            cg.i32.const_((int)&pOut->flags);
            cg.i32.const_(nullFlag);
            cg.i32.store16();
          }
          break;
        }
        case OP_OpenRead:
        case OP_OpenWrite: {
          cg.i32.const_((int)p);
          cg.i32.const_((int)pOp);
          cg.i32.const_(reinterpret_cast<intptr_t>(&execOpenReadWrite));
          cg.call_indirect(func_two_one);
          cg.drop();
          break;
        }
        case OP_Rewind: {  // Goto beginning of table
          conditionalJump = true;
          cg.i32.const_((int32_t)&p->pc);

          // For now, call helper function to achieve goal
          cg.i32.const_((int)p);
          cg.i32.const_((int)pOp);
          cg.i32.const_(reinterpret_cast<intptr_t>(&execOpRewind));
          cg.call_indirect(func_two_one);

          cg.if_(cg.i32);
          { cg.i32.const_(pOp->p2); }
          cg.else_();
          { cg.i32.const_(nextPc); }
          cg.end();
          cg.i32.store(2U, 0U);
          break;
        }
        case OP_Column: {  // r[P3]=PX cursor P1 column P2
          cg.i32.const_((int)p);
          cg.i32.const_((int)pOp);
          cg.i32.const_(reinterpret_cast<intptr_t>(&execOpColumn));
          cg.call_indirect(func_two_one);
          cg.drop();
          break;
        }
        case OP_Add: {  // r[P3]=r[P1]+r[P2]
          cg.i32.const_((int)&p->aMem[pOp->p1]);
          cg.i32.const_((int)&p->aMem[pOp->p2]);
          cg.i32.const_((int)&p->aMem[pOp->p3]);
          cg.i32.const_(reinterpret_cast<intptr_t>(&execOpAdd));
          cg.call_indirect(func_three_zero);
          break;
        }
        case OP_Subtract: {  // r[P3]=r[P1]+r[P2]
          cg.i32.const_((int)&p->aMem[pOp->p1]);
          cg.i32.const_((int)&p->aMem[pOp->p2]);
          cg.i32.const_((int)&p->aMem[pOp->p3]);
          cg.i32.const_(reinterpret_cast<intptr_t>(&execOpSubtract));
          cg.call_indirect(func_three_zero);
          break;
        }
        case OP_ResultRow: {  // output=r[P1@P2]
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
          returnValue = SQLITE_ROW;
          break;
        }
        case OP_DecrJumpZero: {
          cg.i32.const_((int32_t)&p->aMem[pOp->p1].u.i);
          cg.i32.const_((int32_t)&p->aMem[pOp->p1].u.i);
          cg.i32.load(2U, 0U);
          cg.i32.const_(1);
          cg.i32.sub();
          cg.i32.store();

          conditionalJump = true;
          cg.i32.const_((int32_t)&p->pc);
          cg.i32.const_((int32_t)&p->aMem[pOp->p1].u.i);
          cg.i32.load(2U, 0U);

          cg.if_(cg.i32);
          { cg.i32.const_(nextPc); }
          cg.else_();
          { cg.i32.const_(pOp->p2); }
          cg.end();
          cg.i32.store(2U, 0U);
          break;
        }
        case OP_Next: {
          conditionalJump = true;
          cg.i32.const_((int)p);
          cg.i32.const_((int)pOp);
          cg.i32.const_(reinterpret_cast<intptr_t>(&execOpNext));
          cg.call_indirect(func_two_one);
          cg.drop();
          break;
        }
        default: {
          // Return Opcode to notify to implement
          nextPc = i;
          returnValue = 100000 + pOp->opcode;
        }
      }

      if (!conditionalJump) {
        cg.i32.const_((int32_t)&p->pc);
        cg.i32.const_(nextPc);
        cg.i32.store(2U, 0U);
      }

      if (returnValue < 0) {
        cg.br(p->nOp - i - 1);
      } else {
        cg.i32.const_(returnValue);
        cg.return_();
      }
      cg.end();
    };

    cg.call(stackRestore);
    cg.i32.const_(SQLITE_OK);
  });

  auto relocations = genRelocations(cg, &(p->jitCode));
  cg.start(relocations);
  cg.elem(main_func);
  return cg.emit();
}

extern "C" {

int sqlite3VdbeExecJIT(Vdbe *p) {
  int rc;
  if (p->jitCode == NULL) {
    rc = sqlite3VdbeExec(p);
  } else {
    printf("p: %d \n", (int)p);
    rc = ((jitOp)p->jitCode)();

    if (rc > 100000) {
      printf("TODO: Implement OP %d \n", rc - 100000);
      rc = sqlite3VdbeExec(p);
    }
  }
  // printf("pResultRow: %lld\n", p->pResultRow->u.i);
  // printf("rc: %d\n", rc);
  return rc;
}

struct WasmModule {
  std::vector<uint8_t> data;
};

WasmModule *jitStatement(Vdbe *p) {
  std::vector<uint8_t> result = genFunction(p);
  return new WasmModule{result};
}

uint8_t *moduleData(WasmModule *mod) { return mod->data.data(); }
size_t moduleSize(WasmModule *mod) { return mod->data.size(); }
void freeModule(WasmModule *mod) { delete mod; }
}
