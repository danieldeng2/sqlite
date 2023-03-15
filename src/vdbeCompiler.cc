#include <set>

#include "sqliteInt.h"
#include "vdbeInt.h"
#include "wasmblr.h"

typedef int (*jitOp)(int, int);
static std::set<Vdbe *> jitCandidates;

std::vector<uint8_t> genAdd(void *call_address) {
  wasmblr::CodeGenerator cg;
  cg.memory(0).import_("env", "memory");
  cg.table(0U, cg.funcRef).import_("env", "__indirect_function_table");

  auto add_func = cg.function({cg.i32, cg.i32}, {cg.i32}, [&]() {
    cg.local.get(0);
    cg.local.get(1);
    cg.i32.add();
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

    cg.i32.const_(reinterpret_cast<intptr_t>(call_address));
    cg.local.get(base);
    cg.i32.const_(0);
    cg.i32.add();
    cg.i32.store(2U);
  });
  cg.start(relocations);
  cg.elem(add_func);

  return cg.emit();
}

extern "C" {

int sqlite3VdbeExecJIT(Vdbe *p) {
  if (p->jitCode == NULL) {
    jitCandidates.insert(p);
    fprintf(stdout, "appending to jit candidates\n");
  } else {
    int additionResult = ((jitOp)p->jitCode)(15, 12);
    fprintf(stdout, "additionResult: %d\n", additionResult);
  }
  return sqlite3VdbeExec(p);

  // Op *aOp = p->aOp;          /* Copy of p->aOp */
  // Op *pOp = aOp;             /* Current operation */
  // int rc = SQLITE_OK;        /* Value to return */
  // sqlite3 *db = p->db;       /* The database */
  // u8 resetSchemaOnFault = 0; /* Reset schema after an error if positive */
  // u8 encoding = ENC(db);     /* The database encoding */
  // int iCompare = 0;          /* Result of last comparison */
  // u64 nVmStep = 0;           /* Number of virtual machine steps */
  // Mem *aMem = p->aMem;       /* Copy of p->aMem */
  // Mem *pIn1 = 0;             /* 1st input operand */
  // Mem *pIn2 = 0;             /* 2nd input operand */
  // Mem *pIn3 = 0;             /* 3rd input operand */
  // Mem *pOut = 0;             /* Output operand */

  // assert(p->rc == SQLITE_OK || (p->rc & 0xff) == SQLITE_BUSY);
  // testcase(p->rc != SQLITE_OK);
  // p->rc = SQLITE_OK;
  // assert(p->bIsReader || p->readOnly != 0);
  // p->iCurrentTime = 0;
  // assert(p->explain == 0);
  // db->busyHandler.nBusy = 0;
  // sqlite3VdbeIOTraceSql(p);

  // for (pOp = &aOp[p->pc]; 1; pOp++) {
  //   nVmStep++;

  //   switch (pOp->opcode) {
  //       // case OP_Goto:
  //       //   pOp = &aOp[pOp->p2 - 1];
  //       //   break;

  //     default:
  //       return sqlite3VdbeExec(p);
  //   }
  // }

  // return 0;
}

struct WasmModule {
  std::vector<uint8_t> data;
};

WasmModule *jitModule() {
  if (jitCandidates.empty()) return NULL;

  std::vector<uint8_t> result;

  for (Vdbe *p : jitCandidates) result = genAdd(&(p->jitCode));

  jitCandidates.clear();
  return new WasmModule{result};
}

uint8_t *moduleData(WasmModule *mod) { return mod->data.data(); }
size_t moduleSize(WasmModule *mod) { return mod->data.size(); }
void freeModule(WasmModule* mod) { delete mod; }
}
