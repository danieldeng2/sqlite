#include "operations.hh"
#include "wasmblr.h"

static inline uint32_t genRelocations(wasmblr::CodeGenerator &cg,
                                      void *jit_address) {
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

static inline void genMainFunction(wasmblr::CodeGenerator &cg, Vdbe *p,
                                   uint32_t stackAlloc) {
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
      case OP_Init:
        genOpInit(cg, pOp);
        nextPc = pOp->p2;
        break;
      case OP_Goto:
        nextPc = pOp->p2;
        break;
      case OP_Halt:
        returnValue = SQLITE_DONE;
        break;
      case OP_Transaction:
        genOpTransaction(cg, p, pOp, stackAlloc);
        break;
      case OP_Integer:
        genOpInteger(cg, p, pOp);
        break;
      case OP_Real:
        genOpReal(cg, p, pOp);
        break;
      case OP_Null:
        genOpNull(cg, p, pOp);
        break;
      case OP_Once:
        conditionalJump = true;
        genOpOnce(cg, p, pOp, nextPc);
        break;
      case OP_OpenRead:
      case OP_OpenWrite:
        genOpReadOpWrite(cg, p, pOp);
        break;
      case OP_Rewind:
        conditionalJump = true;
        genOpRewind(cg, p, pOp, nextPc);
        break;
      case OP_Column:
        genOpColumn(cg, p, pOp);
        break;
      case OP_Function:
        genOpFunction(cg, p, pOp);
        break;
      case OP_Add:
      case OP_Subtract:
      case OP_Multiply:
        genMathOps(cg, p, pOp);
        break;
      case OP_ResultRow:
        genOpResultRow(cg, p, pOp);
        returnValue = SQLITE_ROW;
        break;
      case OP_Copy:
        genOpCopy(cg, p, pOp);
        break;
      case OP_DecrJumpZero:
        conditionalJump = true;
        genOpDecrJumpZero(cg, p, pOp, nextPc);
        break;
      case OP_Next:
        conditionalJump = true;
        genOpNext(cg, p, pOp);
        break;
      case OP_String8:
        pOp->p1 = 0x3fffffff & (int)strlen(pOp->p4.z);
        pOp->opcode = OP_String;
        genOpString(cg, p, pOp);
        break;
      case OP_String:
        genOpString(cg, p, pOp);
        break;
      case OP_Eq:
      case OP_Ne:
      case OP_Lt:
      case OP_Le:
      case OP_Gt:
      case OP_Ge:
        conditionalJump = true;
        genComparisons(cg, p, pOp);
        break;
      case OP_AggStep:
        genAggrStepZero(cg, p, pOp);
        break;
      case OP_AggStep1:
        genAggrStepOne(cg, p, pOp);
        break;
      case OP_AggFinal:
        genOpAggFinal(cg, p, pOp);
        break;
      default:
        // Return Opcode to notify to implement
        nextPc = i;
        returnValue = 100000 + pOp->opcode;
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
}

std::vector<uint8_t> genProgram(Vdbe *p) {
  wasmblr::CodeGenerator cg;
  cg.memory(0).import_("env", "memory");
  cg.table(0U, cg.funcRef).import_("env", "__indirect_function_table");
  auto stackSave = cg.function({}, {cg.i32}).import_("env", "stackSave");
  auto stackRestore = cg.function({cg.i32}, {}).import_("env", "stackRestore");
  auto stackAlloc =
      cg.function({cg.i32}, {cg.i32}).import_("env", "stackAlloc");

  auto main_func = cg.function({}, {cg.i32}, [&]() {
    cg.call(stackSave);
    genMainFunction(cg, p, stackAlloc);
    cg.call(stackRestore);
    cg.i32.const_(SQLITE_OK);
  });
  auto relocations = genRelocations(cg, &(p->jitCode));

  cg.start(relocations);
  cg.elem(main_func);
  return cg.emit();
}

extern "C" {
struct WasmModule {
  std::vector<uint8_t> data;
};

WasmModule *jitStatement(Vdbe *p) {
  std::vector<uint8_t> result = genProgram(p);
  return new WasmModule{result};
}

uint8_t *moduleData(WasmModule *mod) { return mod->data.data(); }
size_t moduleSize(WasmModule *mod) { return mod->data.size(); }
void freeModule(WasmModule *mod) { delete mod; }
}
