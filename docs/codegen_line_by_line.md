# LLVM Codegen Line-by-Line Breakdown

This document provides a comprehensive line-by-line explanation of every construct, class, method, and LLVM API call written in [include/codegen/llvm_codegen.h](file:///home/ewu/Code/salmon/include/codegen/llvm_codegen.h) and [src/codegen/llvm_codegen.cpp](file:///home/ewu/Code/salmon/src/codegen/llvm_codegen.cpp).

---

## Part 1: [include/codegen/llvm_codegen.h](file:///home/ewu/Code/salmon/include/codegen/llvm_codegen.h)

### Header Guards & Includes (Lines 1–17)

```cpp
1: #pragma once
```
* **Line 1**: Standard preprocessor directive ensuring this header file is included only once per compilation unit, preventing duplicate symbol and type errors.

```cpp
3: #include "ast.h"
4: #include "ast_visitor.h"
```
* **Line 3**: Brings in the definitions of all Salmon AST nodes (`Program`, `FunctionDecl`, `BlockStmt`, `Type`, etc.).
* **Line 4**: Brings in the abstract `ASTVisitor` interface that `LLVMCodegen` implements.

```cpp
5: #include <llvm/IR/BasicBlock.h>
6: #include <llvm/IR/Function.h>
7: #include <llvm/IR/IRBuilder.h>
8: #include <llvm/IR/Instructions.h>
9: #include <llvm/IR/LLVMContext.h>
10: #include <llvm/IR/Module.h>
11: #include <llvm/IR/Type.h>
12: #include <llvm/IR/Value.h>
```
* **Line 5 (`llvm/IR/BasicBlock.h`)**: Defines `llvm::BasicBlock`, representing a linear sequence of LLVM instructions ending in a single terminator (`ret`, `br`).
* **Line 6 (`llvm/IR/Function.h`)**: Defines `llvm::Function`, which represents a callable function unit in LLVM IR containing basic blocks and argument lists.
* **Line 7 (`llvm/IR/IRBuilder.h`)**: The core LLVM builder utility (`llvm::IRBuilder<>`) providing helper methods like `CreateAlloca`, `CreateStore`, `CreateLoad`, `CreateRet`, etc., that automatically insert instructions at the current cursor position.
* **Line 8 (`llvm/IR/Instructions.h`)**: Declares specific instruction classes such as `llvm::AllocaInst`, `llvm::ReturnInst`, `llvm::BranchInst`.
* **Line 9 (`llvm/IR/LLVMContext.h`)**: Core context manager (`llvm::LLVMContext`) that owns global LLVM data structures, type uniqueing tables, and constant pools across a compilation thread.
* **Line 10 (`llvm/IR/Module.h`)**: Top-level container (`llvm::Module`) holding all functions, global variables, data layout strings, target triples, and symbol tables for a single translation unit.
* **Line 11 (`llvm/IR/Type.h`)**: Base class for all LLVM types (`i32`, `i64`, `ptr`, `void`, `float`, `StructType`, `ArrayType`).
* **Line 12 (`llvm/IR/Value.h`)**: Fundamental base class in LLVM representing any computed value or operand that can be used in instructions.

```cpp
13: #include <memory>
14: #include <ostream>
15: #include <string>
16: #include <unordered_map>
17: #include <vector>
```
* **Lines 13–17**: Standard C++ library headers for smart pointers (`std::unique_ptr`), output streams (`std::ostream`), dynamic strings (`std::string`), hash maps (`std::unordered_map`), and dynamic arrays (`std::vector`).

---

### Symbol Representation (Lines 19–23)

```cpp
19: struct LLVMSymbol {
20:     std::string name;
21:     llvm::AllocaInst *alloca_inst{nullptr};
22:     llvm::Type *type{nullptr};
23: };
```
* **Line 19**: Defines `LLVMSymbol`, which represents an entry in our compiler's symbol table.
* **Line 20 (`name`)**: The source-level identifier name (e.g. `"x"`, `"a"`, `"head"`).
* **Line 21 (`alloca_inst`)**: Pointer to the `alloca` stack memory address instruction allocated in the function entry block. When writing or reading this variable, instructions store into or load from this address.
* **Line 22 (`type`)**: Cached LLVM type of the variable (e.g. `i32`, `ptr`, `%struct.Node`) so we can create loads and stores with correct types.

---

### Class Declaration & Lifecycle (Lines 25–30)

```cpp
25: class LLVMCodegen final : public ASTVisitor {
26: public:
27:     explicit LLVMCodegen(std::ostream &out);
28:     ~LLVMCodegen() override = default;
29: 
30:     void generate(const Program &program);
```
* **Line 25**: Declares `LLVMCodegen` marked `final` (cannot be inherited further), deriving from the `ASTVisitor` base class.
* **Line 27**: Explicit constructor accepting an output stream `out` where textual LLVM IR will be written.
* **Line 28**: Virtual default destructor ensuring safe polymorphic destruction.
* **Line 30**: Public entry point method `generate()` that initiates code generation on the root `Program` node.

---

### AST Visitor Overrides (Lines 32–76)

```cpp
32:     // Declarations
33:     void visit(const Program &node) override;
34:     void visit(const IncludeDirective &node) override;
35:     void visit(const StructField &node) override;
36:     void visit(const StructDecl &node) override;
37:     void visit(const Param &node) override;
38:     void visit(const FunctionDecl &node) override;
```
* **Lines 33–38**: Visitor overrides for program-level declarations:
  - `Program`: iterates over all top-level statements/declarations.
  - `IncludeDirective`: tracks imported modules.
  - `StructField`: represents fields inside structs.
  - `StructDecl`: registers user-defined struct types (`%struct.Point = type { ... }`).
  - `Param`: represents function parameters.
  - `FunctionDecl`: emits `define @name(...)` blocks and stack setups.

```cpp
40:     // Statements
41:     void visit(const BlockStmt &node) override;
42:     void visit(const VarDeclStmt &node) override;
43:     void visit(const AssignStmt &node) override;
44:     void visit(const ExprStmt &node) override;
45:     void visit(const IfStmt &node) override;
46:     void visit(const WhileStmt &node) override;
47:     void visit(const ForStmt &node) override;
48:     void visit(const ReturnStmt &node) override;
49:     void visit(const DeferStmt &node) override;
```
* **Lines 41–49**: Visitor hooks for control flow, assignments, local variable allocations, and cleanup statements.

```cpp
51:     // Expressions
52:     void visit(const BinaryExpr &node) override;
...
68:     void visit(const IdentifierExpr &node) override;
```
* **Lines 52–68**: Visitor hooks for evaluating literals, function calls, arithmetic, member accesses, allocations, and identifiers.

```cpp
70:     // Types
71:     void visit(const PrimitiveType &node) override;
...
76:     void visit(const ArrayType &node) override;
```
* **Lines 71–76**: Visitor hooks for lowering Salmon's AST type nodes to LLVM types.

---

### LLVM Accessors (Lines 78–80)

```cpp
78:     llvm::Module &module() { return *module_; }
79:     llvm::IRBuilder<> &builder() { return builder_; }
80:     llvm::LLVMContext &context() { return ctx_; }
```
* **Lines 78–80**: Direct accessors for compiler extensions or unit tests that want to query the underlying LLVM Module, Builder, or Context.

---

### Private Helpers & State (Lines 82–100)

```cpp
83:     llvm::Type *to_llvm_type(const Type &type);
84:     llvm::Type *to_llvm_type(PrimitiveKind kind);
```
* **Lines 83–84**: Helper methods that convert Salmon AST types and primitive kinds into raw `llvm::Type*` instances.

```cpp
86:     void push_scope();
87:     void pop_scope();
88:     bool declare_symbol(const std::string &name, llvm::AllocaInst *inst, llvm::Type *type);
89:     const LLVMSymbol *lookup_symbol(const std::string &name) const;
```
* **Lines 86–89**: Lexical scope management:
  - `push_scope`: enters a new block `{ ... }`.
  - `pop_scope`: exits a block, destroying shadowed variable mappings.
  - `declare_symbol`: registers a variable in the innermost scope.
  - `lookup_symbol`: searches from the innermost to the outermost scope to resolve a variable name.

```cpp
91:     void emit_runtime_decls();
```
* **Line 91**: Declares external C runtime library functions (`printf`, `malloc`, `free`) in the LLVM module.

```cpp
93:     std::ostream &out_;
94:     llvm::LLVMContext ctx_;
95:     std::unique_ptr<llvm::Module> module_;
96:     llvm::IRBuilder<> builder_;
97:     llvm::Function *current_func_{nullptr};
98:     llvm::Value *last_val_{nullptr};
99:     std::vector<std::unordered_map<std::string, LLVMSymbol>> scopes_;
100: };
```
* **Line 93 (`out_`)**: Target stream for writing LLVM IR text.
* **Line 94 (`ctx_`)**: The thread-local LLVM context owning all types.
* **Line 95 (`module_`)**: Unique pointer owning the active LLVM Module.
* **Line 96 (`builder_`)**: The `llvm::IRBuilder<>` instance used to emit instructions.
* **Line 97 (`current_func_`)**: Pointer to the function currently being compiled.
* **Line 98 (`last_val_`)**: Stores the `llvm::Value*` result computed by the most recent expression visitor.
* **Line 99 (`scopes_`)**: Stack of symbol tables for lexical scoping.

---

## Part 2: [src/codegen/llvm_codegen.cpp](file:///home/ewu/Code/salmon/src/codegen/llvm_codegen.cpp)

### Includes (Lines 1–4)

```cpp
1: #include "codegen/llvm_codegen.h"
2: #include <llvm/IR/DerivedTypes.h>
3: #include <llvm/Support/raw_ostream.h>
4: #include <llvm/TargetParser/Triple.h>
```
* **Line 1**: Codegen class declaration.
* **Line 2**: Provides `llvm::StructType::getTypeByName`.
* **Line 3**: Provides `llvm::raw_string_ostream` to stream LLVM IR into strings.
* **Line 4**: Provides `llvm::Triple` to configure target architecture (`x86_64-pc-linux-gnu`).

---

### Constructor & Built-in Aggregates (Lines 6–22)

```cpp
6: LLVMCodegen::LLVMCodegen(std::ostream &out)
7:     : out_(out),
8:       module_(std::make_unique<llvm::Module>("salmon", ctx_)),
9:       builder_(ctx_) {
```
* **Lines 6–9**: Initializes:
  - `out_`: bound to caller stream (e.g. `std::cout`).
  - `module_`: creates a new LLVM module named `"salmon"` bound to `ctx_`.
  - `builder_`: creates the instruction builder bound to `ctx_`.

```cpp
10:     module_->setTargetTriple(llvm::Triple("x86_64-pc-linux-gnu"));
11:     push_scope();
```
* **Line 10**: Sets the target architecture triple to 64-bit Linux (`x86_64-pc-linux-gnu`).
* **Line 11**: Pushes the global/file-level symbol table scope.

```cpp
13:     // Built-in Salmon aggregate types
14:     llvm::Type *ptr_ty = llvm::PointerType::getUnqual(ctx_);
15:     llvm::Type *i64_ty = llvm::Type::getInt64Ty(ctx_);
```
* **Line 14**: Creates an unqualified opaque pointer type (`ptr` in modern LLVM 15+).
* **Line 15**: Fetches 64-bit integer type (`i64`).

```cpp
17:     // %struct.salmon_slice = type { ptr, i64 }
18:     llvm::StructType::create(ctx_, {ptr_ty, i64_ty}, "struct.salmon_slice");
```
* **Line 18**: Defines Salmon's dynamic slice layout in LLVM: `{ ptr data, i64 len }`.

```cpp
20:     // %struct.salmon_list = type { ptr, i64, i64 }
21:     llvm::StructType::create(ctx_, {ptr_ty, i64_ty, i64_ty}, "struct.salmon_list");
22: }
```
* **Line 21**: Defines Salmon's growable `list<T>` layout in LLVM: `{ ptr data, i64 len, i64 capacity }`.

---

### Top-level Pipeline: `generate()` (Lines 24–32)

```cpp
24: void LLVMCodegen::generate(const Program &program) {
25:     program.accept(*this);
26:     emit_runtime_decls();
```
* **Line 25**: Visits all declarations in the AST program, generating code for each struct and function.
* **Line 26**: Injects external runtime declarations (`printf`, `malloc`, `free`).

```cpp
28:     std::string ir_str;
29:     llvm::raw_string_ostream rso(ir_str);
30:     module_->print(rso, nullptr);
31:     out_ << ir_str;
32: }
```
* **Line 28**: Allocates a string buffer.
* **Line 29**: Wraps the string in LLVM's `raw_string_ostream`.
* **Line 30**: Dumps the complete, formatted, type-checked textual LLVM IR into the stream.
* **Line 31**: Flushes the resulting text to `out_`.

---

### Scoping and Symbol Resolution (Lines 34–60)

```cpp
34: void LLVMCodegen::push_scope() {
35:     scopes_.emplace_back();
36: }
```
* **Lines 34–36**: Adds a new map at the top of the scope stack for a new block.

```cpp
38: void LLVMCodegen::pop_scope() {
39:     if (!scopes_.empty()) {
40:         scopes_.pop_back();
41:     }
42: }
```
* **Lines 38–42**: Pops the current scope when exiting a block, safely destroying local variable bindings.

```cpp
44: bool LLVMCodegen::declare_symbol(const std::string &name, llvm::AllocaInst *inst, llvm::Type *type) {
45:     if (scopes_.empty()) {
46:         return false;
47:     }
48:     scopes_.back()[name] = LLVMSymbol{name, inst, type};
49:     return true;
50: }
```
* **Lines 44–50**: Inserts a new variable mapping into `scopes_.back()` (innermost scope).

```cpp
52: const LLVMSymbol *LLVMCodegen::lookup_symbol(const std::string &name) const {
53:     for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
54:         auto found = it->find(name);
55:         if (found != it->end()) {
56:             return &found->second;
57:         }
58:     }
59:     return nullptr;
60: }
```
* **Lines 52–60**: Traverses from innermost scope backwards (`rbegin()` to `rend()`), resolving variables with proper lexical shadowing.

---

### Type Lowering (Lines 62–117)

```cpp
62: llvm::Type *LLVMCodegen::to_llvm_type(PrimitiveKind kind) {
63:     switch (kind) {
64:     case PrimitiveKind::Int:
65:     case PrimitiveKind::I32:
66:     case PrimitiveKind::UInt:
67:     case PrimitiveKind::U32:
68:         return llvm::Type::getInt32Ty(ctx_);
```
* **Lines 64–68**: Maps 32-bit signed and unsigned integers to LLVM's `i32`.

```cpp
69:     case PrimitiveKind::I8:
70:     case PrimitiveKind::U8:
71:     case PrimitiveKind::Char:
72:         return llvm::Type::getInt8Ty(ctx_);
```
* **Lines 69–72**: Maps 8-bit integers and characters to `i8`.

```cpp
73:     case PrimitiveKind::I16:
74:     case PrimitiveKind::U16:
75:         return llvm::Type::getInt16Ty(ctx_);
```
* **Lines 73–75**: Maps 16-bit integers to `i16`.

```cpp
76:     case PrimitiveKind::I64:
77:     case PrimitiveKind::U64:
78:         return llvm::Type::getInt64Ty(ctx_);
```
* **Lines 76–78**: Maps 64-bit integers to `i64`.

```cpp
79:     case PrimitiveKind::Float:
80:     case PrimitiveKind::F32:
81:         return llvm::Type::getFloatTy(ctx_);
82:     case PrimitiveKind::F64:
83:         return llvm::Type::getDoubleTy(ctx_);
84:     case PrimitiveKind::Bool:
85:         return llvm::Type::getInt1Ty(ctx_);
86:     case PrimitiveKind::Void:
87:         return llvm::Type::getVoidTy(ctx_);
```
* **Lines 79–87**: Maps floating-point (`float`, `double`), boolean (`i1`), and `void`.

```cpp
92: llvm::Type *LLVMCodegen::to_llvm_type(const Type &type) {
93:     if (const auto *prim = dynamic_cast<const PrimitiveType *>(&type)) {
94:         return to_llvm_type(prim->kind());
95:     }
```
* **Lines 93–95**: If the node is a `PrimitiveType`, delegates to `to_llvm_type(PrimitiveKind)`.

```cpp
96:     if (dynamic_cast<const PointerType *>(&type)) {
97:         return llvm::PointerType::getUnqual(ctx_);
98:     }
```
* **Lines 96–98**: Maps any pointer (`Node*`, `int*`, etc.) to LLVM's opaque pointer `ptr`.

```cpp
99:     if (const auto *named = dynamic_cast<const NamedType *>(&type)) {
100:         std::string sname = "struct." + named->name();
101:         llvm::StructType *st = llvm::StructType::getTypeByName(ctx_, sname);
102:         if (st) {
103:             return st;
104:         }
105:         return llvm::StructType::create(ctx_, sname);
106:     }
```
* **Lines 99–106**: Looks up or forward-declares a named struct type (e.g. `%struct.Node`).

```cpp
107:     if (dynamic_cast<const ListType *>(&type)) {
108:         return llvm::StructType::getTypeByName(ctx_, "struct.salmon_list");
109:     }
110:     if (dynamic_cast<const SliceType *>(&type)) {
111:         return llvm::StructType::getTypeByName(ctx_, "struct.salmon_slice");
112:     }
```
* **Lines 107–112**: Resolves built-in slice and list aggregates.

```cpp
113:     if (const auto *arr = dynamic_cast<const ArrayType *>(&type)) {
114:         return llvm::ArrayType::get(to_llvm_type(arr->elem_type()), arr->size());
115:     }
116:     return llvm::Type::getVoidTy(ctx_);
117: }
```
* **Lines 113–115**: Constructs an `llvm::ArrayType` of fixed length (e.g. `[4 x i32]`).

---

### External Runtime Declarations (Lines 119–142)

```cpp
119: void LLVMCodegen::emit_runtime_decls() {
120:     llvm::Type *i32_ty = llvm::Type::getInt32Ty(ctx_);
121:     llvm::Type *i64_ty = llvm::Type::getInt64Ty(ctx_);
122:     llvm::Type *ptr_ty = llvm::PointerType::getUnqual(ctx_);
123:     llvm::Type *void_ty = llvm::Type::getVoidTy(ctx_);
```
* **Lines 120–123**: Caches primitive types needed for C standard library function signatures.

```cpp
125:     // declare i32 @printf(ptr, ...)
126:     if (!module_->getFunction("printf")) {
127:         llvm::FunctionType *printf_ty = llvm::FunctionType::get(i32_ty, {ptr_ty}, true);
128:         llvm::Function::Create(printf_ty, llvm::Function::ExternalLinkage, "printf", *module_);
129:     }
```
* **Lines 125–129**: Declares `declare i32 @printf(ptr, ...)` as a variadic external function if not already declared.

```cpp
131:     // declare ptr @malloc(i64)
132:     if (!module_->getFunction("malloc")) {
133:         llvm::FunctionType *malloc_ty = llvm::FunctionType::get(ptr_ty, {i64_ty}, false);
134:         llvm::Function::Create(malloc_ty, llvm::Function::ExternalLinkage, "malloc", *module_);
135:     }
```
* **Lines 131–135**: Declares `declare ptr @malloc(i64)` for heap allocations (`alloc<T>()`).

```cpp
137:     // declare void @free(ptr)
138:     if (!module_->getFunction("free")) {
139:         llvm::FunctionType *free_ty = llvm::FunctionType::get(void_ty, {ptr_ty}, false);
140:         llvm::Function::Create(free_ty, llvm::Function::ExternalLinkage, "free", *module_);
141:     }
142: }
```
* **Lines 137–142**: Declares `declare void @free(ptr)` for deallocations (`free(ptr)`).

---

### Struct & Function Declarations (Lines 144–218)

```cpp
145: void LLVMCodegen::visit(const Program &node) {
146:     for (const auto &decl : node.decls()) {
147:         decl->accept(*this);
148:     }
149: }
```
* **Lines 145–149**: Visits every top-level struct, include, and function in the source file in order.

```cpp
159: void LLVMCodegen::visit(const StructDecl &node) {
160:     std::string sname = "struct." + node.name();
161:     llvm::StructType *st = llvm::StructType::getTypeByName(ctx_, sname);
162:     if (!st) {
163:         st = llvm::StructType::create(ctx_, sname);
164:     }
```
* **Lines 160–164**: Looks up or creates the named `llvm::StructType`.

```cpp
165:     std::vector<llvm::Type *> field_types;
166:     for (const auto &f : node.fields()) {
167:         field_types.push_back(to_llvm_type(f->type()));
168:     }
169:     if (st->isOpaque()) {
170:         st->setBody(field_types);
171:     }
172: }
```
* **Lines 165–172**: Collects all field types and defines the struct body layout (e.g. `%struct.Point = type { double, double }`).

```cpp
178: void LLVMCodegen::visit(const FunctionDecl &node) {
179:     std::vector<llvm::Type *> param_types;
180:     for (const auto &p : node.params()) {
181:         param_types.push_back(to_llvm_type(p->type()));
182:     }
```
* **Lines 179–182**: Lowers all parameter AST types into LLVM types.

```cpp
183:     llvm::Type *ret_t = node.ret_type() ? to_llvm_type(*node.ret_type()) : llvm::Type::getVoidTy(ctx_);
184:     llvm::FunctionType *func_t = llvm::FunctionType::get(ret_t, param_types, false);
185:     llvm::Function *func = llvm::Function::Create(func_t, llvm::Function::ExternalLinkage, node.name(), *module_);
```
* **Lines 183–185**: Creates the `llvm::Function` entity with external linkage in `module_`.

```cpp
187:     size_t idx = 0;
188:     for (auto &arg : func->args()) {
189:         arg.setName(node.params()[idx++]->name());
190:     }
```
* **Lines 187–190**: Names function arguments to match source code parameters (e.g. `%a`, `%flag`).

```cpp
192:     current_func_ = func;
193:     push_scope();
```
* **Lines 192–193**: Sets the current function context pointer and creates a new scope for the function body.

```cpp
195:     llvm::BasicBlock *entry_bb = llvm::BasicBlock::Create(ctx_, "entry", func);
196:     builder_.SetInsertPoint(entry_bb);
```
* **Lines 195–196**: Creates the `"entry"` basic block inside the function and positions `builder_` inside it.

```cpp
198:     for (auto &arg : func->args()) {
199:         llvm::AllocaInst *alloca = builder_.CreateAlloca(arg.getType(), nullptr, arg.getName() + ".addr");
200:         builder_.CreateStore(&arg, alloca);
201:         declare_symbol(std::string(arg.getName()), alloca, arg.getType());
202:     }
```
* **Lines 198–202**: **Standard LLVM `mem2reg` lowering pattern**:
  - `CreateAlloca`: Allocates local stack memory for the parameter (e.g. `%a.addr = alloca i32`).
  - `CreateStore`: Stores the incoming argument register value into the stack slot.
  - `declare_symbol`: Registers `%a.addr` in the symbol table so future reads and writes access this variable.

```cpp
204:     // Function body
205:     node.body().accept(*this);
```
* **Line 205**: Visits the statements inside the function body block.

```cpp
207:     // If block doesn't have a terminator yet
208:     if (!builder_.GetInsertBlock()->getTerminator()) {
209:         if (ret_t->isVoidTy()) {
210:             builder_.CreateRetVoid();
211:         } else {
212:             builder_.CreateUnreachable();
213:         }
214:     }
```
* **Lines 207–214**: Ensures LLVM IR validity: every basic block in LLVM must end with a terminator. If the user didn't write an explicit `return`:
  - Void functions emit `ret void`.
  - Non-void functions without return emit `unreachable`.

```cpp
216:     pop_scope();
217:     current_func_ = nullptr;
218: }
```
* **Lines 216–218**: Pops the function scope and resets `current_func_`.

---

### Statement & Expression Visitor Outlines (Lines 220–352)

```cpp
221: void LLVMCodegen::visit(const BlockStmt &node) {
222:     for (const auto &stmt : node.stmts()) {
223:         stmt->accept(*this);
224:     }
225: }
```
* **Lines 221–225**: Recursively visits each statement within a block `{ stmt1; stmt2; }`.

```cpp
227: void LLVMCodegen::visit(const VarDeclStmt &node) {
228:     (void)node;
229: }
...
351: void LLVMCodegen::visit(const ArrayType &node) {
352:     (void)node;
353: }
```
* **Lines 227–352**: Outlines for specific statement and expression visitors (`IfStmt`, `WhileStmt`, `ForStmt`, `BinaryExpr`, `CallExpr`, `IndexExpr`, etc.), kept as structured stubs ready for incremental feature implementation.
