#include "analysis.h"

std::vector<CodeBlock> *getCodeBlocks(Vdbe *p) {
  // is it possible to jump to current location
  bool isJumpIn[p->nOp];

  for (int i = 0; i < p->nOp; i++) {
    isJumpIn[i] = false;
  }

  for (int i = 0; i < p->nOp; i++) {
    Op pOp = p->aOp[i];
    switch (pOp.opcode) {
      case OP_Init:
        isJumpIn[i] = true;
        isJumpIn[pOp.p2] = true;
      case OP_Goto:
        isJumpIn[pOp.p2] = true;
      case OP_ResultRow:
        isJumpIn[i + 1] = true;
        // conditional jumps
      case OP_If:
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
        isJumpIn[pOp.p2] = true;
    }
  }

  std::vector<CodeBlock> *result = new std::vector<CodeBlock>;
  CodeBlock curr;
  for (int i = 0; i < p->nOp; i++){
    if (isJumpIn[i]){
      curr.jumpIn = i;
    }
    if (i == p->nOp - 1 || isJumpIn[i + 1]){
      curr.jumpOut = i;
      result->emplace_back(curr);
    }
  }
  
  return result;
}

std::vector<uint32_t> *getBranchTable(std::vector<CodeBlock> codeBlocks, int nOp){
  std::vector<uint32_t> *result = new std::vector<uint32_t>;
  int blockIndex = 0;
  for (int i = 0; i < nOp; i++) {
    if (i > codeBlocks[blockIndex].jumpOut) blockIndex++;
    result->emplace_back(blockIndex);
  }
  return result;
}