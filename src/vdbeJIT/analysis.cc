#include "analysis.h"

__attribute__((optnone)) 
std::vector<CodeBlock> *getCodeBlocks(Vdbe *p) {
  bool isJumpIn[p->nOp];
  bool isJumpOut[p->nOp];

  for (int i = 0; i < p->nOp; i++) {
    isJumpIn[i] = false;
    isJumpOut[i] = false;
  }
  isJumpOut[p->nOp - 1] = true;

  for (int i = 0; i < p->nOp; i++) {
    Op pOp = p->aOp[i];
    switch (pOp.opcode) {
      case OP_Init:
        isJumpIn[i] = true;
        isJumpOut[i] = true;
        isJumpOut[pOp.p2 - 1] = true;
        isJumpIn[pOp.p2] = true;
      case OP_Goto:
        isJumpOut[i] = true;
        isJumpOut[pOp.p2 - 1] = true;
        isJumpIn[pOp.p2] = true;
      case OP_Halt:
        isJumpOut[i] = true;
      case OP_ResultRow:
        isJumpOut[i] = true;
        isJumpIn[i + 1] = true;
        // conditional jumps
      case OP_Eq:
      case OP_Ne:
      case OP_Lt:
      case OP_Le:
      case OP_Gt:
      case OP_Ge:
      case OP_Next:
      case OP_Once:
      case OP_Rewind:
      case OP_DecrJumpZero:
        isJumpOut[i] = true;
        isJumpIn[i + 1] = true;
        isJumpOut[pOp.p2 - 1] = true;
        isJumpIn[pOp.p2] = true;
    }
  }

  std::vector<CodeBlock> *result = new std::vector<CodeBlock>;
  CodeBlock curr;
  for (int i = 0; i < p->nOp; i++){
    if (isJumpIn[i]){
      curr.jumpIn = i;
    }
    if (isJumpOut[i]){
      curr.jumpOut = i;
      result->emplace_back(curr);
    }
  }
  
  return result;
}