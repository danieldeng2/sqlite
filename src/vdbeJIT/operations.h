#pragma once

#include "sqliteInt.h"
#include "vdbeInt.h"
#include "wasmblr.h"

void genOpInit(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
               std::vector<uint32_t>& branchTable, int currPos);
void genOpGoto(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
               std::vector<uint32_t>& branchTable, int currPos);

void genOpIf(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
             std::vector<uint32_t>& branchTable, int currPos);

void genOpTransaction(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
                      uint32_t stackAlloc);
void genOpInteger(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpReal(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpNull(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpOnce(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
               std::vector<uint32_t>& branchTable, int currPos);

void genOpReadOpWrite(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpSorterOpen(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpRewind(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
                 std::vector<uint32_t>& branchTable, int currPos);

void genOpResultRow(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpCopy(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpDecrJumpZero(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
                       std::vector<uint32_t>& branchTable, int currPos);

void genOpString(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genComparisons(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
                    std::vector<uint32_t>& branchTable, int currPos);

void genOpAggFinal(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpColumn(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpFunction(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genMathOps(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genMakeRecord(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpSorterInsert(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpenPseudo(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genSorterData(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpCompare(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp, u32* aPermute);

void genOpJump(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
               std::vector<uint32_t>& branchTable, int currPos);

void genOpMove(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpIfPos(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
                std::vector<uint32_t>& branchTable, int currPos);

void genDeferredSeek(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpSeekRowid(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
                    std::vector<uint32_t>& branchTable, int currPos);

void genOpRowid(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpAffinity(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genSeekComparisons(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
                        std::vector<uint32_t>& branchTable, int currPos);

void genOpCast(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genAggrStepZero(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genAggrStepOne(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpNext(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
               std::vector<uint32_t>& branchTable, int currPos);

void genOpSorterNext(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
                     std::vector<uint32_t>& branchTable, int currPos);

void genOpGoSub(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
                std::vector<uint32_t>& branchTable, int currPos);

void genOpReturn(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
                 std::vector<uint32_t>& branchTable, int currPos);
