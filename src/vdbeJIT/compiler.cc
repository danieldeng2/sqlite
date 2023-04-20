#include <binaryen-c.h>

#include "operations.hh"
#include "wasmblr.h"

static inline BinaryenFunctionRef genRelocation(BinaryenModuleRef module,
                                                void* jit_address) {
  BinaryenType params = BinaryenTypeCreate({}, 0);
  BinaryenType results = BinaryenTypeCreate({}, 0);
  BinaryenType localtypes[1] = {BinaryenTypeInt32()};

  BinaryenExpressionRef valueExpr =
      BinaryenRefNull(module, BinaryenTypeNullFuncref());
  BinaryenExpressionRef sizeExpr =
      BinaryenConst(module, BinaryenLiteralInt32(1));
  BinaryenExpressionRef tableGrow =
      BinaryenTableGrow(module, "0", valueExpr, sizeExpr);

  BinaryenExpressionRef tableInit = BinaryenTableInit(
      module, "0", "0", BinaryenLocalGet(module, 0, BinaryenTypeInt32()),
      BinaryenConst(module, BinaryenLiteralInt32(0)),
      BinaryenConst(module, BinaryenLiteralInt32(1)));
  BinaryenExpressionRef body[] = {
      BinaryenLocalSet(module, 0, tableGrow), tableInit,
      BinaryenStore(
          module, 4, 0, 0,
          BinaryenConst(module, BinaryenLiteralInt32((intptr_t)jit_address)),
          BinaryenLocalGet(module, 0, BinaryenTypeInt32()), BinaryenTypeInt32(),
          "0")};

  BinaryenExpressionRef bodyBlock =
      BinaryenBlock(module, NULL, body, sizeof(body) / sizeof(int), NULL);

  BinaryenFunctionRef relocation = BinaryenAddFunction(
      module, "relocations", params, results, localtypes, 1, bodyBlock);

  return relocation;
}

static inline BinaryenFunctionRef genMainFunction(BinaryenModuleRef module,
                                                  Vdbe* p) {
  BinaryenType params = BinaryenTypeCreate({}, 0);
  BinaryenType results = BinaryenTypeInt32();

  BinaryenExpressionRef x = BinaryenConst(module, BinaryenLiteralInt32(69));
  BinaryenExpressionRef y = BinaryenConst(module, BinaryenLiteralInt32(420));
  BinaryenExpressionRef add = BinaryenBinary(module, BinaryenAddInt32(), x, y);

  BinaryenFunctionRef adder =
      BinaryenAddFunction(module, "mainFunction", params, results, NULL, 0, add);

  return adder;
}

BinaryenModuleRef genModule(Vdbe* p) {
  BinaryenModuleRef module = BinaryenModuleCreate();

  BinaryenAddMemoryImport(module, "0", "env", "memory", 0);
  BinaryenAddTableImport(module, "0", "env", "__indirect_function_table");
  BinaryenAddFunctionImport(module, "stackSave", "env", "stackSave",
                            BinaryenTypeCreate({}, 0), BinaryenTypeInt32());
  BinaryenAddFunctionImport(module, "stackRestore", "env", "stackRestore",
                            BinaryenTypeInt32(), BinaryenTypeCreate({}, 0));
  BinaryenAddFunctionImport(module, "stackAlloc", "env", "stackAlloc",
                            BinaryenTypeInt32(), BinaryenTypeInt32());

  BinaryenFunctionRef relocation = genRelocation(module, &(p->jitCode));
  genMainFunction(module, p);

  const char* funcNames[] = {"mainFunction"};

  BinaryenAddPassiveElementSegment(module, "0", funcNames, 1);
  BinaryenSetStart(module, relocation);

  return module;
}

static inline void genMainFunction(wasmblr::CodeGenerator& cg, Vdbe* p,
                                   uint32_t stackAlloc) {
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
    Op* pOp = &p->aOp[i];
    int nextPc = i + 1;
    int returnValue = -1;
    bool conditionalJump = false;

    switch (pOp->opcode) {
      case OP_Init:
        genOpInit(cg, pOp);
        nextPc = pOp->p2;
        break;
      case OP_Goto:
        nextPc = pOp->p2;
        break;
      case OP_Halt:
        returnValue = SQLITE_DONE;
        break;
      case OP_Transaction:
        genOpTransaction(cg, p, pOp, stackAlloc);
        break;
      case OP_Integer:
        genOpInteger(cg, p, pOp);
        break;
      case OP_Real:
        genOpReal(cg, p, pOp);
        break;
      case OP_Null:
        genOpNull(cg, p, pOp);
        break;
      case OP_Once:
        conditionalJump = true;
        genOpOnce(cg, p, pOp, nextPc);
        break;
      case OP_OpenRead:
      case OP_OpenWrite:
        genOpReadOpWrite(cg, p, pOp);
        break;
      case OP_Rewind:
        conditionalJump = true;
        genOpRewind(cg, p, pOp, nextPc);
        break;
      case OP_Column:
        genOpColumn(cg, p, pOp);
        break;
      case OP_Function:
        genOpFunction(cg, p, pOp);
        break;
      case OP_Add:
      case OP_Subtract:
      case OP_Multiply:
        genMathOps(cg, p, pOp);
        break;
      case OP_ResultRow:
        genOpResultRow(cg, p, pOp);
        returnValue = SQLITE_ROW;
        break;
      case OP_Copy:
        genOpCopy(cg, p, pOp);
        break;
      case OP_DecrJumpZero:
        conditionalJump = true;
        genOpDecrJumpZero(cg, p, pOp, nextPc);
        break;
      case OP_Next:
        conditionalJump = true;
        genOpNext(cg, p, pOp);
        break;
      case OP_String8:
        pOp->p1 = 0x3fffffff & (int)strlen(pOp->p4.z);
        pOp->opcode = OP_String;
        genOpString(cg, p, pOp);
        break;
      case OP_String:
        genOpString(cg, p, pOp);
        break;
      case OP_Eq:
      case OP_Ne:
      case OP_Lt:
      case OP_Le:
      case OP_Gt:
      case OP_Ge:
        conditionalJump = true;
        genComparisons(cg, p, pOp);
        break;
      case OP_AggStep:
        genAggrStepZero(cg, p, pOp);
        break;
      case OP_AggStep1:
        genAggrStepOne(cg, p, pOp);
        break;
      case OP_AggFinal:
        genOpAggFinal(cg, p, pOp);
        break;
      default:
        // Return Opcode to notify to implement
        nextPc = i;
        returnValue = 100000 + pOp->opcode;
    }

    if (!conditionalJump) {
      cg.i32.const_((int32_t)&p->pc);
      cg.i32.const_(nextPc);
      cg.i32.store(2U, 0U);
    }

    if (returnValue < 0) {
      cg.br(p->nOp - i - 1);
    } else {
      cg.i32.const_(returnValue);
      cg.return_();
    }
    cg.end();
  };
}

extern "C" {
typedef BinaryenModuleAllocateAndWriteResult WasmBinary;

WasmBinary* jitStatement(Vdbe* p) {
  WasmBinary* result = (WasmBinary*)malloc(sizeof(WasmBinary));
  BinaryenModuleRef module = genModule(p);
  *result = BinaryenModuleAllocateAndWrite(module, NULL);
  BinaryenModuleDispose(module);
  return result;
}

uint8_t* moduleData(WasmBinary* result) { return (uint8_t*)(result->binary); }
size_t moduleSize(WasmBinary* result) { return result->binaryBytes; }
void freeModule(WasmBinary* result) {
  free(result->binary);
  free(result);
}
}
