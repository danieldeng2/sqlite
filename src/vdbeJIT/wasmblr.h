#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

namespace wasmblr {

constexpr std::array<uint8_t, 4> magic_module_header = {0x00, 0x61, 0x73, 0x6d};
constexpr std::array<uint8_t, 4> module_version = {0x01, 0x00, 0x00, 0x00};

struct CodeGenerator;

class Local {
 public:
  int operator()(uint8_t type);
  void get(int idx);
  void set(int idx);
  void tee(int idx);

 private:
  Local(CodeGenerator& cg_) : cg(cg_) {}
  CodeGenerator& cg;
  friend CodeGenerator;
};

class RefNull {
 public:
  void operator()(uint8_t type);

 private:
  RefNull(CodeGenerator& cg_) : cg(cg_) {}
  CodeGenerator& cg;
  friend CodeGenerator;
};

class I32 {
 public:
  operator uint8_t();
  void const_(int32_t i);
  void clz();
  void ctz();
  void popcnt();
  void lt_s();
  void lt_u();
  void gt_s();
  void gt_u();
  void le_s();
  void le_u();
  void ge_s();
  void ge_u();
  void add();
  void sub();
  void mul();
  void div_s();
  void div_u();
  void rem_s();
  void rem_u();
  void and_();
  void or_();
  void xor_();
  void shl();
  void shr_s();
  void shr_u();
  void rotl();
  void rotr();
  void eqz();
  void eq();
  void ne();

  void load(uint32_t alignment = 1, uint32_t offset = 0);
  void store(uint32_t alignment = 1, uint32_t offset = 0);

  void load8_s(uint32_t alignment = 1, uint32_t offset = 0);
  void load8_u(uint32_t alignment = 1, uint32_t offset = 0);
  void load16_s(uint32_t alignment = 1, uint32_t offset = 0);
  void load16_u(uint32_t alignment = 1, uint32_t offset = 0);
  void store8(uint32_t alignment = 1, uint32_t offset = 0);
  void store16(uint32_t alignment = 1, uint32_t offset = 0);

 private:
  I32(CodeGenerator& cg_) : cg(cg_) {}
  CodeGenerator& cg;
  friend CodeGenerator;
};

class I64 {
 public:
  operator uint8_t();
  void const_(int64_t i);
  void add();
  void sub();
  void mul();
  void store(uint32_t alignment = 3, uint32_t offset = 0);
  void load(uint32_t alignment = 3, uint32_t offset = 0);
 private:
  I64(CodeGenerator& cg_) : cg(cg_) {}
  CodeGenerator& cg;
  friend CodeGenerator;
};

class F32 {
 public:
  operator uint8_t();
  void const_(float f);
  void eq();
  void ne();
  void lt();
  void gt();
  void le();
  void ge();
  void abs();
  void neg();
  void ceil();
  void floor();
  void trunc();
  void nearest();
  void sqrt();
  void add();
  void sub();
  void mul();
  void div();
  void min();
  void max();
  void copysign();

  void load(uint32_t alignment = 1, uint32_t offset = 0);
  void store(uint32_t alignment = 1, uint32_t offset = 0);

 private:
  F32(CodeGenerator& cg_) : cg(cg_) {}
  CodeGenerator& cg;
  friend CodeGenerator;
};

class F64 {
 public:
  operator uint8_t();
  void eq();
  void ne();
  void lt();
  void gt();
  void le();
  void ge();
  void add();
  void sub();
  void mul();
  void const_(double f);
  void load(uint32_t alignment = 1, uint32_t offset = 0);
  void store(uint32_t alignment = 1, uint32_t offset = 0);
  void convert_i64_s();

 private:
  F64(CodeGenerator& cg_) : cg(cg_) {}
  CodeGenerator& cg;
  friend CodeGenerator;
};

class FuncRef {
 public:
  operator uint8_t();

 private:
  FuncRef(CodeGenerator& cg_) : cg(cg_) {}
  CodeGenerator& cg;
  friend CodeGenerator;
};

class V128 {
 public:
  operator uint8_t();

  void i32x4_extract_lane(uint8_t lane);
  void i32x4_replace_lane(uint8_t lane);
  void f32x4_extract_lane(uint8_t lane);
  void f32x4_replace_lane(uint8_t lane);
  void i32x4_splat();
  void f32x4_splat();
  void i32x4_eq();
  void i32x4_ne();
  void i32x4_lt_s();
  void i32x4_lt_u();
  void i32x4_gt_s();
  void i32x4_gt_u();
  void i32x4_le_s();
  void i32x4_le_u();
  void i32x4_ge_s();
  void i32x4_ge_u();
  void f32x4_eq();
  void f32x4_ne();
  void f32x4_lt();
  void f32x4_gt();
  void f32x4_le();
  void f32x4_ge();
  void not_();
  void any_true();
  void and_();
  void andnot();
  void or_();
  void xor_();
  void i32x4_abs();
  void i32x4_neg();
  void i32x4_all_true();
  void i32x4_bitmask();
  void i32x4_shl();
  void i32x4_shr_s();
  void i32x4_shr_u();
  void i32x4_add();
  void i32x4_sub();
  void i32x4_mul();
  void i32x4_min_s();
  void i32x4_min_u();
  void i32x4_max_s();
  void i32x4_max_u();
  void f32x4_ceil();
  void f32x4_floor();
  void f32x4_trunc();
  void f32x4_nearest();
  void f32x4_abs();
  void f32x4_neg();
  void f32x4_sqrt();
  void f32x4_add();
  void f32x4_sub();
  void f32x4_mul();
  void f32x4_div();
  void f32x4_min();
  void f32x4_max();
  void f32x4_pmin();
  void f32x4_pmax();

  void load(uint32_t alignment = 1, uint32_t offset = 0);
  void load32x2_s(uint32_t alignment = 1, uint32_t offset = 0);
  void load32x2_u(uint32_t alignment = 1, uint32_t offset = 0);
  void load32_splat(uint32_t alignment = 1, uint32_t offset = 0);
  void load32_zero(uint32_t alignment = 1, uint32_t offset = 0);
  void store(uint32_t alignment = 1, uint32_t offset = 0);

 private:
  V128(CodeGenerator& cg_) : cg(cg_) {}
  CodeGenerator& cg;
  friend CodeGenerator;
};

class Table {
 public:
  Table& operator()(uint32_t min, uint8_t type);
  Table& import_(std::string, std::string);
  void grow(uint32_t idx);
  void init(uint32_t dst, uint32_t src);

 private:
  Table(CodeGenerator& cg_) : cg(cg_) {}
  CodeGenerator& cg;
  uint32_t min = 0;
  uint8_t type = 0;
  bool is_shared = false;
  std::string a_string = "";
  std::string b_string = "";
  bool is_import() const { return a_string.size() && b_string.size(); }
  bool is_export() const { return a_string.size() && !b_string.size(); }
  friend CodeGenerator;
};

class Memory {
 public:
  Memory& operator()(uint32_t min);
  Memory& operator()(uint32_t min, uint32_t max);
  Memory& export_(std::string);
  Memory& shared(bool = true);
  Memory& import_(std::string, std::string);
  void size();
  void grow();

 private:
  Memory(CodeGenerator& cg_) : cg(cg_) {}
  CodeGenerator& cg;
  uint32_t min = 0;
  uint32_t max = 0;
  bool is_shared = false;
  std::string a_string = "";
  std::string b_string = "";
  bool is_import() const { return a_string.size() && b_string.size(); }
  bool is_export() const { return a_string.size() && !b_string.size(); }
  friend CodeGenerator;
};

struct Function {
  Function(std::vector<uint8_t> input_types_,
           std::vector<uint8_t> output_types_)
      : input_types(input_types_), output_types(output_types_) {}
  Function(std::vector<uint8_t> input_types_,
           std::vector<uint8_t> output_types_, std::function<void()> body_)
      : input_types(input_types_), output_types(output_types_), body(body_) {}
  std::vector<uint8_t> input_types;
  std::vector<uint8_t> output_types;
  std::function<void()> body;
  std::vector<uint8_t> locals;  // resolved later

  uint32_t import_(std::string, std::string);
  uint32_t import_idx;
  std::string a_string = "";
  std::string b_string = "";
  bool is_import() const { return a_string.size() && b_string.size(); }

  void emit() {
    locals.clear();
    body();
  };
};

struct CodeGenerator {
  // API
  Local local;
  RefNull refNull;

  I64 _i64;
  I32 i32;
  F32 f32;
  F64 f64;
  V128 v128;
  FuncRef funcRef;

  Memory memory;
  Table table;
  uint8_t void_ = 0x40;

  void nop();
  void block(uint8_t type);
  void loop(uint8_t type);
  void if_(uint8_t type);
  void else_();
  void return_();
  void drop();
  void br(uint32_t labelidx);
  void br_table(std::vector<uint32_t> &labelidxs, uint32_t defaultidx);
  void br_if(uint32_t labelidx);
  void end();
  void call(uint32_t funcidx);
  void call_indirect(std::vector<uint8_t> input_types,
                     std::vector<uint8_t> output_types, uint32_t tableidx);

  void export_(uint32_t fn_idx, std::string name);

  void start(uint32_t fn_idx);
  void elem(uint32_t fn_idx);

  // returns function index
  uint32_t function(std::vector<uint8_t> input_types,
                    std::vector<uint8_t> output_types,
                    std::function<void()> body);

  Function& function(std::vector<uint8_t> input_types,
                     std::vector<uint8_t> output_types);

  std::vector<uint8_t> emit();

  // Implementation

  CodeGenerator()
      : local(*this),
        refNull(*this),
        i32(*this),
        _i64(*this),
        f32(*this),
        f64(*this),
        v128(*this),
        funcRef(*this),
        memory(*this),
        table(*this) {}
  CodeGenerator(const CodeGenerator&) = delete;
  CodeGenerator(CodeGenerator&&) = delete;

  std::vector<std::vector<std::vector<uint8_t>>> type_definitions_;
  std::vector<Function> imported_functions_;
  std::vector<Function> functions_;
  std::vector<uint32_t> elem_functions_;
  uint32_t start_function_ = -1;
  std::unordered_map<uint32_t, std::string> exported_functions_;
  Function* cur_function_ = nullptr;
  // cur_bytes_ is used as a temporary storage
  std::vector<uint8_t> cur_bytes_;
  // a running type checker, purely for safety
  std::stack<uint8_t> type_stack_;

  using memarg = std::pair<uint32_t, uint32_t>;

  // From LLVM
  std::vector<uint8_t> encode_signed(int32_t n) {
    std::vector<uint8_t> out;
    auto more = true;
    do {
      uint8_t byte = n & 0x7f;
      n >>= 7;
      more = !((((n == 0) && ((byte & 0x40) == 0)) ||
                ((n == -1) && ((byte & 0x40) != 0))));
      if (more) {
        byte |= 0x80;
      }
      out.emplace_back(byte);
    } while (more);
    return out;
  }

  std::vector<uint8_t> encode_unsigned(uint32_t n) {
    std::vector<uint8_t> out;
    do {
      uint8_t byte = n & 0x7f;
      n >>= 7;
      if (n != 0) {
        byte |= 0x80;
      }
      out.emplace_back(byte);
    } while (n != 0);
    return out;
  }

  std::vector<uint8_t> encode_string(std::string s) {
    std::vector<uint8_t> out;
    out.emplace_back(s.size());
    for (const auto& c : s) {
      out.emplace_back(c);
    }
    return out;
  }

  template <typename T>
  void concat(std::vector<uint8_t>& out, const T& inp) {
    out.insert(out.end(), inp.begin(), inp.end());
  };

  template <typename T>
  void concat(std::vector<Function>& out, const T& inp) {
    out.insert(out.end(), inp.begin(), inp.end());
  };

  int declare_local(uint8_t type) {
    assert(cur_function_);
    int idx = cur_function_->locals.size() + cur_function_->input_types.size();
    cur_function_->locals.emplace_back(type);
    return idx;
  }

  const std::vector<uint8_t>& input_types() {
    assert(cur_function_);
    return cur_function_->input_types;
  }

  const std::vector<uint8_t>& locals() {
    assert(cur_function_);
    return cur_function_->locals;
  }

  void push(uint8_t type) { type_stack_.push(type); };

  uint8_t pop() {
    assert(type_stack_.size() && "popping empty stack");
    auto type = type_stack_.top();
    type_stack_.pop();
    return type;
  };

  void emit(uint8_t byte) { cur_bytes_.emplace_back(byte); }
  void emit(std::vector<uint8_t> bytes) { concat(cur_bytes_, bytes); }
  void emit(const memarg& m) {
    emit(encode_unsigned(std::get<0>(m)));
    emit(encode_unsigned(std::get<1>(m)));
  }
};

inline void RefNull::operator()(uint8_t type) {
  cg.emit(0xd0);  // RefNull OpCode
  cg.emit(type);
};

inline int Local::operator()(uint8_t type) { return cg.declare_local(type); };

inline void Local::set(int idx) {
  auto t = cg.pop();
  const auto& input_types = cg.input_types();
  auto expected_type = [&]() {
    if (idx < input_types.size()) {
      return input_types.at(idx);
    }
    return cg.locals().at(idx - input_types.size());
  }();
  assert(expected_type == t && "can't set local to this value (wrong type)");

  cg.emit(0x21);
  cg.emit(cg.encode_unsigned(idx));
}

inline void Local::get(int idx) {
  const auto& input_types = cg.input_types();
  if (idx < input_types.size()) {
    cg.push(input_types.at(idx));
  } else {
    cg.push(cg.locals().at(idx - input_types.size()));
  }

  cg.emit(0x20);
  cg.emit(cg.encode_unsigned(idx));
}

inline void Local::tee(int idx) {
  auto t = cg.pop();
  const auto& input_types = cg.input_types();
  auto expected_type = [&]() {
    if (idx < input_types.size()) {
      return input_types.at(idx);
    }
    return cg.locals().at(idx - input_types.size());
  }();
  assert(expected_type == t && "can't set local to this value (wrong type)");

  cg.emit(0x22);
  cg.emit(cg.encode_unsigned(idx));
  cg.push(expected_type);
}

inline I32::operator uint8_t() { return 0x7f; }

inline I64::operator uint8_t() { return 0x7e; }

inline void I32::const_(int32_t i) {
  cg.emit(0x41);
  cg.emit(cg.encode_signed(i));
  cg.push(cg.i32);
}

inline void I64::const_(int64_t i) {
  cg.emit(0x42);
  cg.emit(cg.encode_signed(i));
  cg.push(cg._i64);
}

inline F32::operator uint8_t() { return 0x7d; }

inline F64::operator uint8_t() { return 0x7c; }

inline FuncRef::operator uint8_t() { return 0x70; }

inline void F32::const_(float f) {
  cg.emit(0x43);
  uint8_t r[4];
  memcpy(&r, &f, sizeof(float));
  for (auto i = 0; i < 4; ++i) {
    cg.emit(r[i]);
  }
  cg.push(cg.f32);
}

inline void F64::const_(double f) {
  cg.emit(0x44);
  uint8_t r[8];
  memcpy(&r, &f, sizeof(double));
  for (auto i = 0; i < 8; ++i) {
    cg.emit(r[i]);
  }
  cg.push(cg.f64);
}

inline V128::operator uint8_t() { return 0x7b; }

#define UNARY_OP(classname, op, opcode, in_type, out_type) \
  inline void classname::op() {                            \
    bool valid = cg.pop() == cg.in_type;                   \
    assert(valid && "invalid type for " #op);              \
    cg.emit(opcode);                                       \
    cg.push(cg.out_type);                                  \
  }

#define BINARY_OP(classname, op, opcode, type_a, type_b, out_type) \
  inline void classname::op() {                                    \
    bool valid = cg.pop() == cg.type_a && cg.pop() == cg.type_b;   \
    assert(valid && "invalid type for " #op);                      \
    cg.emit(opcode);                                               \
    cg.push(cg.out_type);                                          \
  }

#define LOAD_OP(classname, op, opcode, out_type)                   \
  inline void classname::op(uint32_t alignment, uint32_t offset) { \
    auto idx_type = cg.pop();                                      \
    assert(idx_type == cg.i32);                                    \
    cg.emit(opcode);                                               \
    cg.emit(cg.encode_unsigned(alignment));                        \
    cg.emit(cg.encode_unsigned(offset));                           \
    cg.push(cg.out_type);                                          \
  }

#define STORE_OP(classname, op, opcode)                            \
  inline void classname::op(uint32_t alignment, uint32_t offset) { \
    auto val_type = cg.pop();                                      \
    auto idx_type = cg.pop();                                      \
    assert(idx_type == cg.i32);                                    \
    cg.emit(opcode);                                               \
    cg.emit(cg.encode_unsigned(alignment));                        \
    cg.emit(cg.encode_unsigned(offset));                           \
  }

UNARY_OP(I32, clz, 0x67, i32, i32);
UNARY_OP(I32, ctz, 0x68, i32, i32);
UNARY_OP(I32, popcnt, 0x69, i32, i32);
BINARY_OP(I32, lt_s, 0x48, i32, i32, i32);
BINARY_OP(I32, lt_u, 0x49, i32, i32, i32);
BINARY_OP(I32, gt_s, 0x4a, i32, i32, i32);
BINARY_OP(I32, gt_u, 0x4b, i32, i32, i32);
BINARY_OP(I32, le_s, 0x4c, i32, i32, i32);
BINARY_OP(I32, le_u, 0x4d, i32, i32, i32);
BINARY_OP(I32, ge_s, 0x4e, i32, i32, i32);
BINARY_OP(I32, ge_u, 0x4f, i32, i32, i32);
BINARY_OP(I32, add, 0x6a, i32, i32, i32);
BINARY_OP(I32, sub, 0x6b, i32, i32, i32);
BINARY_OP(I32, mul, 0x6c, i32, i32, i32);
BINARY_OP(I32, div_s, 0x6d, i32, i32, i32);
BINARY_OP(I32, div_u, 0x6e, i32, i32, i32);
BINARY_OP(I32, rem_s, 0x6f, i32, i32, i32);
BINARY_OP(I32, rem_u, 0x70, i32, i32, i32);
BINARY_OP(I32, and_, 0x71, i32, i32, i32);
BINARY_OP(I32, or_, 0x72, i32, i32, i32);
BINARY_OP(I32, xor_, 0x73, i32, i32, i32);
BINARY_OP(I32, shl, 0x74, i32, i32, i32);
BINARY_OP(I32, shr_s, 0x75, i32, i32, i32);
BINARY_OP(I32, shr_u, 0x76, i32, i32, i32);
BINARY_OP(I32, rotl, 0x77, i32, i32, i32);
BINARY_OP(I32, rotr, 0x78, i32, i32, i32);
BINARY_OP(I32, eqz, 0x45, i32, i32, i32);
BINARY_OP(I32, eq, 0x46, i32, i32, i32);
BINARY_OP(I32, ne, 0x47, i32, i32, i32);
LOAD_OP(I32, load, 0x28, i32);
LOAD_OP(I32, load8_s, 0x2c, i32);
LOAD_OP(I32, load8_u, 0x2d, i32);
LOAD_OP(I32, load16_s, 0x2e, i32);
LOAD_OP(I32, load16_u, 0x2f, i32);
STORE_OP(I32, store, 0x36);
STORE_OP(I32, store8, 0x3a);
STORE_OP(I32, store16, 0x3b);

BINARY_OP(I64, add, 0x7c, _i64, _i64, _i64);
BINARY_OP(I64, sub, 0x7d, _i64, _i64, _i64);
BINARY_OP(I64, mul, 0x7e, _i64, _i64, _i64);
LOAD_OP(I64, load, 0x29, _i64);
STORE_OP(I64, store, 0x37);

BINARY_OP(F32, eq, 0x5b, f32, f32, i32);
BINARY_OP(F32, ne, 0x5c, f32, f32, i32);
BINARY_OP(F32, lt, 0x5d, f32, f32, i32);
BINARY_OP(F32, gt, 0x5e, f32, f32, i32);
BINARY_OP(F32, le, 0x5f, f32, f32, i32);
BINARY_OP(F32, ge, 0x60, f32, f32, i32);
UNARY_OP(F32, abs, 0x8B, f32, f32);
UNARY_OP(F32, neg, 0x8C, f32, f32);
UNARY_OP(F32, ceil, 0x8D, f32, f32);
UNARY_OP(F32, floor, 0x8E, f32, f32);
UNARY_OP(F32, trunc, 0x8F, f32, f32);
UNARY_OP(F32, nearest, 0x90, f32, f32);
UNARY_OP(F32, sqrt, 0x91, f32, f32);
BINARY_OP(F32, add, 0x92, f32, f32, f32);
BINARY_OP(F32, sub, 0x93, f32, f32, f32);
BINARY_OP(F32, mul, 0x94, f32, f32, f32);
BINARY_OP(F32, div, 0x95, f32, f32, f32);
BINARY_OP(F32, min, 0x96, f32, f32, f32);
BINARY_OP(F32, max, 0x97, f32, f32, f32);
BINARY_OP(F32, copysign, 0x98, f32, f32, f32);
LOAD_OP(F32, load, 0x2a, f32);
STORE_OP(F32, store, 0x38);

LOAD_OP(F64, load, 0x2b, f64);
STORE_OP(F64, store, 0x39);
BINARY_OP(F64, eq, 0x61, f64, f64, i32);
BINARY_OP(F64, ne, 0x62, f64, f64, i32);
BINARY_OP(F64, lt, 0x63, f64, f64, i32);
BINARY_OP(F64, gt, 0x64, f64, f64, i32);
BINARY_OP(F64, le, 0x65, f64, f64, i32);
BINARY_OP(F64, ge, 0x66, f64, f64, i32);

BINARY_OP(F64, add, 0xa0, f64, f64, f64);
BINARY_OP(F64, sub, 0xa1, f64, f64, f64);
BINARY_OP(F64, mul, 0xa2, f64, f64, f64);
UNARY_OP(F64, convert_i64_s, 0xb9, _i64, f64);

#undef UNARY_OP
#undef BINARY_OP
#undef LOAD_OP
#undef STORE_OP

#define VECTOR_LOAD(op, vopcode)                              \
  inline void V128::op(uint32_t alignment, uint32_t offset) { \
    auto idx_type = cg.pop();                                 \
    assert(idx_type == cg.i32);                               \
    cg.emit(0xfd);                                            \
    cg.emit(cg.encode_unsigned(vopcode));                     \
    cg.emit(cg.encode_unsigned(alignment));                   \
    cg.emit(cg.encode_unsigned(offset));                      \
    cg.push(cg.v128);                                         \
  }

VECTOR_LOAD(load, 0);
VECTOR_LOAD(load32x2_s, 5);
VECTOR_LOAD(load32x2_u, 6);
VECTOR_LOAD(load32_splat, 9);
VECTOR_LOAD(load32_zero, 92);

inline void V128::store(uint32_t alignment, uint32_t offset) {
  auto val_type = cg.pop();
  assert(val_type == cg.v128);
  auto idx_type = cg.pop();
  assert(idx_type == cg.i32);
  cg.emit(0xfd);
  cg.emit(cg.encode_unsigned(11));
  cg.emit(cg.encode_unsigned(alignment));
  cg.emit(cg.encode_unsigned(offset));
}

inline void V128::i32x4_extract_lane(uint8_t lane) {
  auto val_type = cg.pop();
  assert(val_type == cg.v128);
  cg.emit(0xfd);
  cg.emit(cg.encode_unsigned(27));
  cg.emit(lane);
  cg.push(cg.i32);
}

inline void V128::f32x4_extract_lane(uint8_t lane) {
  auto val_type = cg.pop();
  assert(val_type == cg.v128);
  cg.emit(0xfd);
  cg.emit(cg.encode_unsigned(31));
  cg.emit(lane);
  cg.push(cg.f32);
}

inline void V128::i32x4_replace_lane(uint8_t lane) {
  auto val_type = cg.pop();
  assert(val_type == cg.i32);
  auto vec_type = cg.pop();
  assert(vec_type == cg.v128);
  cg.emit(0xfd);
  cg.emit(cg.encode_unsigned(28));
  cg.emit(lane);
  cg.push(cg.v128);
}

inline void V128::f32x4_replace_lane(uint8_t lane) {
  auto val_type = cg.pop();
  assert(val_type == cg.f32);
  auto vec_type = cg.pop();
  assert(vec_type == cg.v128);
  cg.emit(0xfd);
  cg.emit(cg.encode_unsigned(32));
  cg.emit(lane);
  cg.push(cg.v128);
}

inline void V128::i32x4_splat() {
  auto val_type = cg.pop();
  assert(val_type == cg.i32);
  cg.emit(0xfd);
  cg.emit(cg.encode_unsigned(17));
  cg.push(cg.v128);
}

inline void V128::f32x4_splat() {
  auto val_type = cg.pop();
  assert(val_type == cg.f32);
  cg.emit(0xfd);
  cg.emit(cg.encode_unsigned(19));
  cg.push(cg.v128);
}

#define VECTOR_BINARY_OP(op, vopcode, a_type, b_type, out_type) \
  inline void V128::op() {                                      \
    auto b = cg.pop();                                          \
    assert(cg.b_type == b);                                     \
    auto a = cg.pop();                                          \
    assert(cg.a_type == a);                                     \
    cg.emit(0xfd);                                              \
    cg.emit(cg.encode_unsigned(vopcode));                       \
    cg.push(cg.out_type);                                       \
  }

#define VECTOR_UNARY_OP(op, vopcode, inp_type, out_type) \
  inline void V128::op() {                               \
    auto inp = cg.pop();                                 \
    assert(cg.inp_type == inp);                          \
    cg.emit(0xfd);                                       \
    cg.emit(cg.encode_unsigned(vopcode));                \
    cg.push(cg.out_type);                                \
  }

VECTOR_BINARY_OP(i32x4_eq, 55, v128, v128, v128);
VECTOR_BINARY_OP(i32x4_ne, 56, v128, v128, v128);
VECTOR_BINARY_OP(i32x4_lt_s, 57, v128, v128, v128);
VECTOR_BINARY_OP(i32x4_lt_u, 58, v128, v128, v128);
VECTOR_BINARY_OP(i32x4_gt_s, 59, v128, v128, v128);
VECTOR_BINARY_OP(i32x4_gt_u, 60, v128, v128, v128);
VECTOR_BINARY_OP(i32x4_le_s, 61, v128, v128, v128);
VECTOR_BINARY_OP(i32x4_le_u, 62, v128, v128, v128);
VECTOR_BINARY_OP(i32x4_ge_s, 63, v128, v128, v128);
VECTOR_BINARY_OP(i32x4_ge_u, 64, v128, v128, v128);

VECTOR_BINARY_OP(f32x4_eq, 65, v128, v128, v128);
VECTOR_BINARY_OP(f32x4_ne, 66, v128, v128, v128);
VECTOR_BINARY_OP(f32x4_lt, 67, v128, v128, v128);
VECTOR_BINARY_OP(f32x4_gt, 68, v128, v128, v128);
VECTOR_BINARY_OP(f32x4_le, 69, v128, v128, v128);
VECTOR_BINARY_OP(f32x4_ge, 70, v128, v128, v128);

VECTOR_UNARY_OP(not_, 77, v128, v128);
VECTOR_UNARY_OP(any_true, 83, v128, i32);

// TODO
// VECTOR_TERNARY_OP(bitselect, 82, v128);

VECTOR_BINARY_OP(and_, 78, v128, v128, v128);
VECTOR_BINARY_OP(andnot, 79, v128, v128, v128);
VECTOR_BINARY_OP(or_, 80, v128, v128, v128);
VECTOR_BINARY_OP(xor_, 81, v128, v128, v128);

VECTOR_UNARY_OP(i32x4_abs, 160, v128, v128);
VECTOR_UNARY_OP(i32x4_neg, 161, v128, v128);
VECTOR_UNARY_OP(i32x4_all_true, 163, v128, i32);
VECTOR_UNARY_OP(i32x4_bitmask, 164, v128, i32);
VECTOR_BINARY_OP(i32x4_shl, 171, v128, i32, v128);
VECTOR_BINARY_OP(i32x4_shr_s, 172, v128, i32, v128);
VECTOR_BINARY_OP(i32x4_shr_u, 173, v128, i32, v128);
VECTOR_BINARY_OP(i32x4_add, 174, v128, v128, v128);
VECTOR_BINARY_OP(i32x4_sub, 177, v128, v128, v128);
VECTOR_BINARY_OP(i32x4_mul, 181, v128, v128, v128);
VECTOR_BINARY_OP(i32x4_min_s, 182, v128, v128, v128);
VECTOR_BINARY_OP(i32x4_min_u, 183, v128, v128, v128);
VECTOR_BINARY_OP(i32x4_max_s, 184, v128, v128, v128);
VECTOR_BINARY_OP(i32x4_max_u, 185, v128, v128, v128);

VECTOR_UNARY_OP(f32x4_ceil, 103, v128, v128);
VECTOR_UNARY_OP(f32x4_floor, 104, v128, v128);
VECTOR_UNARY_OP(f32x4_trunc, 105, v128, v128);
VECTOR_UNARY_OP(f32x4_nearest, 106, v128, v128);
VECTOR_UNARY_OP(f32x4_abs, 224, v128, v128);
VECTOR_UNARY_OP(f32x4_neg, 225, v128, v128);
VECTOR_UNARY_OP(f32x4_sqrt, 227, v128, v128);
VECTOR_BINARY_OP(f32x4_add, 228, v128, v128, v128);
VECTOR_BINARY_OP(f32x4_sub, 229, v128, v128, v128);
VECTOR_BINARY_OP(f32x4_mul, 230, v128, v128, v128);
VECTOR_BINARY_OP(f32x4_div, 231, v128, v128, v128);
VECTOR_BINARY_OP(f32x4_min, 232, v128, v128, v128);
VECTOR_BINARY_OP(f32x4_max, 233, v128, v128, v128);
VECTOR_BINARY_OP(f32x4_pmin, 234, v128, v128, v128);
VECTOR_BINARY_OP(f32x4_pmax, 235, v128, v128, v128);

inline Memory& Memory::operator()(uint32_t min_) {
  assert(min == 0 && max == 0);
  min = min_;
  return *this;
}

inline Memory& Memory::operator()(uint32_t min_, uint32_t max_) {
  assert(min == 0 && max == 0);
  min = min_;
  max = max_;
  return *this;
}

inline Memory& Memory::export_(std::string a) {
  assert(!(is_import() || is_export()) && "already set");
  a_string = a;
  return *this;
}

inline Memory& Memory::shared(bool make_shared) {
  is_shared = make_shared;
  return *this;
}

inline Memory& Memory::import_(std::string a, std::string b) {
  assert(!(is_import() || is_export()) && "already set");
  a_string = a;
  b_string = b;
  return *this;
}

inline void Memory::size() {
  cg.emit(0x3f);
  cg.emit(0x00);
}
inline void Memory::grow() {
  cg.emit(0x40);
  cg.emit(0x00);
}

inline Table& Table::operator()(uint32_t min_, uint8_t type_) {
  assert(min == 0 && type == 0);
  min = min_;
  type = type_;
  return *this;
}

inline Table& Table::import_(std::string a, std::string b) {
  assert(!(is_import() || is_export()) && "already set");
  a_string = a;
  b_string = b;
  return *this;
}

inline void Table::grow(uint32_t idx) {
  cg.emit(0xfc);  // FC extensions opcode
  cg.emit(0x0f);  // tableGrow opcode
  cg.emit(cg.encode_unsigned(idx));
}

inline void Table::init(uint32_t dst, uint32_t src) {
  cg.emit(0xfc);  // FC extensions opcode
  cg.emit(0x0c);  // tableinit opcode
  cg.emit(cg.encode_unsigned(dst));
  cg.emit(cg.encode_unsigned(src));
}

inline void CodeGenerator::nop() { emit(0x01); }
inline void CodeGenerator::block(uint8_t type) {
  emit(0x02);
  emit(type);
}
inline void CodeGenerator::loop(uint8_t type) {
  emit(0x03);
  emit(type);
}

inline void CodeGenerator::return_() { emit(0x0f); }

inline void CodeGenerator::drop() { emit(0x1a); }

inline void CodeGenerator::if_(uint8_t type) {
  auto t = pop();
  assert(t == i32);
  emit(0x04);
  emit(type);
}
inline void CodeGenerator::else_() { emit(0x05); }
inline void CodeGenerator::br(uint32_t labelidx) {
  emit(0x0c);
  emit(encode_unsigned(labelidx));
}
inline void CodeGenerator::br_if(uint32_t labelidx) {
  auto t = pop();
  assert(t == i32);
  emit(0x0d);
  emit(encode_unsigned(labelidx));
}
inline void CodeGenerator::br_table(std::vector<uint32_t> &labelidxs,
                                    uint32_t defaultidx) {
  auto t = pop();
  assert(t == i32);
  emit(0x0e);
  emit(encode_unsigned(labelidxs.size()));
  for (uint32_t labelidx : labelidxs) {
    emit(encode_unsigned(labelidx));
  }
  emit(encode_unsigned(defaultidx));
}

inline void CodeGenerator::end() { emit(0x0b); }
inline void CodeGenerator::call(uint32_t fn_idx) {
  assert(fn_idx < functions_.size() + imported_functions_.size());
  emit(0x10);
  emit(encode_unsigned(fn_idx));
}

inline void CodeGenerator::call_indirect(std::vector<uint8_t> input_types,
                                         std::vector<uint8_t> output_types,
                                         uint32_t tableidx = 0) {
  uint32_t typeidx;

  std::vector<std::vector<uint8_t>> new_type{input_types, output_types};
  auto iterator =
      std::find(type_definitions_.begin(), type_definitions_.end(), new_type);

  if (iterator != type_definitions_.end()) {
    typeidx = iterator - type_definitions_.begin();
  } else {
    typeidx = type_definitions_.size();
    type_definitions_.emplace_back(new_type);
  }

  emit(0x11);
  emit(encode_unsigned(typeidx));
  emit(encode_unsigned(tableidx));
}

inline void CodeGenerator::export_(uint32_t fn, std::string name) {
  exported_functions_[fn] = name;
}

inline void CodeGenerator::elem(uint32_t fn) {
  elem_functions_.emplace_back(fn);
}

inline void CodeGenerator::start(uint32_t fn) { start_function_ = fn; }

inline uint32_t CodeGenerator::function(std::vector<uint8_t> input_types,
                                        std::vector<uint8_t> output_types,
                                        std::function<void()> body) {
  auto idx = functions_.size();
  functions_.emplace_back(input_types, output_types, body);
  return imported_functions_.size() + idx;
}

inline Function& CodeGenerator::function(std::vector<uint8_t> input_types,
                                         std::vector<uint8_t> output_types) {
  imported_functions_.emplace_back(input_types, output_types);
  imported_functions_[imported_functions_.size() - 1].import_idx =
      imported_functions_.size() - 1;
  return imported_functions_[imported_functions_.size() - 1];
}

inline uint32_t Function::import_(std::string a, std::string b) {
  assert(!(is_import()) && "already set");
  a_string = a;
  b_string = b;
  return import_idx;
}

inline std::vector<uint8_t> CodeGenerator::emit() {
  cur_bytes_.clear();
  std::vector<uint8_t> emitted_bytes;

  concat(emitted_bytes, magic_module_header);
  concat(emitted_bytes, module_version);

  std::vector<uint8_t> type_section_bytes;
  std::vector<Function> all_functions;
  concat(all_functions, imported_functions_);
  concat(all_functions, functions_);

  std::vector<uint8_t> code_section_bytes;
  concat(code_section_bytes, encode_unsigned(functions_.size()));
  for (auto& f : functions_) {
    cur_function_ = &f;

    cur_bytes_.clear();
    f.emit();
    end();
    std::vector<uint8_t> body_bytes = cur_bytes_;

    cur_bytes_.clear();
    concat(cur_bytes_, encode_unsigned(f.locals.size()));
    for (const auto& l : f.locals) {
      emit(0x1);
      emit(l);
    }

    std::vector<uint8_t> header_bytes = cur_bytes_;
    auto fn_size = header_bytes.size() + body_bytes.size();

    concat(code_section_bytes, encode_unsigned(fn_size));
    concat(code_section_bytes, header_bytes);
    concat(code_section_bytes, body_bytes);
  }
  cur_function_ = nullptr;

  concat(type_section_bytes,
         encode_unsigned(type_definitions_.size() + all_functions.size()));
  for (const auto& f : type_definitions_) {
    type_section_bytes.emplace_back(0x60);
    concat(type_section_bytes, encode_unsigned(f[0].size()));
    for (const auto& t : f[0]) {
      type_section_bytes.emplace_back(t);
    }
    concat(type_section_bytes, encode_unsigned(f[1].size()));
    for (const auto& t : f[1]) {
      type_section_bytes.emplace_back(t);
    }
  }
  for (const auto& f : all_functions) {
    type_section_bytes.emplace_back(0x60);
    concat(type_section_bytes, encode_unsigned(f.input_types.size()));
    for (const auto& t : f.input_types) {
      type_section_bytes.emplace_back(t);
    }
    concat(type_section_bytes, encode_unsigned(f.output_types.size()));
    for (const auto& t : f.output_types) {
      type_section_bytes.emplace_back(t);
    }
  }

  emitted_bytes.emplace_back(0x1);
  concat(emitted_bytes, encode_unsigned(type_section_bytes.size()));
  concat(emitted_bytes, type_section_bytes);

  std::vector<uint8_t> import_section_bytes;
  int numImports = 0;
  numImports += memory.is_import() ? 1 : 0;
  numImports += table.is_import() ? 1 : 0;
  numImports += imported_functions_.size();
  if (numImports != 0) {
    concat(import_section_bytes, encode_unsigned(numImports));

    if (memory.is_import()) {
      concat(import_section_bytes, encode_string(memory.a_string));
      concat(import_section_bytes, encode_string(memory.b_string));
      import_section_bytes.emplace_back(0x2);  // memory flag
      if (memory.min && memory.max) {
        if (memory.is_shared) {
          import_section_bytes.emplace_back(0x3);
        } else {
          import_section_bytes.emplace_back(0x01);
        }
        concat(import_section_bytes, encode_unsigned(memory.min));
        concat(import_section_bytes, encode_unsigned(memory.max));
      } else {
        assert(!memory.is_shared && "shared memory must have a max size");
        import_section_bytes.emplace_back(0x0);  // Default flag
        concat(import_section_bytes, encode_unsigned(memory.min));
      }
    }

    if (imported_functions_.size() != 0) {
      for (auto i = 0; i < imported_functions_.size(); ++i) {
        auto imported_function = imported_functions_[i];
        concat(import_section_bytes, encode_string(imported_function.a_string));
        concat(import_section_bytes, encode_string(imported_function.b_string));
        import_section_bytes.emplace_back(0x0);  // function flag

        concat(import_section_bytes,
               encode_unsigned(type_definitions_.size() + i));  // type index
      }
    }

    if (table.is_import()) {
      concat(import_section_bytes, encode_string(table.a_string));
      concat(import_section_bytes, encode_string(table.b_string));
      import_section_bytes.emplace_back(0x01);  // table flag
      import_section_bytes.emplace_back(table.type);
      import_section_bytes.emplace_back(0x0);  // Default flag
      concat(import_section_bytes, encode_unsigned(table.min));
    }

    emitted_bytes.emplace_back(0x2);  // Import section id
    concat(emitted_bytes, encode_unsigned(import_section_bytes.size()));
    concat(emitted_bytes, import_section_bytes);
  }

  std::vector<uint8_t> function_section_bytes;
  concat(function_section_bytes, encode_unsigned(functions_.size()));
  for (auto i = 0; i < functions_.size(); ++i) {
    concat(function_section_bytes,
           encode_unsigned(type_definitions_.size() +
                           imported_functions_.size() + i));
  }
  emitted_bytes.emplace_back(0x3);
  concat(emitted_bytes, encode_unsigned(function_section_bytes.size()));
  concat(emitted_bytes, function_section_bytes);

  std::vector<uint8_t> memory_section_bytes;
  if (!memory.is_import() && (memory.min || memory.max)) {
    memory_section_bytes.emplace_back(0x01);  // always 1 memory
    if (memory.min && memory.max) {
      if (memory.is_shared) {
        memory_section_bytes.emplace_back(0x03);
      } else {
        memory_section_bytes.emplace_back(0x01);
      }
      concat(memory_section_bytes, encode_unsigned(memory.min));
      concat(memory_section_bytes, encode_unsigned(memory.max));
    } else {
      assert(!memory.is_shared && "shared memory must have a max size");
      memory_section_bytes.emplace_back(0x00);
      concat(memory_section_bytes, encode_unsigned(memory.min));
    }
    emitted_bytes.emplace_back(0x05);
    concat(emitted_bytes, encode_unsigned(memory_section_bytes.size()));
    concat(emitted_bytes, memory_section_bytes);
  }

  std::vector<uint8_t> export_section_bytes;
  auto num_exports = exported_functions_.size() + memory.is_export();
  concat(export_section_bytes, encode_unsigned(num_exports));
  if (memory.is_export()) {
    concat(export_section_bytes, encode_string(memory.a_string));
    export_section_bytes.emplace_back(0x02);
    export_section_bytes.emplace_back(0x00);  // always 1 memory at index 0
  }
  for (const auto& p : exported_functions_) {
    concat(export_section_bytes, encode_string(p.second));
    export_section_bytes.emplace_back(0x00);
    concat(export_section_bytes, encode_unsigned(p.first));
  }
  emitted_bytes.emplace_back(0x7);
  concat(emitted_bytes, encode_unsigned(export_section_bytes.size()));
  concat(emitted_bytes, export_section_bytes);

  std::vector<uint8_t> start_section_bytes;
  if (start_function_ != -1) {
    concat(start_section_bytes, encode_unsigned(start_function_));
    emitted_bytes.emplace_back(0x8);
    concat(emitted_bytes, encode_unsigned(start_section_bytes.size()));
    concat(emitted_bytes, start_section_bytes);
  }

  std::vector<uint8_t> elems_section_bytes;
  if (!elem_functions_.empty()) {
    concat(elems_section_bytes, encode_unsigned(1));
    elems_section_bytes.emplace_back(1U);    // Passive
    elems_section_bytes.emplace_back(0x00);  // elemkind: funcref
    concat(elems_section_bytes, encode_unsigned(elem_functions_.size()));
    for (auto idx : elem_functions_)
      concat(elems_section_bytes, encode_unsigned(idx));

    emitted_bytes.emplace_back(0x09);  // Element section
    concat(emitted_bytes, encode_unsigned(elems_section_bytes.size()));
    concat(emitted_bytes, elems_section_bytes);
  }

  emitted_bytes.emplace_back(0xa);
  concat(emitted_bytes, encode_unsigned(code_section_bytes.size()));
  concat(emitted_bytes, code_section_bytes);

  return emitted_bytes;
}

}  // namespace wasmblr