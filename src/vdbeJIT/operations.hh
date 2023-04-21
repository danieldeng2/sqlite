#pragma once

#include "sqliteInt.h"
#include "vdbeInt.h"
#include "wasmblr.h"

void genOpInit(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp, int branchChecker);
void genOpGoto(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp, int branchChecker);
void genOpTransaction(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
                      uint32_t stackAlloc);
void genOpInteger(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpReal(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpNull(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpOnce(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp, int branchChecker);

void genOpReadOpWrite(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpRewind(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp, int branchChecker);

void genOpResultRow(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpCopy(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpDecrJumpZero(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp,
                       int branchChecker);

void genOpString(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genComparisons(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp, int branchChecker);

void genOpAggFinal(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpColumn(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpFunction(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genMathOps(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genAggrStepZero(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genAggrStepOne(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp);

void genOpNext(wasmblr::CodeGenerator& cg, Vdbe* p, Op* pOp, int branchChecker);
