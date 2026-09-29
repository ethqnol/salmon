#include "codegen/llvm_codegen.h"
#include <filesystem>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <unordered_set>

LLVMCodegen::LLVMCodegen(std::ostream &out, DiagnosticEngine *diag)
    : out_(out),
      diag_(diag),
      module_(std::make_unique<llvm::Module>("salmon", ctx_)),
      builder_(ctx_) {
    module_->setTargetTriple(llvm::Triple(llvm::sys::getDefaultTargetTriple()));
    push_scope();

    llvm::Type *ptr_ty = llvm::PointerType::getUnqual(ctx_);
    llvm::Type *i64_ty = llvm::Type::getInt64Ty(ctx_);

    llvm::StructType::create(ctx_, {ptr_ty, i64_ty}, "struct.salmon_slice");
    llvm::StructType::create(ctx_, {ptr_ty, i64_ty, i64_ty},
                             "struct.salmon_list");
}

void LLVMCodegen::error(const SourceLoc &loc, const std::string &msg,
                        const std::vector<std::string> &notes,
                        const std::vector<std::string> &suggestions) {
    if (diag_) {
        diag_->error(loc, msg, notes, suggestions);
        throw CompileError("Codegen error: " + msg);
    }
    throw CompileError("[" + loc.to_string() + "] " + msg);
}

std::vector<std::string> LLVMCodegen::get_visible_symbols() const {
    std::vector<std::string> names;
    for (const auto &scope : scopes_) {
        for (const auto &[name, sym] : scope) {
            names.push_back(name);
        }
    }
    return names;
}

void LLVMCodegen::generate(const Program &program) {
    emit_runtime_decls();
    program.accept(*this);

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
                                 llvm::AllocaInst *inst, llvm::Type *type,
                                 const Type *ast_type) {
    if (scopes_.empty()) {
        return false;
    }
    scopes_.back()[name] = LLVMSymbol{name, inst, type, ast_type};
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
    case PrimitiveKind::String:
        return llvm::PointerType::getUnqual(ctx_);
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

    if (!module_->getFunction("printf")) {
        llvm::FunctionType *printf_ty =
            llvm::FunctionType::get(i32_ty, {ptr_ty}, true);
        llvm::Function::Create(printf_ty, llvm::Function::ExternalLinkage, "printf",
                               *module_);
    }

    if (!module_->getFunction("malloc")) {
        llvm::FunctionType *malloc_ty =
            llvm::FunctionType::get(ptr_ty, {i64_ty}, false);
        llvm::Function::Create(malloc_ty, llvm::Function::ExternalLinkage, "malloc",
                               *module_);
    }

    if (!module_->getFunction("realloc")) {
        llvm::FunctionType *realloc_ty =
            llvm::FunctionType::get(ptr_ty, {ptr_ty, i64_ty}, false);
        llvm::Function::Create(realloc_ty, llvm::Function::ExternalLinkage,
                               "realloc", *module_);
    }

    if (!module_->getFunction("free")) {
        llvm::FunctionType *free_ty =
            llvm::FunctionType::get(void_ty, {ptr_ty}, false);
        llvm::Function::Create(free_ty, llvm::Function::ExternalLinkage, "free",
                               *module_);
    }
}

void LLVMCodegen::emit_push_definition() {
    llvm::Type *ptr_ty = llvm::PointerType::getUnqual(ctx_);
    llvm::Type *i32_ty = llvm::Type::getInt32Ty(ctx_);
    llvm::Type *i64_ty = llvm::Type::getInt64Ty(ctx_);
    llvm::Type *void_ty = llvm::Type::getVoidTy(ctx_);
    llvm::Type *list_ty =
        llvm::StructType::getTypeByName(ctx_, "struct.salmon_list");

    llvm::Function *push_fn = module_->getFunction("push");
    if (push_fn && !push_fn->isDeclaration()) {
        return;
    }
    if (!push_fn) {
        llvm::FunctionType *push_ty =
            llvm::FunctionType::get(void_ty, {ptr_ty, i32_ty}, false);
        push_fn = llvm::Function::Create(
            push_ty, llvm::Function::ExternalLinkage, "push", *module_);
    }

    llvm::BasicBlock *prev_block = builder_.GetInsertBlock();
    llvm::Function *prev_func = current_func_;

    llvm::BasicBlock *entry_bb =
        llvm::BasicBlock::Create(ctx_, "entry", push_fn);
    llvm::BasicBlock *grow_bb =
        llvm::BasicBlock::Create(ctx_, "grow", push_fn);
    llvm::BasicBlock *store_bb =
        llvm::BasicBlock::Create(ctx_, "store", push_fn);

    builder_.SetInsertPoint(entry_bb);
    auto arg_it = push_fn->arg_begin();
    llvm::Value *list_ptr = &*arg_it++;
    list_ptr->setName("list");
    llvm::Value *val = &*arg_it;
    val->setName("val");

    llvm::Value *data_gep =
        builder_.CreateStructGEP(list_ty, list_ptr, 0, "data_gep");
    llvm::Value *len_gep =
        builder_.CreateStructGEP(list_ty, list_ptr, 1, "len_gep");
    llvm::Value *cap_gep =
        builder_.CreateStructGEP(list_ty, list_ptr, 2, "cap_gep");

    llvm::Value *len = builder_.CreateLoad(i64_ty, len_gep, "len");
    llvm::Value *cap = builder_.CreateLoad(i64_ty, cap_gep, "cap");

    llvm::Value *need_grow = builder_.CreateICmpUGE(len, cap, "need_grow");
    builder_.CreateCondBr(need_grow, grow_bb, store_bb);

    builder_.SetInsertPoint(grow_bb);
    llvm::Value *cap_is_zero =
        builder_.CreateICmpEQ(cap, llvm::ConstantInt::get(i64_ty, 0), "cap_zero");
    llvm::Value *double_cap =
        builder_.CreateShl(cap, llvm::ConstantInt::get(i64_ty, 1), "double_cap");
    llvm::Value *new_cap = builder_.CreateSelect(
        cap_is_zero, llvm::ConstantInt::get(i64_ty, 4), double_cap, "new_cap");
    llvm::Value *new_bytes = builder_.CreateMul(
        new_cap, llvm::ConstantInt::get(i64_ty, sizeof(int32_t)), "new_bytes");
    llvm::Value *old_data = builder_.CreateLoad(ptr_ty, data_gep, "old_data");
    llvm::Function *realloc_fn = module_->getFunction("realloc");
    llvm::Value *new_data =
        builder_.CreateCall(realloc_fn, {old_data, new_bytes}, "new_data");
    builder_.CreateStore(new_data, data_gep);
    builder_.CreateStore(new_cap, cap_gep);
    builder_.CreateBr(store_bb);

    builder_.SetInsertPoint(store_bb);
    llvm::Value *cur_data = builder_.CreateLoad(ptr_ty, data_gep, "cur_data");
    llvm::Value *cur_len = builder_.CreateLoad(i64_ty, len_gep, "cur_len");
    llvm::Value *slot_ptr =
        builder_.CreateGEP(i32_ty, cur_data, cur_len, "slot_ptr");
    builder_.CreateStore(val, slot_ptr);
    llvm::Value *inc_len =
        builder_.CreateAdd(cur_len, llvm::ConstantInt::get(i64_ty, 1), "inc_len");
    builder_.CreateStore(inc_len, len_gep);
    builder_.CreateRetVoid();

    if (prev_block) {
        builder_.SetInsertPoint(prev_block);
    }
    current_func_ = prev_func;
}

llvm::Function *LLVMCodegen::get_or_create_strcat() {
    llvm::Type *ptr_ty = llvm::PointerType::getUnqual(ctx_);
    llvm::Type *i64_ty = llvm::Type::getInt64Ty(ctx_);
    llvm::Type *i8_ty = llvm::Type::getInt8Ty(ctx_);

    llvm::Function *strcat_fn = module_->getFunction("salmon_strcat");
    if (strcat_fn && !strcat_fn->isDeclaration()) {
        return strcat_fn;
    }
    if (!strcat_fn) {
        llvm::FunctionType *fn_ty = llvm::FunctionType::get(ptr_ty, {ptr_ty, ptr_ty}, false);
        strcat_fn = llvm::Function::Create(fn_ty, llvm::Function::InternalLinkage, "salmon_strcat", *module_);
    }

    if (!module_->getFunction("strlen")) {
        llvm::FunctionType *strlen_ty = llvm::FunctionType::get(i64_ty, {ptr_ty}, false);
        llvm::Function::Create(strlen_ty, llvm::Function::ExternalLinkage, "strlen", *module_);
    }
    if (!module_->getFunction("malloc")) {
        llvm::FunctionType *malloc_ty = llvm::FunctionType::get(ptr_ty, {i64_ty}, false);
        llvm::Function::Create(malloc_ty, llvm::Function::ExternalLinkage, "malloc", *module_);
    }
    if (!module_->getFunction("memcpy")) {
        llvm::FunctionType *memcpy_ty = llvm::FunctionType::get(ptr_ty, {ptr_ty, ptr_ty, i64_ty}, false);
        llvm::Function::Create(memcpy_ty, llvm::Function::ExternalLinkage, "memcpy", *module_);
    }

    llvm::BasicBlock *prev_block = builder_.GetInsertBlock();
    llvm::Function *prev_func = current_func_;

    llvm::BasicBlock *entry_bb = llvm::BasicBlock::Create(ctx_, "entry", strcat_fn);
    builder_.SetInsertPoint(entry_bb);

    auto it = strcat_fn->arg_begin();
    llvm::Value *s1 = &*it++;
    s1->setName("s1");
    llvm::Value *s2 = &*it++;
    s2->setName("s2");

    llvm::Function *strlen_fn = module_->getFunction("strlen");
    llvm::Function *malloc_fn = module_->getFunction("malloc");
    llvm::Function *memcpy_fn = module_->getFunction("memcpy");

    llvm::Value *len1 = builder_.CreateCall(strlen_fn, {s1}, "len1");
    llvm::Value *len2 = builder_.CreateCall(strlen_fn, {s2}, "len2");
    llvm::Value *sum_len = builder_.CreateAdd(len1, len2, "sum_len");
    llvm::Value *total_len = builder_.CreateAdd(sum_len, llvm::ConstantInt::get(i64_ty, 1), "total_len");
    llvm::Value *buf = builder_.CreateCall(malloc_fn, {total_len}, "buf");
    builder_.CreateCall(memcpy_fn, {buf, s1, len1});
    llvm::Value *buf2 = builder_.CreateGEP(i8_ty, buf, len1, "buf2");
    builder_.CreateCall(memcpy_fn, {buf2, s2, len2});
    llvm::Value *buf_end = builder_.CreateGEP(i8_ty, buf, sum_len, "buf_end");
    builder_.CreateStore(llvm::ConstantInt::get(i8_ty, 0), buf_end);
    builder_.CreateRet(buf);

    if (prev_block) {
        builder_.SetInsertPoint(prev_block);
    }
    current_func_ = prev_func;

    return strcat_fn;
}

const Type *LLVMCodegen::infer_type(const Expr &expr) const {
    if (const auto *ident = dynamic_cast<const IdentifierExpr *>(&expr)) {
        const LLVMSymbol *sym = lookup_symbol(ident->name());
        if (sym) {
            return sym->ast_type;
        }
        return nullptr;
    }
    if (dynamic_cast<const StringLiteralExpr *>(&expr)) {
        static const PrimitiveType str_type(PrimitiveKind::String);
        return &str_type;
    }
    if (const auto *call = dynamic_cast<const CallExpr *>(&expr)) {
        std::string callee_name;
        if (const auto *ident = dynamic_cast<const IdentifierExpr *>(&call->callee())) {
            callee_name = ident->name();
        } else if (const auto *mem = dynamic_cast<const MemberAccessExpr *>(&call->callee())) {
            callee_name = mem->member();
        }
        if (!callee_name.empty()) {
            auto it = func_ret_types_.find(callee_name);
            if (it != func_ret_types_.end()) {
                return it->second;
            }
        }
        return nullptr;
    }
    if (const auto *bin = dynamic_cast<const BinaryExpr *>(&expr)) {
        if (bin->op() == BinaryOp::Add && (is_string_type(bin->left()) || is_string_type(bin->right()))) {
            static const PrimitiveType str_type(PrimitiveKind::String);
            return &str_type;
        }
        return infer_type(bin->left());
    }
    return nullptr;
}

bool LLVMCodegen::is_string_type(const Expr &expr) const {
    const Type *t = infer_type(expr);
    if (!t) {
        return false;
    }
    if (const auto *prim = dynamic_cast<const PrimitiveType *>(t)) {
        return prim->kind() == PrimitiveKind::String;
    }
    return false;
}

void LLVMCodegen::visit(const Program &node) {
    for (const auto &decl : node.decls()) {
        if (const auto *fn = dynamic_cast<const FunctionDecl *>(decl.get())) {
            func_ret_types_[fn->name()] = fn->ret_type();
        }
    }
    emit_runtime_decls();
    emit_push_definition();
    for (const auto &decl : node.decls()) {
        decl->accept(*this);
    }
}

void LLVMCodegen::visit(const IncludeDirective &node) {
    (void)node;
}

void LLVMCodegen::visit(const StructField &node) { (void)node; }

void LLVMCodegen::visit(const StructDecl &node) {
    std::string sname = "struct." + node.name();
    llvm::StructType *st = llvm::StructType::getTypeByName(ctx_, sname);
    if (!st) {
        st = llvm::StructType::create(ctx_, sname);
    } else if (!st->isOpaque()) {
        error(node.loc(), "redefinition of struct '" + node.name() + "'");
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
        llvm::FunctionType::get(ret_t, param_types, node.is_vararg());

    llvm::Function *func = module_->getFunction(node.name());
    if (node.is_extern()) {
        if (!func) {
            llvm::Function::Create(func_t, llvm::Function::ExternalLinkage, node.name(), *module_);
        }
        return;
    }

    if (!func) {
        func = llvm::Function::Create(
            func_t, llvm::Function::ExternalLinkage, node.name(), *module_);
    } else if (!func->isDeclaration()) {
        error(node.loc(), "redefinition of function '" + node.name() + "'");
    }

    size_t idx = 0;
    for (auto &arg : func->args()) {
        arg.setName(node.params()[idx++]->name());
    }

    current_func_ = func;
    push_scope();

    llvm::BasicBlock *entry_bb = llvm::BasicBlock::Create(ctx_, "entry", func);
    builder_.SetInsertPoint(entry_bb);

    idx = 0;
    for (auto &arg : func->args()) {
        llvm::AllocaInst *alloca =
            builder_.CreateAlloca(arg.getType(), nullptr, arg.getName() + ".addr");
        builder_.CreateStore(&arg, alloca);
        declare_symbol(std::string(arg.getName()), alloca, arg.getType(), &node.params()[idx++]->type());
    }

    if (node.body()) {
        node.body()->accept(*this);
    }

    if (!builder_.GetInsertBlock()->getTerminator()) {
        for (auto it = defer_stack_.rbegin(); it != defer_stack_.rend(); ++it) {
            (*it)->accept(*this);
        }
        if (ret_t->isVoidTy()) {
            builder_.CreateRetVoid();
        } else {
            builder_.CreateUnreachable();
        }
    }

    defer_stack_.clear();
    pop_scope();
    current_func_ = nullptr;
}

void LLVMCodegen::visit(const BlockStmt &node) {
    for (const auto &stmt : node.stmts()) {
        stmt->accept(*this);
    }
}

void LLVMCodegen::visit(const VarDeclStmt &node) {
    llvm::Type *decl_ty = to_llvm_type(node.type());
    llvm::AllocaInst *alloca =
        builder_.CreateAlloca(decl_ty, nullptr, node.name());
    declare_symbol(node.name(), alloca, decl_ty, &node.type());

    if (node.init()) {
        node.init()->accept(*this);
        llvm::Value *val = last_val_;
        if (val && val->getType()->isIntegerTy() && decl_ty->isIntegerTy()) {
            if (val->getType() != decl_ty) {
                val = builder_.CreateZExtOrTrunc(val, decl_ty, "init_cast");
            }
        }
        builder_.CreateStore(val, alloca);
    }
}

void LLVMCodegen::visit(const AssignStmt &node) {
    llvm::Value *dest_ptr = nullptr;
    llvm::Type *dest_type = nullptr;

    if (const auto *ident =
            dynamic_cast<const IdentifierExpr *>(&node.target())) {
        const LLVMSymbol *sym = lookup_symbol(ident->name());
        if (!sym) {
            std::vector<std::string> visible = get_visible_symbols();
            std::string similar = DiagnosticEngine::find_similar(ident->name(), visible);
            std::vector<std::string> suggestions;
            if (!similar.empty()) {
                suggestions.push_back("did you mean '" + similar + "'?");
            }
            error(ident->loc(), "undefined variable '" + ident->name() + "'", {}, suggestions);
        }
        dest_ptr = sym->alloca_inst;
        dest_type = sym->type;
    } else if (const auto *unary =
                   dynamic_cast<const UnaryExpr *>(&node.target())) {
        if (unary->op() == UnaryOp::Dereference) {
            unary->operand().accept(*this);
            dest_ptr = last_val_;
            dest_type = llvm::Type::getInt32Ty(ctx_);
        }
    } else if (const auto *mem =
                   dynamic_cast<const MemberAccessExpr *>(&node.target())) {
        mem->object().accept(*this);
        llvm::Value *base_ptr = last_val_;

        const StructInfo *found_info = nullptr;
        for (const auto &[sname, info] : struct_defs_) {
            if (info.field_indices.find(mem->member()) != info.field_indices.end()) {
                found_info = &info;
                break;
            }
        }

        if (found_info) {
            unsigned field_idx = found_info->field_indices.at(mem->member());
            dest_ptr = builder_.CreateStructGEP(found_info->llvm_type, base_ptr, field_idx,
                                                mem->member() + "_ptr");
            dest_type = to_llvm_type(*found_info->field_types.at(mem->member()));
        } else {
            error(mem->loc(), "unknown struct member '" + mem->member() + "'");
        }
    } else if (const auto *idx =
                   dynamic_cast<const IndexExpr *>(&node.target())) {
        idx->object().accept(*this);
        llvm::Value *base_ptr = last_val_;
        if (base_ptr && base_ptr->getType()->isStructTy()) {
            base_ptr = builder_.CreateExtractValue(base_ptr, 0, "buf");
        } else if (const auto *ident =
                       dynamic_cast<const IdentifierExpr *>(&idx->object())) {
            const LLVMSymbol *s = lookup_symbol(ident->name());
            if (s && s->type->isArrayTy()) {
                base_ptr = s->alloca_inst;
            }
        }
        idx->index().accept(*this);
        llvm::Value *index_val = last_val_;

        llvm::Type *elem_type = llvm::Type::getInt32Ty(ctx_);
        if (is_string_type(idx->object())) {
            elem_type = llvm::Type::getInt8Ty(ctx_);
        }
        dest_ptr = builder_.CreateGEP(elem_type, base_ptr, index_val, "elem_ptr");
        dest_type = elem_type;
    }

    if (!dest_ptr) {
        error(node.target().loc(), "invalid lvalue target in assignment");
    }

    node.value().accept(*this);
    llvm::Value *rhs_val = last_val_;
    llvm::Value *final_val = rhs_val;

    if (node.op() != AssignOp::Assign) {
        llvm::Value *current_val =
            builder_.CreateLoad(dest_type, dest_ptr, "current_val");

        switch (node.op()) {
        case AssignOp::AddAssign:
            final_val = builder_.CreateAdd(current_val, rhs_val, "add_tmp");
            break;
        case AssignOp::SubAssign:
            final_val = builder_.CreateSub(current_val, rhs_val, "sub_tmp");
            break;
        case AssignOp::MulAssign:
            final_val = builder_.CreateMul(current_val, rhs_val, "mul_tmp");
            break;
        case AssignOp::DivAssign:
            final_val = builder_.CreateSDiv(current_val, rhs_val, "div_tmp");
            break;
        default:
            break;
        }
    }

    if (final_val && dest_type && final_val->getType()->isIntegerTy() && dest_type->isIntegerTy()) {
        if (final_val->getType() != dest_type) {
            final_val = builder_.CreateZExtOrTrunc(final_val, dest_type, "assign_cast");
        }
    }

    builder_.CreateStore(final_val, dest_ptr);
}

void LLVMCodegen::visit(const ExprStmt &node) {
    node.expr().accept(*this);
}

void LLVMCodegen::visit(const IfStmt &node) {
    node.cond().accept(*this);
    llvm::Value *cond_val = last_val_;

    if (!cond_val->getType()->isIntegerTy(1)) {
        cond_val = builder_.CreateICmpNE(
            cond_val, llvm::ConstantInt::get(cond_val->getType(), 0), "ifcond");
    }

    llvm::BasicBlock *then_bb =
        llvm::BasicBlock::Create(ctx_, "if.then", current_func_);
    llvm::BasicBlock *else_bb = nullptr;
    llvm::BasicBlock *merge_bb =
        llvm::BasicBlock::Create(ctx_, "if.end", current_func_);

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

void LLVMCodegen::visit(const WhileStmt &node) {
    llvm::BasicBlock *cond_bb =
        llvm::BasicBlock::Create(ctx_, "while.cond", current_func_);
    llvm::BasicBlock *body_bb =
        llvm::BasicBlock::Create(ctx_, "while.body", current_func_);
    llvm::BasicBlock *exit_bb =
        llvm::BasicBlock::Create(ctx_, "while.end", current_func_);

    builder_.CreateBr(cond_bb);

    builder_.SetInsertPoint(cond_bb);

    node.cond().accept(*this);
    llvm::Value *cond_val = last_val_;

    if (!cond_val->getType()->isIntegerTy(1)) {
        cond_val = builder_.CreateICmpNE(
            cond_val, llvm::ConstantInt::get(cond_val->getType(), 0), "whilecond");
    }
    builder_.CreateCondBr(cond_val, body_bb, exit_bb);

    builder_.SetInsertPoint(body_bb);
    node.body().accept(*this);
    if (!builder_.GetInsertBlock()->getTerminator()) {
        builder_.CreateBr(cond_bb);
    }

    builder_.SetInsertPoint(exit_bb);
}

void LLVMCodegen::visit(const ForStmt &node) {

    llvm::BasicBlock *cond_bb =
        llvm::BasicBlock::Create(ctx_, "for.cond", current_func_);
    llvm::BasicBlock *body_bb =
        llvm::BasicBlock::Create(ctx_, "for.body", current_func_);
    llvm::BasicBlock *step_bb =
        llvm::BasicBlock::Create(ctx_, "for.step", current_func_);
    llvm::BasicBlock *exit_bb =
        llvm::BasicBlock::Create(ctx_, "for.end", current_func_);

    push_scope();
    if (node.init()) {
        node.init()->accept(*this);
    }

    builder_.CreateBr(cond_bb);

    builder_.SetInsertPoint(cond_bb);
    if (node.cond()) {
        node.cond()->accept(*this);
        llvm::Value *cond_val = last_val_;
        if (!cond_val->getType()->isIntegerTy(1)) {
            cond_val = builder_.CreateICmpNE(
                cond_val,
                llvm::ConstantInt::get(cond_val->getType(), 0),
                "forcond");
        }
        builder_.CreateCondBr(cond_val, body_bb, exit_bb);
    } else {
        builder_.CreateBr(body_bb);
    }

    builder_.SetInsertPoint(body_bb);
    node.body().accept(*this);
    if (!builder_.GetInsertBlock()->getTerminator()) {
        builder_.CreateBr(step_bb);
    }

    builder_.SetInsertPoint(step_bb);
    if (node.update()) {
        node.update()->accept(*this);
    }

    builder_.CreateBr(cond_bb);
    builder_.SetInsertPoint(exit_bb);
    pop_scope();
}

void LLVMCodegen::visit(const ReturnStmt &node) {
    llvm::Value *ret_val = nullptr;
    if (node.value()) {
        node.value()->accept(*this);
        ret_val = last_val_;
    }

    for (auto it = defer_stack_.rbegin(); it != defer_stack_.rend(); ++it) {
        (*it)->accept(*this);
    }

    if (ret_val) {
        if (current_func_ && current_func_->getReturnType()->isIntegerTy() &&
            ret_val->getType()->isIntegerTy() &&
            ret_val->getType() != current_func_->getReturnType()) {
            ret_val = builder_.CreateZExtOrTrunc(ret_val, current_func_->getReturnType(), "ret_cast");
        }
        builder_.CreateRet(ret_val);
    } else {
        builder_.CreateRetVoid();
    }
}

void LLVMCodegen::visit(const DeferStmt &node) {
    defer_stack_.push_back(&node.stmt());
}

void LLVMCodegen::visit(const BinaryExpr &node) {
    node.left().accept(*this);
    auto *lhs = last_val_;

    node.right().accept(*this);
    auto *rhs = last_val_;

    if (node.op() == BinaryOp::Add && (is_string_type(node.left()) || is_string_type(node.right()))) {
        last_val_ = builder_.CreateCall(get_or_create_strcat(), {lhs, rhs}, "strcat_tmp");
        return;
    }

    if ((node.op() == BinaryOp::Equal || node.op() == BinaryOp::NotEqual) &&
        (is_string_type(node.left()) || is_string_type(node.right()))) {
        bool left_null = dynamic_cast<const NullLiteralExpr *>(&node.left()) != nullptr;
        bool right_null = dynamic_cast<const NullLiteralExpr *>(&node.right()) != nullptr;
        if (!left_null && !right_null) {
            if (!module_->getFunction("strcmp")) {
                llvm::FunctionType *strcmp_ty = llvm::FunctionType::get(
                    llvm::Type::getInt32Ty(ctx_),
                    {llvm::PointerType::getUnqual(ctx_), llvm::PointerType::getUnqual(ctx_)},
                    false);
                llvm::Function::Create(strcmp_ty, llvm::Function::ExternalLinkage, "strcmp", *module_);
            }
            llvm::Function *strcmp_fn = module_->getFunction("strcmp");
            llvm::Value *cmp_res = builder_.CreateCall(strcmp_fn, {lhs, rhs}, "strcmp_res");
            if (node.op() == BinaryOp::Equal) {
                last_val_ = builder_.CreateICmpEQ(cmp_res, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 0), "streq_tmp");
            } else {
                last_val_ = builder_.CreateICmpNE(cmp_res, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 0), "strne_tmp");
            }
            return;
        }
    }

    if (lhs && rhs && lhs->getType()->isIntegerTy() && rhs->getType()->isIntegerTy()) {
        unsigned lhs_bits = lhs->getType()->getIntegerBitWidth();
        unsigned rhs_bits = rhs->getType()->getIntegerBitWidth();
        if (lhs_bits < rhs_bits) {
            lhs = builder_.CreateSExt(lhs, rhs->getType(), "sext_lhs");
        } else if (rhs_bits < lhs_bits) {
            rhs = builder_.CreateSExt(rhs, lhs->getType(), "sext_rhs");
        }
    }

    switch (node.op()) {
    case BinaryOp::Add:
        last_val_ = builder_.CreateAdd(lhs, rhs, "add_tmp");
        break;
    case BinaryOp::Sub:
        last_val_ = builder_.CreateSub(lhs, rhs, "sub_tmp");
        break;
    case BinaryOp::Mul:
        last_val_ = builder_.CreateMul(lhs, rhs, "mul_tmp");
        break;
    case BinaryOp::Div:
        last_val_ = builder_.CreateSDiv(lhs, rhs, "div_tmp");
        break;
    case BinaryOp::Mod:
        last_val_ = builder_.CreateSRem(lhs, rhs, "mod_tmp");
        break;
    case BinaryOp::Equal:
        last_val_ = builder_.CreateICmpEQ(lhs, rhs, "eq_tmp");
        break;
    case BinaryOp::NotEqual:
        last_val_ = builder_.CreateICmpNE(lhs, rhs, "ne_tmp");
        break;
    case BinaryOp::Less:
        last_val_ = builder_.CreateICmpSLT(lhs, rhs, "lt_tmp");
        break;
    case BinaryOp::LessEqual:
        last_val_ = builder_.CreateICmpSLE(lhs, rhs, "le_tmp");
        break;
    case BinaryOp::Greater:
        last_val_ = builder_.CreateICmpSGT(lhs, rhs, "gt_tmp");
        break;
    case BinaryOp::GreaterEqual:
        last_val_ = builder_.CreateICmpSGE(lhs, rhs, "ge_tmp");
        break;
    case BinaryOp::LogicalAnd:
        last_val_ = builder_.CreateLogicalAnd(lhs, rhs, "and_tmp");
        break;
    case BinaryOp::LogicalOr:
        last_val_ = builder_.CreateLogicalOr(lhs, rhs, "or_tmp");
        break;
    }
}

void LLVMCodegen::visit(const UnaryExpr &node) {
    switch (node.op()) {
    case UnaryOp::LogicalNot: {
        node.operand().accept(*this);
        llvm::Value *val = last_val_;
        if (!val->getType()->isIntegerTy(1)) {
            val = builder_.CreateICmpNE(
                val, llvm::ConstantInt::get(val->getType(), 0), "not_cond");
        }
        last_val_ = builder_.CreateNot(val, "lnot_tmp");
        break;
    }
    case UnaryOp::Negate: {
        node.operand().accept(*this);
        last_val_ = builder_.CreateNeg(last_val_, "neg_tmp");
        break;
    }
    case UnaryOp::AddressOf: {
        if (const auto *ident =
                dynamic_cast<const IdentifierExpr *>(&node.operand())) {
            const LLVMSymbol *s = lookup_symbol(ident->name());
            if (!s) {
                std::vector<std::string> visible = get_visible_symbols();
                std::string similar =
                    DiagnosticEngine::find_similar(ident->name(), visible);
                std::vector<std::string> suggestions;
                if (!similar.empty()) {
                    suggestions.push_back("did you mean '" + similar + "'?");
                }
                error(ident->loc(), "undefined variable '" + ident->name() + "'",
                      {}, suggestions);
            }
            last_val_ = s->alloca_inst;
        } else {
            error(node.loc(), "cannot take address of non-lvalue");
        }
        break;
    }
    case UnaryOp::Dereference: {
        node.operand().accept(*this);
        last_val_ = builder_.CreateLoad(llvm::Type::getInt32Ty(ctx_), last_val_,
                                        "deref_tmp");
        break;
    }
    }
}

void LLVMCodegen::visit(const SizeofExpr &node) {
    const llvm::DataLayout &dl = module_->getDataLayout();
    llvm::Type *target_ty = nullptr;

    if (const auto *ident =
            dynamic_cast<const IdentifierExpr *>(&node.operand())) {
        const std::string &name = ident->name();

        if (name == "int" || name == "i32" || name == "uint" || name == "u32") {
            target_ty = llvm::Type::getInt32Ty(ctx_);
        } else if (name == "i64" || name == "u64") {
            target_ty = llvm::Type::getInt64Ty(ctx_);
        } else if (name == "i16" || name == "u16") {
            target_ty = llvm::Type::getInt16Ty(ctx_);
        } else if (name == "i8" || name == "u8" || name == "char") {
            target_ty = llvm::Type::getInt8Ty(ctx_);
        } else if (name == "float" || name == "f32") {
            target_ty = llvm::Type::getFloatTy(ctx_);
        } else if (name == "f64") {
            target_ty = llvm::Type::getDoubleTy(ctx_);
        } else if (name == "bool") {
            target_ty = llvm::Type::getInt1Ty(ctx_);
        } else if (struct_defs_.find(name) != struct_defs_.end()) {
            target_ty = struct_defs_.at(name).llvm_type;
        } else if (auto *st = llvm::StructType::getTypeByName(ctx_, "struct." + name)) {
            target_ty = st;
        } else if (const LLVMSymbol *sym = lookup_symbol(name)) {
            target_ty = sym->type;
        } else {
            std::vector<std::string> candidates = get_visible_symbols();
            for (const auto &[sname, _] : struct_defs_) {
                candidates.push_back(sname);
            }
            std::string sim = DiagnosticEngine::find_similar(name, candidates);
            std::vector<std::string> suggestions;
            if (!sim.empty()) {
                suggestions.push_back("did you mean '" + sim + "'?");
            }
            error(ident->loc(), "unknown type or variable '" + name + "' in sizeof",
                  {}, suggestions);
        }
    } else {
        node.operand().accept(*this);
        if (last_val_) {
            target_ty = last_val_->getType();
        }
    }

    if (!target_ty) {
        error(node.loc(), "cannot determine type for sizeof expression");
    }

    uint64_t size_bytes = dl.getTypeAllocSize(target_ty);
    last_val_ = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), size_bytes);
}

void LLVMCodegen::visit(const PostfixUpdateExpr &node) {
    if (const auto *ident =
            dynamic_cast<const IdentifierExpr *>(&node.operand())) {
        const LLVMSymbol *s = lookup_symbol(ident->name());
        if (!s) {
            std::vector<std::string> visible = get_visible_symbols();
            std::string similar =
                DiagnosticEngine::find_similar(ident->name(), visible);
            std::vector<std::string> suggestions;
            if (!similar.empty()) {
                suggestions.push_back("did you mean '" + similar + "'?");
            }
            error(ident->loc(), "undefined variable '" + ident->name() + "'",
                  {}, suggestions);
        }
        llvm::Value *cur = builder_.CreateLoad(s->type, s->alloca_inst, "cur");
        llvm::Value *step = llvm::ConstantInt::get(s->type, 1);
        llvm::Value *updated = nullptr;
        if (node.op() == PostfixOp::PostIncrement) {
            updated = builder_.CreateAdd(cur, step, "inc");
        } else {
            updated = builder_.CreateSub(cur, step, "dec");
        }
        builder_.CreateStore(updated, s->alloca_inst);
        last_val_ = cur;
    } else {
        error(node.loc(), "postfix ++ / -- only supported on variables");
    }
}

void LLVMCodegen::visit(const CallExpr &node) {
    std::string callee_name;
    std::vector<llvm::Value *> args;

    if (const auto *ident =
            dynamic_cast<const IdentifierExpr *>(&node.callee())) {
        callee_name = ident->name();
    } else if (const auto *mem =
                   dynamic_cast<const MemberAccessExpr *>(&node.callee())) {
        callee_name = mem->member();
        if (const auto *obj_id =
                dynamic_cast<const IdentifierExpr *>(&mem->object())) {
            const LLVMSymbol *s = lookup_symbol(obj_id->name());
            if (s) {
                llvm::Type *list_ty =
                    llvm::StructType::getTypeByName(ctx_, "struct.salmon_list");
                if (callee_name == "free" && s->type == list_ty) {
                    llvm::Value *data_gep =
                        builder_.CreateStructGEP(list_ty, s->alloca_inst, 0, "list_data_ptr");
                    llvm::Value *data_val =
                        builder_.CreateLoad(llvm::PointerType::getUnqual(ctx_), data_gep, "list_data");
                    llvm::Function *free_fn = module_->getFunction("free");
                    builder_.CreateCall(free_fn, {data_val});
                    builder_.CreateStore(
                        llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(ctx_)),
                        data_gep);
                    llvm::Value *len_gep =
                        builder_.CreateStructGEP(list_ty, s->alloca_inst, 1, "list_len_ptr");
                    builder_.CreateStore(
                        llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0), len_gep);
                    llvm::Value *cap_gep =
                        builder_.CreateStructGEP(list_ty, s->alloca_inst, 2, "list_cap_ptr");
                    builder_.CreateStore(
                        llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0), cap_gep);
                    last_val_ = nullptr;
                    return;
                }
                args.push_back(s->alloca_inst);
            }
        }
    } else {
        error(node.loc(), "complex callee expressions are not supported");
    }

    llvm::Function *callee_fn = module_->getFunction(callee_name);
    if (!callee_fn) {
        std::vector<std::string> fn_names;
        for (const auto &f : module_->functions()) {
            fn_names.push_back(f.getName().str());
        }
        std::string sim = DiagnosticEngine::find_similar(callee_name, fn_names);
        std::vector<std::string> suggestions;
        if (!sim.empty()) {
            suggestions.push_back("did you mean '" + sim + "'?");
        }
        error(node.callee().loc(),
              "call to undefined function '" + callee_name + "'", {},
              suggestions);
    }

    for (size_t i = 0; i < node.args().size(); ++i) {
        node.args()[i]->accept(*this);
        llvm::Value *arg_val = last_val_;

        if (callee_fn && i < callee_fn->getFunctionType()->getNumParams()) {
            llvm::Type *param_ty = callee_fn->getFunctionType()->getParamType(i);
            llvm::Type *slice_ty =
                llvm::StructType::getTypeByName(ctx_, "struct.salmon_slice");

            if (param_ty == slice_ty && arg_val && arg_val->getType()->isArrayTy()) {
                llvm::ArrayType *arr_t =
                    llvm::cast<llvm::ArrayType>(arg_val->getType());
                uint64_t arr_len = arr_t->getNumElements();

                llvm::Value *first_elem_ptr = nullptr;
                if (const auto *ident =
                        dynamic_cast<const IdentifierExpr *>(node.args()[i].get())) {
                    const LLVMSymbol *s = lookup_symbol(ident->name());
                    if (s) {
                        first_elem_ptr = s->alloca_inst;
                    }
                }
                if (!first_elem_ptr) {
                    llvm::AllocaInst *tmp_arr = builder_.CreateAlloca(
                        arr_t, nullptr, "arr_slice_tmp");
                    builder_.CreateStore(arg_val, tmp_arr);
                    first_elem_ptr = tmp_arr;
                }

                llvm::Value *slice_val = llvm::UndefValue::get(slice_ty);
                slice_val = builder_.CreateInsertValue(slice_val, first_elem_ptr, 0, "s_ptr");
                llvm::Value *len_val = llvm::ConstantInt::get(
                    llvm::Type::getInt64Ty(ctx_), arr_len);
                slice_val = builder_.CreateInsertValue(slice_val, len_val, 1, "s_len");
                arg_val = slice_val;
            } else if (arg_val && arg_val->getType()->isIntegerTy() && param_ty->isIntegerTy() &&
                       arg_val->getType() != param_ty) {
                arg_val = builder_.CreateZExtOrTrunc(arg_val, param_ty, "arg_cast");
            }
        }
        args.push_back(arg_val);
    }

    if (callee_fn->getReturnType()->isVoidTy()) {
        last_val_ = builder_.CreateCall(callee_fn, args);
    } else {
        last_val_ =
            builder_.CreateCall(callee_fn, args, callee_name + "_call");
    }
}

void LLVMCodegen::visit(const IndexExpr &node) {
    node.object().accept(*this);
    llvm::Value *base = last_val_;
    node.index().accept(*this);
    llvm::Value *idx = last_val_;

    llvm::Value *buffer = base;
    if (base && base->getType()->isStructTy()) {
        buffer = builder_.CreateExtractValue(base, 0, "buf");
    } else if (const auto *ident =
                   dynamic_cast<const IdentifierExpr *>(&node.object())) {
        const LLVMSymbol *s = lookup_symbol(ident->name());
        if (s && s->type->isArrayTy()) {
            buffer = s->alloca_inst;
        }
    }

    llvm::Type *elem_ty = llvm::Type::getInt32Ty(ctx_);
    if (is_string_type(node.object())) {
        elem_ty = llvm::Type::getInt8Ty(ctx_);
    }
    llvm::Value *elem_ptr =
        builder_.CreateGEP(elem_ty, buffer, idx, "elem_ptr");
    last_val_ = builder_.CreateLoad(elem_ty, elem_ptr, "elem_val");
}

void LLVMCodegen::visit(const MemberAccessExpr &node) {
    node.object().accept(*this);
    llvm::Value *base = last_val_;

    if (node.member() == "len") {
        if (is_string_type(node.object())) {
            if (!module_->getFunction("strlen")) {
                llvm::FunctionType *strlen_ty = llvm::FunctionType::get(
                    llvm::Type::getInt64Ty(ctx_),
                    {llvm::PointerType::getUnqual(ctx_)},
                    false);
                llvm::Function::Create(strlen_ty, llvm::Function::ExternalLinkage, "strlen", *module_);
            }
            llvm::Function *strlen_fn = module_->getFunction("strlen");
            last_val_ = builder_.CreateCall(strlen_fn, {base}, "strlen_res");
            return;
        }
        if (base && base->getType()->isStructTy()) {
            last_val_ = builder_.CreateExtractValue(base, 1, "len");
            return;
        }
        if (base && base->getType()->isPointerTy()) {
            llvm::Type *slice_ty =
                llvm::StructType::getTypeByName(ctx_, "struct.salmon_slice");
            llvm::Value *len_ptr =
                builder_.CreateStructGEP(slice_ty, base, 1, "len_ptr");
            last_val_ = builder_.CreateLoad(llvm::Type::getInt64Ty(ctx_),
                                            len_ptr, "len");
            return;
        }
    }

    for (const auto &[sname, info] : struct_defs_) {
        auto field_it = info.field_indices.find(node.member());
        if (field_it != info.field_indices.end()) {
            if (base && base->getType()->isPointerTy()) {
                llvm::Value *field_ptr = builder_.CreateStructGEP(
                    info.llvm_type, base, field_it->second, node.member() + "_ptr");
                llvm::Type *field_ty = to_llvm_type(*info.field_types.at(node.member()));
                last_val_ =
                    builder_.CreateLoad(field_ty, field_ptr, node.member() + "_val");
            } else if (base && base->getType()->isStructTy()) {
                last_val_ = builder_.CreateExtractValue(base, field_it->second,
                                                        node.member() + "_val");
            }
            return;
        }
    }

    std::vector<std::string> all_fields;
    for (const auto &[sname, info] : struct_defs_) {
        for (const auto &[f, _] : info.field_indices) {
            all_fields.push_back(f);
        }
    }
    std::string sim = DiagnosticEngine::find_similar(node.member(), all_fields);
    std::vector<std::string> suggestions;
    if (!sim.empty()) {
        suggestions.push_back("did you mean '" + sim + "'?");
    }
    error(node.loc(), "unknown member access '" + node.member() + "'", {},
          suggestions);
}

void LLVMCodegen::visit(const IntLiteralExpr &node) {
    last_val_ =
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), node.value());
}

void LLVMCodegen::visit(const FloatLiteralExpr &node) {
    last_val_ =
        llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), node.value());
}

void LLVMCodegen::visit(const StringLiteralExpr &node) {
    last_val_ = builder_.CreateGlobalString(node.value(), "str");
}

void LLVMCodegen::visit(const CharLiteralExpr &node) {
    last_val_ = llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx_),
                                       static_cast<uint8_t>(node.value()));
}

void LLVMCodegen::visit(const BoolLiteralExpr &node) {
    last_val_ = llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx_),
                                       node.value() ? 1 : 0);
}

void LLVMCodegen::visit(const NullLiteralExpr &node) {
    (void)node;
    last_val_ =
        llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(ctx_));
}

void LLVMCodegen::visit(const ArrayLiteralExpr &node) {
    llvm::Type *i32_ty = llvm::Type::getInt32Ty(ctx_);
    llvm::ArrayType *arr_ty =
        llvm::ArrayType::get(i32_ty, node.elements().size());
    llvm::AllocaInst *alloca =
        builder_.CreateAlloca(arr_ty, nullptr, "arr_lit");

    for (size_t i = 0; i < node.elements().size(); ++i) {
        node.elements()[i]->accept(*this);
        llvm::Value *idx = llvm::ConstantInt::get(i32_ty, i);
        llvm::Value *elem_ptr =
            builder_.CreateGEP(arr_ty, alloca,
                               {llvm::ConstantInt::get(i32_ty, 0), idx},
                               "elem_init");
        builder_.CreateStore(last_val_, elem_ptr);
    }
    last_val_ = builder_.CreateLoad(arr_ty, alloca, "arr_val");
}

void LLVMCodegen::visit(const ListLiteralExpr &node) {
    llvm::Type *list_ty =
        llvm::StructType::getTypeByName(ctx_, "struct.salmon_list");
    llvm::AllocaInst *alloca =
        builder_.CreateAlloca(list_ty, nullptr, "list_lit");

    uint64_t count = node.elements().size();
    llvm::Type *i64_ty = llvm::Type::getInt64Ty(ctx_);
    llvm::Type *i32_ty = llvm::Type::getInt32Ty(ctx_);

    llvm::Value *backing_ptr = nullptr;
    if (count > 0) {
        llvm::Function *malloc_fn = module_->getFunction("malloc");
        llvm::Value *bytes =
            llvm::ConstantInt::get(i64_ty, count * sizeof(int32_t));
        backing_ptr = builder_.CreateCall(malloc_fn, {bytes}, "list_buf");
        for (size_t i = 0; i < count; ++i) {
            node.elements()[i]->accept(*this);
            llvm::Value *idx = llvm::ConstantInt::get(i32_ty, i);
            llvm::Value *elem_ptr =
                builder_.CreateGEP(i32_ty, backing_ptr, idx, "list_elem");
            builder_.CreateStore(last_val_, elem_ptr);
        }
    } else {
        backing_ptr = llvm::ConstantPointerNull::get(
            llvm::PointerType::getUnqual(ctx_));
    }

    llvm::Value *data_ptr =
        builder_.CreateStructGEP(list_ty, alloca, 0, "list_data");
    builder_.CreateStore(backing_ptr, data_ptr);

    llvm::Value *len_ptr =
        builder_.CreateStructGEP(list_ty, alloca, 1, "list_len");
    builder_.CreateStore(llvm::ConstantInt::get(i64_ty, count), len_ptr);

    llvm::Value *cap_ptr =
        builder_.CreateStructGEP(list_ty, alloca, 2, "list_cap");
    builder_.CreateStore(llvm::ConstantInt::get(i64_ty, count), cap_ptr);

    last_val_ = builder_.CreateLoad(list_ty, alloca, "list_val");
}

void LLVMCodegen::visit(const AllocExpr &node) {
    llvm::Type *elem_ty = to_llvm_type(node.alloc_type());
    const llvm::DataLayout &dl = module_->getDataLayout();
    uint64_t type_size = dl.getTypeAllocSize(elem_ty);

    llvm::Value *size_val =
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), type_size);
    if (node.count()) {
        node.count()->accept(*this);
        llvm::Value *cnt = last_val_;
        if (cnt->getType() != llvm::Type::getInt64Ty(ctx_)) {
            cnt = builder_.CreateZExtOrTrunc(cnt, llvm::Type::getInt64Ty(ctx_));
        }
        size_val = builder_.CreateMul(size_val, cnt, "alloc_bytes");
    }

    llvm::Function *malloc_fn = module_->getFunction("malloc");
    last_val_ = builder_.CreateCall(malloc_fn, {size_val}, "alloc_ptr");
}

void LLVMCodegen::visit(const IdentifierExpr &node) {
    const LLVMSymbol *s = lookup_symbol(node.name());
    if (!s) {
        std::vector<std::string> visible = get_visible_symbols();
        std::string similar =
            DiagnosticEngine::find_similar(node.name(), visible);
        std::vector<std::string> suggestions;
        if (!similar.empty()) {
            suggestions.push_back("did you mean '" + similar + "'?");
        }
        error(node.loc(), "undefined variable '" + node.name() + "'", {},
              suggestions);
    }
    last_val_ = builder_.CreateLoad(s->type, s->alloca_inst, node.name());
}

void LLVMCodegen::visit(const PrimitiveType &node) { (void)node; }

void LLVMCodegen::visit(const NamedType &node) { (void)node; }

void LLVMCodegen::visit(const ListType &node) { (void)node; }

void LLVMCodegen::visit(const PointerType &node) { (void)node; }

void LLVMCodegen::visit(const SliceType &node) { (void)node; }

void LLVMCodegen::visit(const ArrayType &node) { (void)node; }
