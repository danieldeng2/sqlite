const em = require('./add_jit.js');
em().then(async function(module) {
  Module = module;
  const wasm = Module._jit_add();
  const wasm_len = Module._jit_add_len();
  const wasm_data = new Uint8Array(Module.HEAP8.buffer, wasm, wasm_len);
  const m = await WebAssembly.compile(wasm_data);
  const instance = await WebAssembly.instantiate(m, {});
  // use the function
  console.log(instance.exports.add(8, 10));

});