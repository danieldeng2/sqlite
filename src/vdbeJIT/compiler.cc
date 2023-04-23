#include "analysis.h"
#include "operations.h"
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
    cg.i32.store(2U);
  });
}

static inline void genReturnAndStartAt(wasmblr::CodeGenerator &cg, Vdbe *p,
                                       int returnValue, int startIndex) {
  cg.i32.const_((int32_t)&p->pc);
  cg.i32.const_(startIndex);
  cg.i32.store(2U, 0U);
  cg.i32.const_(returnValue);
  cg.return_();
}

static inline void genMainFunction(wasmblr::CodeGenerator &cg, Vdbe *p,
                                   uint32_t stackAlloc) {
  std::vector<CodeBlock> codeBlocks = *getCodeBlocks(p);
  std::vector<uint32_t> branchTable = *getBranchTable(codeBlocks, p->nOp);
  cg.local(cg.i32);

  cg.loop(cg.void_);
  for (int i = 0; i < codeBlocks.size(); i++) {
    cg.block(cg.void_);
  }

  cg.i32.const_((int32_t)&p->pc);
  cg.i32.load(2U, 0U);

  cg.br_table(branchTable, codeBlocks.size());
  cg.end();

  for (int blockIndex = 0; blockIndex < codeBlocks.size(); blockIndex++) {
    CodeBlock codeBlock = codeBlocks[blockIndex];
    for (int i = codeBlock.jumpIn; i <= codeBlock.jumpOut; i++) {
      Op *pOp = &p->aOp[i];

      // for debugging
      // if (i == 19){
      //     genReturnAndStartAt(cg, p, 100000 + pOp->opcode, i);
      //     break;
      // }
      // cg.i32.const_(100000 + i);
      // cg.drop();
      switch (pOp->opcode) {
        case OP_Noop:
          break;
        case OP_Init:
          genOpInit(cg, p, pOp, branchTable, i);
          break;
        case OP_Return:
          genOpReturn(cg, p, pOp, branchTable, i);
          break;
        case OP_Goto:
          genOpGoto(cg, p, pOp, branchTable, i);
          break;
        case OP_Gosub:
          genOpGoSub(cg, p, pOp, branchTable, i);
          break;
        case OP_Halt:
          genReturnAndStartAt(cg, p, SQLITE_DONE, i);
          break;
        case OP_If:
        case OP_IfNot:
          genOpIf(cg, p, pOp, branchTable, i);
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
          genOpOnce(cg, p, pOp, branchTable, i);
          break;
        case OP_OpenRead:
        case OP_OpenWrite:
          genOpReadOpWrite(cg, p, pOp);
          break;
        case OP_SorterOpen:
          genOpSorterOpen(cg, p, pOp);
          break;
        case OP_SorterSort:
        case OP_Sort:
        case OP_Rewind:
          genOpRewind(cg, p, pOp, branchTable, i);
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
          genReturnAndStartAt(cg, p, SQLITE_ROW, i + 1);
          break;
        case OP_Move:
          genOpMove(cg, p, pOp);
          break;
        case OP_Copy:
          genOpCopy(cg, p, pOp);
          break;
        case OP_DecrJumpZero:
          genOpDecrJumpZero(cg, p, pOp, branchTable, i);
          break;
        case OP_Next:
          genOpNext(cg, p, pOp, branchTable, i);
          break;
        case OP_SorterNext:
          genOpSorterNext(cg, p, pOp, branchTable, i);
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
          genComparisons(cg, p, pOp, branchTable, i);
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
        case OP_MakeRecord:
          genMakeRecord(cg, p, pOp);
          break;
        case OP_SorterInsert:
          genOpSorterInsert(cg, p, pOp);
          break;
        case OP_OpenPseudo:
          genOpenPseudo(cg, p, pOp);
          break;
        case OP_SorterData:
          genSorterData(cg, p, pOp);
          break;
        case OP_Compare:
          genOpCompare(cg, p, pOp, pOp[-1].p4.ai + 1);
          break;
        case OP_Jump:
          genOpJump(cg, p, pOp, branchTable, i);
          break;
        case OP_IfPos:
          genOpIfPos(cg, p, pOp, branchTable, i);
          break;
        case OP_SeekRowid:
          genOpSeekRowid(cg, p, pOp, branchTable, i);
          break;
        case OP_Rowid:
          genOpRowid(cg, p, pOp);
          break;
        case OP_DeferredSeek:
          genDeferredSeek(cg, p, pOp);
          break;
        case OP_Affinity:
          genOpAffinity(cg, p, pOp);
          break;
        case OP_Cast:
          genOpCast(cg, p, pOp);
          break;
        case OP_SeekLT:
        case OP_SeekLE:
        case OP_SeekGT:
        case OP_SeekGE:
          genSeekComparisons(cg, p, pOp, branchTable, i);
          break;
        default:
          // Return Opcode to notify to implement
          printf("Compiler: Implement OP %d\n", pOp->opcode);
          genReturnAndStartAt(cg, p, 100000 + pOp->opcode, i);
      }
    };
    cg.end();
  }
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
