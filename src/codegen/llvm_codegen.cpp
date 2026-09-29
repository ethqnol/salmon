#include "codegen/llvm_codegen.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>

LLVMCodegen::LLVMCodegen(std::ostream &out)
    : out_(out), module_(std::make_unique<llvm::Module>("salmon", ctx_)),
      builder_(ctx_) {
    module_->setTargetTriple(llvm::Triple("x86_64-pc-linux-gnu"));
    push_scope();

    llvm::Type *ptr_ty = llvm::PointerType::getUnqual(ctx_);
    llvm::Type *i64_ty = llvm::Type::getInt64Ty(ctx_);

    // %struct.salmon_slice = type { ptr, i64 }
    llvm::StructType::create(ctx_, {ptr_ty, i64_ty}, "struct.salmon_slice");

    // %struct.salmon_list = type { ptr, i64, i64 }
    llvm::StructType::create(ctx_, {ptr_ty, i64_ty, i64_ty},
                             "struct.salmon_list");
}

void LLVMCodegen::generate(const Program &program) {
    program.accept(*this);
    emit_runtime_decls();

    std::string ir_str;
    llvm::raw_string_ostream rso(ir_str);
    module_->print(rso, nullptr);
    out_ << ir_str;
}

void LLVMCodegen::push_scope() { scopes_.emplace_back(); }

void LLVMCodegen::pop_scope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

bool LLVMCodegen::declare_symbol(const std::string &name,
                                 llvm::AllocaInst *inst, llvm::Type *type) {
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
        llvm::FunctionType *printf_ty =
            llvm::FunctionType::get(i32_ty, {ptr_ty}, true);
        llvm::Function::Create(printf_ty, llvm::Function::ExternalLinkage, "printf",
                               *module_);
    }

    // declare ptr @malloc(i64)
    if (!module_->getFunction("malloc")) {
        llvm::FunctionType *malloc_ty =
            llvm::FunctionType::get(ptr_ty, {i64_ty}, false);
        llvm::Function::Create(malloc_ty, llvm::Function::ExternalLinkage, "malloc",
                               *module_);
    }

    // declare void @free(ptr)
    if (!module_->getFunction("free")) {
        llvm::FunctionType *free_ty =
            llvm::FunctionType::get(void_ty, {ptr_ty}, false);
        llvm::Function::Create(free_ty, llvm::Function::ExternalLinkage, "free",
                               *module_);
    }
}

// Declarations
void LLVMCodegen::visit(const Program &node) {
    for (const auto &decl : node.decls()) {
        decl->accept(*this);
    }
}

void LLVMCodegen::visit(const IncludeDirective &node) { (void)node; }

void LLVMCodegen::visit(const StructField &node) { (void)node; }

void LLVMCodegen::visit(const StructDecl &node) {
    std::string sname = "struct." + node.name();
    llvm::StructType *st = llvm::StructType::getTypeByName(ctx_, sname);
    if (!st) {
        st = llvm::StructType::create(ctx_, sname);
    } else if (!st->isOpaque()) {
        throw std::runtime_error("Redefinition of struct '" + node.name() + "'");
    }

    StructInfo info;
    info.llvm_type = st;

    std::vector<llvm::Type *> field_types;
    for (size_t i = 0; i < node.fields().size(); ++i) {
        const auto &f = node.fields()[i];
        field_types.push_back(to_llvm_type(f->type()));
        info.field_indices[f->name()] = static_cast<unsigned>(i);
        info.field_types[f->name()] = &f->type();
    }
    if (st->isOpaque()) {
        st->setBody(field_types);
    }
    struct_defs_[node.name()] = std::move(info);
}

void LLVMCodegen::visit(const Param &node) { (void)node; }

void LLVMCodegen::visit(const FunctionDecl &node) {
    std::vector<llvm::Type *> param_types;
    for (const auto &p : node.params()) {
        param_types.push_back(to_llvm_type(p->type()));
    }
    llvm::Type *ret_t = node.ret_type() ? to_llvm_type(*node.ret_type())
                                        : llvm::Type::getVoidTy(ctx_);
    llvm::FunctionType *func_t =
        llvm::FunctionType::get(ret_t, param_types, false);
    llvm::Function *func = llvm::Function::Create(
        func_t, llvm::Function::ExternalLinkage, node.name(), *module_);

    size_t idx = 0;
    for (auto &arg : func->args()) {
        arg.setName(node.params()[idx++]->name());
    }

    current_func_ = func;
    push_scope();

    llvm::BasicBlock *entry_bb = llvm::BasicBlock::Create(ctx_, "entry", func);
    builder_.SetInsertPoint(entry_bb);

    for (auto &arg : func->args()) {
        llvm::AllocaInst *alloca =
            builder_.CreateAlloca(arg.getType(), nullptr, arg.getName() + ".addr");
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
    llvm::AllocaInst *alloca =
        builder_.CreateAlloca(to_llvm_type(node.type()), nullptr, node.name());
    declare_symbol(node.name(), alloca, to_llvm_type(node.type()));

    if (node.init()) {
        node.init()->accept(*this);
        builder_.CreateStore(last_val_, alloca);
    }
}

void LLVMCodegen::visit(const AssignStmt &node) {
    llvm::Value *dest_ptr = nullptr;
    llvm::Type *dest_type = nullptr;

    if (const auto *ident =
            dynamic_cast<const IdentifierExpr *>(&node.target())) {
        const LLVMSymbol *sym = lookup_symbol(ident->name());
        if (!sym) {
            throw std::runtime_error("Undefined variable: " + ident->name());
        }
        dest_ptr = sym->alloca_inst;
        dest_type = sym->type;
    } else if (const auto *unary =
                   dynamic_cast<const UnaryExpr *>(&node.target())) {
        if (unary->op() == UnaryOp::Dereference) {
            unary->operand().accept(
                *this);
            dest_ptr = last_val_;
            dest_type = llvm::Type::getInt32Ty(ctx_);
        }
    } else if (const auto *mem =
                   dynamic_cast<const MemberAccessExpr *>(&node.target())) {
        mem->object().accept(*this);
        llvm::Value *base_ptr = last_val_;

        const StructInfo &info = struct_defs_.at("Node");
        unsigned field_idx = info.field_indices.at(mem->member());

        dest_ptr = builder_.CreateStructGEP(info.llvm_type, base_ptr, field_idx,
                                            mem->member() + "_ptr");
        dest_type = to_llvm_type(*info.field_types.at(mem->member()));
    } else if (const auto *idx =
                   dynamic_cast<const IndexExpr *>(&node.target())) {
        idx->object().accept(*this);
        llvm::Value *base_ptr = last_val_;
        idx->index().accept(*this);
        llvm::Value *index_val = last_val_;

        llvm::Type *elem_type = llvm::Type::getInt32Ty(ctx_); // element type
        dest_ptr = builder_.CreateGEP(elem_type, base_ptr, index_val, "elem_ptr");
        dest_type = elem_type;
    }

    if (!dest_ptr) {
        throw std::runtime_error("Invalid lvalue target in assignment");
    }

    node.value().accept(*this);
    llvm::Value *rhs_val = last_val_;

    llvm::Value *final_val = rhs_val;

    if (node.op() != AssignOp::Assign) {
        llvm::Value *current_val =
            builder_.CreateLoad(dest_type, dest_ptr, "current_val");

        switch (node.op()) {
        case AssignOp::AddAssign: // x += y -> x = x + y
            final_val = builder_.CreateAdd(current_val, rhs_val, "add_tmp");
            break;
        case AssignOp::SubAssign: // x -= y -> x = x - y
            final_val = builder_.CreateSub(current_val, rhs_val, "sub_tmp");
            break;
        case AssignOp::MulAssign: // x *= y -> x = x * y
            final_val = builder_.CreateMul(current_val, rhs_val, "mul_tmp");
            break;
        case AssignOp::DivAssign: // x /= y -> x = x / y
            final_val = builder_.CreateSDiv(current_val, rhs_val, "div_tmp");
            break;
        default:
            break;
        }
    }

    builder_.CreateStore(final_val, dest_ptr);
}

void LLVMCodegen::visit(const ExprStmt &node) { (void)node; }

void LLVMCodegen::visit(const IfStmt &node) {
    node.cond().accept(*this);
    llvm::Value *cond_val = last_val_;

    // integer truthy
    if (!cond_val->getType()->isIntegerTy(1)) {
        cond_val = builder_.CreateICmpNE(
            cond_val,
            llvm::ConstantInt::get(cond_val->getType(), 0),
            "ifcond");
    }

    llvm::BasicBlock *then_bb = llvm::BasicBlock::Create(ctx_, "if.then", current_func_);
    llvm::BasicBlock *else_bb = nullptr;
    llvm::BasicBlock *merge_bb = llvm::BasicBlock::Create(ctx_, "if.end", current_func_);

    if (node.else_branch()) {
        else_bb = llvm::BasicBlock::Create(ctx_, "if.else", current_func_);
        builder_.CreateCondBr(cond_val, then_bb, else_bb);
    } else {
        builder_.CreateCondBr(cond_val, then_bb, merge_bb);
    }

    builder_.SetInsertPoint(then_bb);
    node.then_branch().accept(*this);

    if (!builder_.GetInsertBlock()->getTerminator()) {
        builder_.CreateBr(merge_bb);
    }

    if (else_bb) {
        builder_.SetInsertPoint(else_bb);
        node.else_branch()->accept(*this);

        if (!builder_.GetInsertBlock()->getTerminator()) {
            builder_.CreateBr(merge_bb);
        }
    }

    builder_.SetInsertPoint(merge_bb);
}

void LLVMCodegen::visit(const WhileStmt &node) { (void)node; }

void LLVMCodegen::visit(const ForStmt &node) { (void)node; }

void LLVMCodegen::visit(const ReturnStmt &node) {
    if (node.value()) {
        node.value()->accept(*this);
        builder_.CreateRet(last_val_);
    } else {
        builder_.CreateRetVoid();
    }
}

void LLVMCodegen::visit(const DeferStmt &node) { (void)node; }

// Expressions
void LLVMCodegen::visit(const BinaryExpr &node) {
    node.left().accept(*this);
    auto *lhs = last_val_;

    node.right().accept(*this);
    auto *rhs = last_val_;
    switch (node.op()) {
    case BinaryOp::Add:
        last_val_ = builder_.CreateAdd(lhs, rhs);
        break;

    case BinaryOp::Sub:
        last_val_ = builder_.CreateSub(lhs, rhs);
        break;

    case BinaryOp::Mul:
        last_val_ = builder_.CreateMul(lhs, rhs);
        break;

    case BinaryOp::Div:
        last_val_ = builder_.CreateSDiv(lhs, rhs);
        break;
    }
}

void LLVMCodegen::visit(const UnaryExpr &node) { (void)node; }

void LLVMCodegen::visit(const SizeofExpr &node) { (void)node; }

void LLVMCodegen::visit(const PostfixUpdateExpr &node) { (void)node; }

void LLVMCodegen::visit(const CallExpr &node) { (void)node; }

void LLVMCodegen::visit(const IndexExpr &node) { (void)node; }

void LLVMCodegen::visit(const MemberAccessExpr &node) { (void)node; }

void LLVMCodegen::visit(const IntLiteralExpr &node) {
    last_val_ =
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), node.value());
    (void)node;
}

void LLVMCodegen::visit(const FloatLiteralExpr &node) { (void)node; }

void LLVMCodegen::visit(const StringLiteralExpr &node) { (void)node; }

void LLVMCodegen::visit(const CharLiteralExpr &node) { (void)node; }

void LLVMCodegen::visit(const BoolLiteralExpr &node) { (void)node; }

void LLVMCodegen::visit(const NullLiteralExpr &node) { (void)node; }

void LLVMCodegen::visit(const ArrayLiteralExpr &node) { (void)node; }

void LLVMCodegen::visit(const ListLiteralExpr &node) { (void)node; }

void LLVMCodegen::visit(const AllocExpr &node) { (void)node; }

void LLVMCodegen::visit(const IdentifierExpr &node) {
    const LLVMSymbol *s = lookup_symbol(node.name());
    last_val_ = builder_.CreateLoad(s->type, s->alloca_inst, node.name());
}

// Types
void LLVMCodegen::visit(const PrimitiveType &node) { (void)node; }

void LLVMCodegen::visit(const NamedType &node) { (void)node; }

void LLVMCodegen::visit(const ListType &node) { (void)node; }

void LLVMCodegen::visit(const PointerType &node) { (void)node; }

void LLVMCodegen::visit(const SliceType &node) { (void)node; }

void LLVMCodegen::visit(const ArrayType &node) { (void)node; }
