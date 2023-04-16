#include <map>
#include <set>

#include "sqliteInt.h"
#include "vdbeInt.h"
#include "wasmblr.h"

typedef int (*jitOp)();
static std::set<Vdbe *> jitCandidates;

__attribute__((optnone)) void doSomething(int value) { printf("%d", value); }
__attribute__((optnone)) void doSomethingElse(int value) {
  printf("%d", value);
}

__attribute__((noinline)) void genGoto(int *init_pc) {
  while (1) {
    switch (*init_pc) {
      case 0: {
        doSomethingElse(123123);
        *init_pc = 1111111;
        break;
      }
      case 1: {
        doSomething(123123);
        *init_pc = 1111111;
        break;
      }
      case 2: {
        doSomething(321312);
        *init_pc = 2222222;
        break;
      }
      case 3: {
        doSomethingElse(213213);
        *init_pc = 3333333;
        break;
      }
    }
  }
}

std::vector<uint8_t> genFunction(Vdbe *p) {
  wasmblr::CodeGenerator cg;
  cg.memory(0).import_("env", "memory");
  cg.table(0U, cg.funcRef).import_("env", "__indirect_function_table");
  auto stackSave = cg.function({}, {cg.i32}).import_("env", "stackSave");
  auto stackRestore = cg.function({cg.i32}, {}).import_("env", "stackRestore");
  auto stackAlloc =
      cg.function({cg.i32}, {cg.i32}).import_("env", "stackAlloc");
  auto begin_transaction_type = cg.type_def({cg.i32, cg.i32}, {});
  auto block_type = cg.type_def({}, {});

  auto main_func = cg.function({}, {cg.i32}, [&]() {
    cg.call(stackSave);
    cg.loop(cg.void_);
    for (int i = 0; i < p->nOp; i++) {
      cg.block(cg.void_);
    }

    cg.i32.const_((int32_t)&p->pc);
    cg.i32.load(2U, 0U);
    std::vector<uint32_t> labelidxs;
    for (int i = 0; i < p->nOp; i++) labelidxs.emplace_back(i);
    cg.br_table(labelidxs, p->nOp);
    cg.end();

    for (int i = 0; i < p->nOp; i++) {
      Op pOp = p->aOp[i];
      int nextPc = i + 1;
      int returnValue = -1;

      switch (pOp.opcode) {
        case OP_Init:
        case OP_Goto: {  // GOTO P2
          nextPc = pOp.p2;
          break;
        }
        case OP_Halt: {  // return code P1
          returnValue = pOp.p1;
          break;
        }
        case OP_Transaction: {  // Begin a transaction on database P1
          int iMeta = 0;
          sqlite3 *db = p->db;
          Db *pDb = &db->aDb[pOp.p1];
          Btree *pBt = pDb->pBt;

          cg.i32.const_((int)pBt);
          cg.i32.const_((int)pOp.p2);
          cg.i32.const_(reinterpret_cast<intptr_t>(&beginTransaction));
          cg.call_indirect(begin_transaction_type);
          break;
        }
        case OP_Integer: {  // r[P2]=P1
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

      cg.i32.const_((int32_t)&p->pc);
      cg.i32.const_(nextPc);
      cg.i32.store(2U, 0U);

      if (returnValue < 0) {
        cg.br(p->nOp - i - 1);
      } else {
        cg.i32.const_(returnValue);
        cg.return_();
      }
      cg.end();
    };

    cg.call(stackRestore);
    cg.i32.const_(SQLITE_OK);
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
    int additionResult = ((jitOp)p->jitCode)();
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
