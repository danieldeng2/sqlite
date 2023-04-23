#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "sqliteInt.h"
#include "vdbeInt.h"

void execOpAdd(Mem *pIn1, Mem *pIn2, Mem *pOut);
void execOpSubtract(Mem *pIn1, Mem *pIn2, Mem *pOut);
void execOpMultiply(Mem *pIn1, Mem *pIn2, Mem *pOut);
int execOpenReadWrite(Vdbe *p, Op *pOp);
int execOpRewind(Vdbe *p, Op *pOp);
int execOpColumn(Vdbe *p, Op *pOp);
int execOpFunction(Vdbe *p, Op *pOp);
void execAggrStepOne(Vdbe *p, Op *pOp);
void execOpMakeRecord(Vdbe *p, Op *pOp);
void execOpMove(Vdbe *p, Op *pOp);
int execOpCompare(Vdbe *p, Op *pOp, u32 *aPermute);
void execDeferredSeek(Vdbe *p, Op *pOp);
Bool execSeekRowid(Vdbe *p, Op *pOp);

void execOpRowid(Vdbe *p, Op *pOp);
void execOpAffinity(Vdbe *p, Op *pOp);
int execSeekComparisons(Vdbe *p, Op *pOp);

#ifdef __cplusplus
}
#endif