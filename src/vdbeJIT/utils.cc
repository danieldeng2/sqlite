#include "utils.h"

void genImports(wasmblr::CodeGenerator &cg,
                std::unordered_map<std::string, uint32_t> &imports) {
  cg.memory(0).import_("env", "memory");
  cg.table(0U, cg.funcRef).import_("env", "__indirect_function_table");
  imports["stackSave"] = cg.function({}, {cg.i32}).import_("env", "stackSave");
  imports["stackAlloc"] =
      cg.function({cg.i32}, {cg.i32}).import_("env", "stackAlloc");
  imports["stackRestore"] =
      cg.function({cg.i32}, {}).import_("env", "stackRestore");

  imports["sqlite3InMemSorterInit"] =
      cg.function({cg.i32, cg.i32, cg.i32}, {cg.i32})
          .import_("env", "sqlite3InMemSorterInit");
  imports["sqlite3BtreeBeginTrans"] =
      cg.function({cg.i32, cg.i32, cg.i32}, {cg.i32})
          .import_("env", "sqlite3BtreeBeginTrans");
  imports["execOpenReadWrite"] = cg.function({cg.i32, cg.i32}, {cg.i32})
                                     .import_("env", "execOpenReadWrite");
  imports["execOpRewind"] =
      cg.function({cg.i32, cg.i32}, {cg.i32}).import_("env", "execOpRewind");
  imports["sqlite3VdbeMemShallowCopy"] =
      cg.function({cg.i32, cg.i32, cg.i32}, {})
          .import_("env", "sqlite3VdbeMemShallowCopy");
  imports["sqlite3MemCompare"] = cg.function({cg.i32, cg.i32, cg.i32}, {cg.i32})
                                     .import_("env", "sqlite3MemCompare");
  imports["sqlite3VdbeMemAggValue"] =
      cg.function({cg.i32, cg.i32, cg.i32}, {cg.i32})
          .import_("env", "sqlite3VdbeMemAggValue");
  imports["sqlite3VdbeMemFinalize"] =
      cg.function({cg.i32, cg.i32}, {cg.i32})
          .import_("env", "sqlite3VdbeMemFinalize");
  imports["sqlite3VdbeChangeEncoding"] =
      cg.function({cg.i32, cg.i32}, {cg.i32})
          .import_("env", "sqlite3VdbeChangeEncoding");
  imports["execOpColumn"] =
      cg.function({cg.i32, cg.i32}, {cg.i32}).import_("env", "execOpColumn");
  imports["execOpFunction"] =
      cg.function({cg.i32, cg.i32}, {cg.i32}).import_("env", "execOpFunction");
  imports["execOpAdd"] =
      cg.function({cg.i32, cg.i32, cg.i32}, {}).import_("env", "execOpAdd");
  imports["execOpSubtract"] = cg.function({cg.i32, cg.i32, cg.i32}, {})
                                  .import_("env", "execOpSubtract");
  imports["execOpMultiply"] = cg.function({cg.i32, cg.i32, cg.i32}, {})
                                  .import_("env", "execOpMultiply");
  imports["execOpMakeRecord"] =
      cg.function({cg.i32, cg.i32}, {}).import_("env", "execOpMakeRecord");
  imports["execAggrStepOne"] =
      cg.function({cg.i32, cg.i32}, {}).import_("env", "execAggrStepOne");
  imports["sqlite3BtreeNext"] = cg.function({cg.i32, cg.i32}, {cg.i32})
                                    .import_("env", "sqlite3BtreeNext");
  imports["sqlite3InMemSorterNext"] =
      cg.function({cg.i32, cg.i32}, {cg.i32})
          .import_("env", "sqlite3InMemSorterNext");
  imports["sqlite3InMemSorterWrite"] =
      cg.function({cg.i32, cg.i32}, {cg.i32})
          .import_("env", "sqlite3InMemSorterWrite");
  imports["allocateCursor"] =
      cg.function({cg.i32, cg.i32, cg.i32, cg.i32}, {cg.i32})
          .import_("env", "allocateCursor");
  imports["sqlite3InMemSorterRowkey"] =
      cg.function({cg.i32, cg.i32}, {cg.i32})
          .import_("env", "sqlite3InMemSorterRowkey");
  imports["execOpCompare"] = cg.function({cg.i32, cg.i32, cg.i32}, {cg.i32})
                                 .import_("env", "execOpCompare");
  imports["execOpMove"] =
      cg.function({cg.i32, cg.i32}, {}).import_("env", "execOpMove");
  imports["sqlite3VdbeMemMove"] =
      cg.function({cg.i32, cg.i32}, {}).import_("env", "sqlite3VdbeMemMove");
  imports["execDeferredSeek"] =
      cg.function({cg.i32, cg.i32}, {}).import_("env", "execDeferredSeek");
  imports["execSeekRowid"] =
      cg.function({cg.i32, cg.i32}, {cg.i32}).import_("env", "execSeekRowid");
  imports["execOpRowid"] =
      cg.function({cg.i32, cg.i32}, {}).import_("env", "execOpRowid");
  imports["execOpAffinity"] =
      cg.function({cg.i32, cg.i32}, {}).import_("env", "execOpAffinity");
  imports["execSeekComparisons"] = cg.function({cg.i32, cg.i32}, {cg.i32})
                                       .import_("env", "execSeekComparisons");
  imports["sqlite3VdbeMemCast"] =
      cg.function({cg.i32, cg.i32, cg.i32}, {cg.i32})
          .import_("env", "sqlite3VdbeMemCast");
  imports["execOpOpenEphemeral"] =
      cg.function({cg.i32, cg.i32}, {}).import_("env", "execOpOpenEphemeral");
  imports["execOpNullRow"] =
      cg.function({cg.i32, cg.i32}, {}).import_("env", "execOpNullRow");
  imports["execOpIdxInsert"] =
      cg.function({cg.i32, cg.i32}, {}).import_("env", "execOpIdxInsert");
  imports["sqlite3BtreeInsert"] =
      cg.function({cg.i32, cg.i32, cg.i32, cg.i32}, {cg.i32})
          .import_("env", "sqlite3BtreeInsert");
  imports["execIdxComparisons"] = cg.function({cg.i32, cg.i32}, {cg.i32})
                                      .import_("env", "execIdxComparisons");
}

void getCodeBlocks(Vdbe *p, std::vector<CodeBlock> &result) {
  // is it possible to jump to current location
  bool isJumpIn[p->nOp];

  for (int i = 0; i < p->nOp; i++) {
    isJumpIn[i] = false;
  }

  bool hasOpReturn = false;

  for (int i = 0; i < p->nOp; i++) {
    Op pOp = p->aOp[i];
    switch (pOp.opcode) {
      case OP_Init:
        isJumpIn[i] = true;
        isJumpIn[pOp.p2] = true;
        break;
      case OP_ResultRow:
        isJumpIn[i + 1] = true;
        break;
      case OP_Return:
        hasOpReturn = true;
        break;
      case OP_Jump:
        isJumpIn[pOp.p1] = true;
        isJumpIn[pOp.p2] = true;
        isJumpIn[pOp.p3] = true;
        break;
      case OP_SeekLT:
      case OP_SeekLE:
      case OP_SeekGT:
      case OP_SeekGE:
        isJumpIn[i + 2] = true;
        isJumpIn[pOp.p2] = true;
      case OP_Goto:
      case OP_If:
      case OP_Eq:
      case OP_Ne:
      case OP_Lt:
      case OP_Le:
      case OP_Gt:
      case OP_Ge:
      case OP_Next:
      case OP_Gosub:
      case OP_Once:
      case OP_Rewind:
      case OP_DecrJumpZero:
      case OP_IfPos:
      case OP_IsNull:
      case OP_SeekRowid:
      case OP_IdxLE:
      case OP_IdxGT:
      case OP_IdxLT:
      case OP_IdxGE:
        isJumpIn[pOp.p2] = true;
        break;
    }
  }

  // Op return can jump to arbitrary address, therefore we cannot do analysis
  if (hasOpReturn) {
    for (int i = 0; i < p->nOp; i++) {
      isJumpIn[i] = true;
    }
  }

  CodeBlock curr;
  for (int i = 0; i < p->nOp; i++) {
    if (isJumpIn[i]) {
      curr.jumpIn = i;
    }
    if (i == p->nOp - 1 || isJumpIn[i + 1]) {
      curr.jumpOut = i;
      result.emplace_back(curr);
    }
  }
}

void getBranchTable(std::vector<CodeBlock> &codeBlocks,
                    std::vector<uint32_t> &result, int nOp) {
  int blockIndex = 0;
  for (int i = 0; i < nOp; i++) {
    if (i > codeBlocks[blockIndex].jumpOut) blockIndex++;
    result.emplace_back(blockIndex);
  }
}

CompilerContext genCompilerContext(wasmblr::CodeGenerator &cg, Vdbe *p) {
  CompilerContext ctx(cg);
  genImports(cg, ctx.imports);
  getCodeBlocks(p, ctx.codeBlocks);
  getBranchTable(ctx.codeBlocks, ctx.branchTable, p->nOp);
  ctx.p = p;
  return ctx;
}