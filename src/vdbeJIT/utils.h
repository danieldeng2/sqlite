#pragma once

#include <cstdint>
#include <vector>

#include "sqliteInt.h"
#include "vdbeInt.h"
#include "wasmblr.h"

typedef struct {
  int jumpIn;
  int jumpOut;
} CodeBlock;

struct CompilerContext {
  wasmblr::CodeGenerator &cg;
  Vdbe *p;
  std::unordered_map<std::string, uint32_t> imports;
  std::vector<uint32_t> branchTable;
  std::vector<CodeBlock> codeBlocks;
  CompilerContext(wasmblr::CodeGenerator& cg_) : cg(cg_) {}
};

CompilerContext genCompilerContext(wasmblr::CodeGenerator &cg, Vdbe *p);
