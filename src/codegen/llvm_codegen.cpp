#include "codegen/llvm_codegen.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>

LLVMCodegen::LLVMCodegen(std::ostream &out)
    : out_(out),
      module_(std::make_unique<llvm::Module>("salmon", ctx_)),
      builder_(ctx_) {
    module_->setTargetTriple(llvm::Triple("x86_64-pc-linux-gnu"));
    push_scope();

    llvm::Type *ptr_ty = llvm::PointerType::getUnqual(ctx_);
    llvm::Type *i64_ty = llvm::Type::getInt64Ty(ctx_);

    // %struct.salmon_slice = type { ptr, i64 }
    llvm::StructType::create(ctx_, {ptr_ty, i64_ty}, "struct.salmon_slice");

    // %struct.salmon_list = type { ptr, i64, i64 }
    llvm::StructType::create(ctx_, {ptr_ty, i64_ty, i64_ty}, "struct.salmon_list");
}

void LLVMCodegen::generate(const Program &program) {
    program.accept(*this);
    emit_runtime_decls();

    std::string ir_str;
    llvm::raw_string_ostream rso(ir_str);
    module_->print(rso, nullptr);
    out_ << ir_str;
}

void LLVMCodegen::push_scope() {
    scopes_.emplace_back();
}

void LLVMCodegen::pop_scope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

bool LLVMCodegen::declare_symbol(const std::string &name, llvm::AllocaInst *inst, llvm::Type *type) {
    if (scopes_.empty()) {
        return false;
    }
    scopes_.back()[name] = LLVMSymbol{name, inst, type};
    return true;
}

const LLVMSymbol *LLVMCodegen::lookup_symbol(const std::string &name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

llvm::Type *LLVMCodegen::to_llvm_type(PrimitiveKind kind) {
    switch (kind) {
    case PrimitiveKind::Int:
    case PrimitiveKind::I32:
    case PrimitiveKind::UInt:
    case PrimitiveKind::U32:
        return llvm::Type::getInt32Ty(ctx_);
    case PrimitiveKind::I8:
    case PrimitiveKind::U8:
    case PrimitiveKind::Char:
        return llvm::Type::getInt8Ty(ctx_);
    case PrimitiveKind::I16:
    case PrimitiveKind::U16:
        return llvm::Type::getInt16Ty(ctx_);
    case PrimitiveKind::I64:
    case PrimitiveKind::U64:
        return llvm::Type::getInt64Ty(ctx_);
    case PrimitiveKind::Float:
    case PrimitiveKind::F32:
        return llvm::Type::getFloatTy(ctx_);
    case PrimitiveKind::F64:
        return llvm::Type::getDoubleTy(ctx_);
    case PrimitiveKind::Bool:
        return llvm::Type::getInt1Ty(ctx_);
    case PrimitiveKind::Void:
        return llvm::Type::getVoidTy(ctx_);
    }
    return llvm::Type::getVoidTy(ctx_);
}

llvm::Type *LLVMCodegen::to_llvm_type(const Type &type) {
    if (const auto *prim = dynamic_cast<const PrimitiveType *>(&type)) {
        return to_llvm_type(prim->kind());
    }
    if (dynamic_cast<const PointerType *>(&type)) {
        return llvm::PointerType::getUnqual(ctx_);
    }
    if (const auto *named = dynamic_cast<const NamedType *>(&type)) {
        std::string sname = "struct." + named->name();
        llvm::StructType *st = llvm::StructType::getTypeByName(ctx_, sname);
        if (st) {
            return st;
        }
        return llvm::StructType::create(ctx_, sname);
    }
    if (dynamic_cast<const ListType *>(&type)) {
        return llvm::StructType::getTypeByName(ctx_, "struct.salmon_list");
    }
    if (dynamic_cast<const SliceType *>(&type)) {
        return llvm::StructType::getTypeByName(ctx_, "struct.salmon_slice");
    }
    if (const auto *arr = dynamic_cast<const ArrayType *>(&type)) {
        return llvm::ArrayType::get(to_llvm_type(arr->elem_type()), arr->size());
    }
    return llvm::Type::getVoidTy(ctx_);
}

void LLVMCodegen::emit_runtime_decls() {
    llvm::Type *i32_ty = llvm::Type::getInt32Ty(ctx_);
    llvm::Type *i64_ty = llvm::Type::getInt64Ty(ctx_);
    llvm::Type *ptr_ty = llvm::PointerType::getUnqual(ctx_);
    llvm::Type *void_ty = llvm::Type::getVoidTy(ctx_);

    // declare i32 @printf(ptr, ...)
    if (!module_->getFunction("printf")) {
        llvm::FunctionType *printf_ty = llvm::FunctionType::get(i32_ty, {ptr_ty}, true);
        llvm::Function::Create(printf_ty, llvm::Function::ExternalLinkage, "printf", *module_);
    }

    // declare ptr @malloc(i64)
    if (!module_->getFunction("malloc")) {
        llvm::FunctionType *malloc_ty = llvm::FunctionType::get(ptr_ty, {i64_ty}, false);
        llvm::Function::Create(malloc_ty, llvm::Function::ExternalLinkage, "malloc", *module_);
    }

    // declare void @free(ptr)
    if (!module_->getFunction("free")) {
        llvm::FunctionType *free_ty = llvm::FunctionType::get(void_ty, {ptr_ty}, false);
        llvm::Function::Create(free_ty, llvm::Function::ExternalLinkage, "free", *module_);
    }
}

// Declarations
void LLVMCodegen::visit(const Program &node) {
    for (const auto &decl : node.decls()) {
        decl->accept(*this);
    }
}

void LLVMCodegen::visit(const IncludeDirective &node) {
    (void)node;
}

void LLVMCodegen::visit(const StructField &node) {
    (void)node;
}

void LLVMCodegen::visit(const StructDecl &node) {
    std::string sname = "struct." + node.name();
    llvm::StructType *st = llvm::StructType::getTypeByName(ctx_, sname);
    if (!st) {
        st = llvm::StructType::create(ctx_, sname);
    }
    std::vector<llvm::Type *> field_types;
    for (const auto &f : node.fields()) {
        field_types.push_back(to_llvm_type(f->type()));
    }
    if (st->isOpaque()) {
        st->setBody(field_types);
    }
}

void LLVMCodegen::visit(const Param &node) {
    (void)node;
}

void LLVMCodegen::visit(const FunctionDecl &node) {
    std::vector<llvm::Type *> param_types;
    for (const auto &p : node.params()) {
        param_types.push_back(to_llvm_type(p->type()));
    }
    llvm::Type *ret_t = node.ret_type() ? to_llvm_type(*node.ret_type()) : llvm::Type::getVoidTy(ctx_);
    llvm::FunctionType *func_t = llvm::FunctionType::get(ret_t, param_types, false);
    llvm::Function *func = llvm::Function::Create(func_t, llvm::Function::ExternalLinkage, node.name(), *module_);

    size_t idx = 0;
    for (auto &arg : func->args()) {
        arg.setName(node.params()[idx++]->name());
    }

    current_func_ = func;
    push_scope();

    llvm::BasicBlock *entry_bb = llvm::BasicBlock::Create(ctx_, "entry", func);
    builder_.SetInsertPoint(entry_bb);

    for (auto &arg : func->args()) {
        llvm::AllocaInst *alloca = builder_.CreateAlloca(arg.getType(), nullptr, arg.getName() + ".addr");
        builder_.CreateStore(&arg, alloca);
        declare_symbol(std::string(arg.getName()), alloca, arg.getType());
    }

    // Function body
    node.body().accept(*this);

    // If block doesn't have a terminator yet
    if (!builder_.GetInsertBlock()->getTerminator()) {
        if (ret_t->isVoidTy()) {
            builder_.CreateRetVoid();
        } else {
            builder_.CreateUnreachable();
        }
    }

    pop_scope();
    current_func_ = nullptr;
}

// Statements
void LLVMCodegen::visit(const BlockStmt &node) {
    for (const auto &stmt : node.stmts()) {
        stmt->accept(*this);
    }
}

void LLVMCodegen::visit(const VarDeclStmt &node) {
    (void)node;
}

void LLVMCodegen::visit(const AssignStmt &node) {
    (void)node;
}

void LLVMCodegen::visit(const ExprStmt &node) {
    (void)node;
}

void LLVMCodegen::visit(const IfStmt &node) {
    (void)node;
}

void LLVMCodegen::visit(const WhileStmt &node) {
    (void)node;
}

void LLVMCodegen::visit(const ForStmt &node) {
    (void)node;
}

void LLVMCodegen::visit(const ReturnStmt &node) {
    (void)node;
}

void LLVMCodegen::visit(const DeferStmt &node) {
    (void)node;
}

// Expressions
void LLVMCodegen::visit(const BinaryExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const UnaryExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const SizeofExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const PostfixUpdateExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const CallExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const IndexExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const MemberAccessExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const IntLiteralExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const FloatLiteralExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const StringLiteralExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const CharLiteralExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const BoolLiteralExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const NullLiteralExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const ArrayLiteralExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const ListLiteralExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const AllocExpr &node) {
    (void)node;
}

void LLVMCodegen::visit(const IdentifierExpr &node) {
    (void)node;
}

// Types
void LLVMCodegen::visit(const PrimitiveType &node) {
    (void)node;
}

void LLVMCodegen::visit(const NamedType &node) {
    (void)node;
}

void LLVMCodegen::visit(const ListType &node) {
    (void)node;
}

void LLVMCodegen::visit(const PointerType &node) {
    (void)node;
}

void LLVMCodegen::visit(const SliceType &node) {
    (void)node;
}

void LLVMCodegen::visit(const ArrayType &node) {
    (void)node;
}
