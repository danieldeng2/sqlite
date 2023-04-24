#include "operations.h"
#include "utils.h"
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

static inline void genReturnAndStartAt(CompilerContext *ctx, int returnValue,
                                       int startIndex) {
  Vdbe *p = ctx->p;
  wasmblr::CodeGenerator &cg = ctx->cg;

  cg.i32.const_((int32_t)&p->pc);
  cg.i32.const_(startIndex);
  cg.i32.store(2U, 0U);
  cg.i32.const_(returnValue);
  cg.return_();
}

static inline void genMainFunction(CompilerContext *ctx) {
  std::vector<CodeBlock> codeBlocks = ctx->codeBlocks;
  std::vector<uint32_t> branchTable = ctx->branchTable;
  Vdbe *p = ctx->p;
  wasmblr::CodeGenerator &cg = ctx->cg;

  cg.local(cg.i32);
  cg.local(cg.i32);

  cg.i32.const_((int32_t)&p->pc);
  cg.i32.load(2U, 0U);
  cg.local.set(0);

  cg.loop(cg.void_);
  for (int i = 0; i < codeBlocks.size(); i++) {
    cg.block(cg.void_);
  }
  cg.local.get(0);
  cg.br_table(branchTable, codeBlocks.size());
  cg.end();

  for (int blockIndex = 0; blockIndex < codeBlocks.size(); blockIndex++) {
    CodeBlock codeBlock = codeBlocks[blockIndex];
    for (int i = codeBlock.jumpIn; i <= codeBlock.jumpOut; i++) {
      Op *pOp = &p->aOp[i];

      // for debugging
      // if (i == 19){
      //     genReturnAndStartAt(ctx, 100000 + pOp->opcode, i);
      //     break;
      // }
      // cg.i32.const_(100000 + i);
      // cg.drop();
      switch (pOp->opcode) {
        case OP_Noop:
          break;
        case OP_Init:
          genOpInit(ctx, pOp, i);
          break;
        case OP_Return:
          genOpReturn(ctx, pOp, i);
          break;
        case OP_Goto:
          genOpGoto(ctx, pOp, i);
          break;
        case OP_Gosub:
          genOpGoSub(ctx, pOp, i);
          break;
        case OP_Halt:
          genReturnAndStartAt(ctx, SQLITE_DONE, i);
          break;
        case OP_If:
        case OP_IfNot:
          genOpIf(ctx, pOp, i);
          break;
        case OP_IsNull:
          genOpIsNull(ctx, pOp, i);
          break;
        case OP_IdxLE:
        case OP_IdxGT:
        case OP_IdxLT:
        case OP_IdxGE:
          genIdxComparisons(ctx, pOp, i);
          break;
        case OP_Transaction:
          genOpTransaction(ctx, pOp);
          break;
        case OP_Integer:
          genOpInteger(ctx, pOp);
          break;
        case OP_Real:
          genOpReal(ctx, pOp);
          break;
        case OP_BeginSubrtn:
        case OP_Null:
          genOpNull(ctx, pOp);
          break;
        case OP_NullRow:
          genOpNullRow(ctx, pOp);
          break;
        case OP_Once:
          genOpOnce(ctx, pOp, i);
          break;
        case OP_OpenRead:
        case OP_OpenWrite:
          genOpReadOpWrite(ctx, pOp);
          break;
        case OP_SorterOpen:
          genOpSorterOpen(ctx, pOp);
          break;
        case OP_SorterSort:
        case OP_Sort:
        case OP_Rewind:
          genOpRewind(ctx, pOp, i);
          break;
        case OP_Column:
          genOpColumn(ctx, pOp);
          break;
        case OP_Function:
          genOpFunction(ctx, pOp);
          break;
        case OP_Add:
        case OP_Subtract:
        case OP_Multiply:
          genMathOps(ctx, pOp);
          break;
        case OP_ResultRow:
          genOpResultRow(ctx, pOp);
          genReturnAndStartAt(ctx, SQLITE_ROW, i + 1);
          break;
        case OP_Move:
          genOpMove(ctx, pOp);
          break;
        case OP_Copy:
          genOpCopy(ctx, pOp);
          break;
        case OP_SCopy:
          genOpSCopy(ctx, pOp);
          break;
        case OP_IdxInsert:
          genOpIdxInsert(ctx, pOp);
          break;
        case OP_DecrJumpZero:
          genOpDecrJumpZero(ctx, pOp, i);
          break;
        case OP_Next:
          genOpNext(ctx, pOp, i);
          break;
        case OP_SorterNext:
          genOpSorterNext(ctx, pOp, i);
          break;
        case OP_String8:
          pOp->p1 = 0x3fffffff & (int)strlen(pOp->p4.z);
          pOp->opcode = OP_String;
          genOpString(ctx, pOp);
          break;
        case OP_String:
          genOpString(ctx, pOp);
          break;
        case OP_Eq:
        case OP_Ne:
        case OP_Lt:
        case OP_Le:
        case OP_Gt:
        case OP_Ge:
          genComparisons(ctx, pOp, i);
          break;
        case OP_AggStep:
          genAggrStepZero(ctx, pOp);
          break;
        case OP_AggStep1:
          genAggrStepOne(ctx, pOp);
          break;
        case OP_AggFinal:
          genOpAggFinal(ctx, pOp);
          break;
        case OP_MakeRecord:
          genMakeRecord(ctx, pOp);
          break;
        case OP_SorterInsert:
          genOpSorterInsert(ctx, pOp);
          break;
        case OP_OpenPseudo:
          genOpenPseudo(ctx, pOp);
          break;
        case OP_SorterData:
          genSorterData(ctx, pOp);
          break;
        case OP_Compare:
          genOpCompare(ctx, pOp, pOp[-1].p4.ai + 1);
          break;
        case OP_Jump:
          genOpJump(ctx, pOp, i);
          break;
        case OP_IfPos:
          genOpIfPos(ctx, pOp, i);
          break;
        case OP_SeekRowid:
          genOpSeekRowid(ctx, pOp, i);
          break;
        case OP_Rowid:
          genOpRowid(ctx, pOp);
          break;
        case OP_DeferredSeek:
          genDeferredSeek(ctx, pOp);
          break;
        case OP_Affinity:
          genOpAffinity(ctx, pOp);
          break;
        case OP_Cast:
          genOpCast(ctx, pOp);
          break;
        case OP_OpenEphemeral:
          genOpenEphemeral(ctx, pOp);
          break;
        case OP_SeekLT:
        case OP_SeekLE:
        case OP_SeekGT:
        case OP_SeekGE:
          genSeekComparisons(ctx, pOp, i);
          break;
        default:
          // Return Opcode to notify to implement
          printf("Compiler: Implement OP %d\n", pOp->opcode);
          genReturnAndStartAt(ctx, 100000 + pOp->opcode, i);
      }
    };
    cg.end();
  }
}

std::vector<uint8_t> genProgram(Vdbe *p) {
  wasmblr::CodeGenerator cg;
  CompilerContext ctx = genCompilerContext(cg, p);

  auto main_func = cg.function({}, {cg.i32}, [&]() {
    cg.call(ctx.imports["stackSave"]);
    genMainFunction(&ctx);
    cg.call(ctx.imports["stackRestore"]);
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
