#pragma once

#include <vector>
#include "sqliteInt.h"
#include "vdbeInt.h"

typedef struct {
  int jumpIn;
  int jumpOut;
} CodeBlock;

std::vector<CodeBlock>* getCodeBlocks(Vdbe* p);
