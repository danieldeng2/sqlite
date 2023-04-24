#pragma once

#include "sqliteInt.h"
#include "utils.h"
#include "vdbeInt.h"
#include "wasmblr.h"

void genOpInit(CompilerContext* ctx, Op* pOp, int currPos);
void genOpGoto(CompilerContext* ctx, Op* pOp, int currPos);

void genOpIf(CompilerContext* ctx, Op* pOp, int currPos);

void genOpTransaction(CompilerContext* ctx, Op* pOp);
void genOpInteger(CompilerContext* ctx, Op* pOp);

void genOpReal(CompilerContext* ctx, Op* pOp);

void genOpNull(CompilerContext* ctx, Op* pOp);

void genOpOnce(CompilerContext* ctx, Op* pOp, int currPos);

void genOpReadOpWrite(CompilerContext* ctx, Op* pOp);

void genOpSorterOpen(CompilerContext* ctx, Op* pOp);

void genOpRewind(CompilerContext* ctx, Op* pOp, int currPos);

void genOpResultRow(CompilerContext* ctx, Op* pOp);

void genOpCopy(CompilerContext* ctx, Op* pOp);

void genOpSCopy(CompilerContext* ctx, Op* pOp);

void genOpDecrJumpZero(CompilerContext* ctx, Op* pOp, int currPos);

void genOpString(CompilerContext* ctx, Op* pOp);

void genComparisons(CompilerContext* ctx, Op* pOp, int currPos);

void genOpAggFinal(CompilerContext* ctx, Op* pOp);

void genOpColumn(CompilerContext* ctx, Op* pOp);

void genOpFunction(CompilerContext* ctx, Op* pOp);

void genMathOps(CompilerContext* ctx, Op* pOp);

void genMakeRecord(CompilerContext* ctx, Op* pOp);

void genOpSorterInsert(CompilerContext* ctx, Op* pOp);

void genOpenPseudo(CompilerContext* ctx, Op* pOp);

void genSorterData(CompilerContext* ctx, Op* pOp);

void genOpCompare(CompilerContext* ctx, Op* pOp, u32* aPermute);

void genOpJump(CompilerContext* ctx, Op* pOp, int currPos);

void genOpMove(CompilerContext* ctx, Op* pOp);

void genOpIfPos(CompilerContext* ctx, Op* pOp, int currPos);

void genDeferredSeek(CompilerContext* ctx, Op* pOp);

void genOpSeekRowid(CompilerContext* ctx, Op* pOp, int currPos);

void genOpRowid(CompilerContext* ctx, Op* pOp);

void genOpAffinity(CompilerContext* ctx, Op* pOp);

void genSeekComparisons(CompilerContext* ctx, Op* pOp, int currPos);

void genOpCast(CompilerContext* ctx, Op* pOp);

void genOpenEphemeral(CompilerContext* ctx, Op* pOp);

void genOpNullRow(CompilerContext* ctx, Op* pOp);

void genOpIdxInsert(CompilerContext* ctx, Op* pOp);

void genOpIsNull(CompilerContext* ctx, Op* pOp, int currPos);

void genIdxComparisons(CompilerContext* ctx, Op* pOp, int currPos);

void genAggrStepZero(CompilerContext* ctx, Op* pOp);

void genAggrStepOne(CompilerContext* ctx, Op* pOp);

void genOpNext(CompilerContext* ctx, Op* pOp, int currPos);

void genOpSorterNext(CompilerContext* ctx, Op* pOp, int currPos);

void genOpGoSub(CompilerContext* ctx, Op* pOp, int currPos);

void genOpReturn(CompilerContext* ctx, Op* pOp, int currPos);
