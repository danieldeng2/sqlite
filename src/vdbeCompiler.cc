#include <map>
#include <set>

#include "sqliteInt.h"
#include "vdbeInt.h"
#include "wasmblr.h"

typedef int (*jitOp)(Vdbe *);
static std::set<Vdbe *> jitCandidates;

std::vector<uint8_t> genFunction(Vdbe *p) {
  wasmblr::CodeGenerator cg;
  cg.memory(0).import_("env", "memory");
  cg.table(0U, cg.funcRef).import_("env", "__indirect_function_table");

  auto main_func = cg.function({cg.i32}, {cg.i32}, [&]() {
    std::map<int, int> jump_labels;
    int beginning = 0;

    for (int i = 0; i < p->nOp; i++) {
      Op pOp = p->aOp[i];
      switch (pOp.opcode) {
        case OP_Init:
          // cg.block(beginning);
        case OP_Goto:  // GOTO P2
          // for now, act under assumption of a linear flow
          i = pOp.p2 - 1;
          break;
        case OP_Halt:
          // We've reached the end of code generation
          i = p->nOp;
          break;
        case OP_Transaction:
          break;
        case OP_Integer:  // r[P2]=P1
          Op pOp = p->aOp[i];
          Mem *pOut = &p->aMem[pOp.p2];

          cg.i32.const_((int)pOut);
          cg._i64.const_((i64)pOp.p1);
          cg._i64.store(3U, (int)&pOut->u.i - (int)pOut);
          cg.i32.const_((int)pOut);
          cg.i32.const_(MEM_Int);
          cg.i32.store16(1U, (int)&pOut->flags - (int)pOut);
          break;
      }
    }

    cg.i32.const_(0);
  });

  auto relocations = cg.function({}, {}, [&]() {
    uint32_t count = 1;
    uint32_t base = 0;

    cg.local(cg.i32);
    cg.refNull(cg.funcRef);
    cg.i32.const_(count);
    cg.table.grow(0);
    cg.local.set(base);

    cg.local.get(base);
    cg.i32.const_(0);
    cg.i32.const_(count);
    cg.table.init(0, 0);

    cg.i32.const_(reinterpret_cast<intptr_t>(&(p->jitCode)));
    cg.local.get(base);
    cg.i32.const_(0);
    cg.i32.add();
    cg.i32.store(2U);
  });
  cg.start(relocations);
  cg.elem(main_func);

  return cg.emit();
}

extern "C" {

int sqlite3VdbeExecJIT(Vdbe *p) {
  if (p->jitCode == NULL) {
    if (jitCandidates.find(p) == jitCandidates.end()) {
      jitCandidates.insert(p);
      fprintf(stdout, "appending to jit candidates\n");
    }
  } else {
    int additionResult = ((jitOp)p->jitCode)(p);
    fprintf(stdout, "result: %d\n", additionResult);
  }
  return sqlite3VdbeExec(p);
}

struct WasmModule {
  std::vector<uint8_t> data;
};

WasmModule *jitModule() {
  if (jitCandidates.empty()) return NULL;

  std::vector<uint8_t> result;

  for (Vdbe *p : jitCandidates) result = genFunction(p);

  jitCandidates.clear();
  return new WasmModule{result};
}

uint8_t *moduleData(WasmModule *mod) { return mod->data.data(); }
size_t moduleSize(WasmModule *mod) { return mod->data.size(); }
void freeModule(WasmModule *mod) { delete mod; }
}
