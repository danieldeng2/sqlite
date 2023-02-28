// #include "sqliteInt.h"
// #include "vdbeInt.h"

#include "wasmblr.h"

std::vector<uint8_t> gen_add() {
  wasmblr::CodeGenerator cg;
  auto add_func = cg.function({cg.f32, cg.f32}, {cg.f32}, [&]() {
    cg.local.get(0);
    cg.local.get(1);
    cg.f32.add();
  });
  cg.export_(add_func, "add");

  return cg.emit();
}

extern "C" {
uint8_t* jit_add() {
  auto bytes = gen_add();
  uint8_t* out = (uint8_t*)malloc(bytes.size());
  memcpy(out, bytes.data(), bytes.size());
  return out;
}

int jit_add_len() {
  auto bytes = gen_add();
  return bytes.size();
}
}
