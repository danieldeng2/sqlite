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
int execOpNext(Vdbe* p, Op pOp);
int execOpFunction(Vdbe *p, Op *pOp);
Bool execComparison(Vdbe *p, Op *pOp);
void execAggrStepZero(Vdbe *p, Op *pOp);
void execAggrStepOne(Vdbe *p, Op *pOp);

#ifdef __cplusplus
}
#endif