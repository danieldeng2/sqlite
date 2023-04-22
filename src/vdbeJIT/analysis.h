#pragma once

#include <vector>
#include <cstdint>
#include "sqliteInt.h"
#include "vdbeInt.h"

typedef struct {
  int jumpIn;
  int jumpOut;
} CodeBlock;

std::vector<CodeBlock>* getCodeBlocks(Vdbe* p);

std::vector<uint32_t>* getBranchTable(std::vector<CodeBlock> codeBlocks,
                                      int nOp);
