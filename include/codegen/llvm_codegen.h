#pragma once

#include "ast.h"
#include "ast_visitor.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

struct LLVMSymbol {
    std::string name;
    llvm::AllocaInst *alloca_inst{nullptr};
    llvm::Type *type{nullptr};
};

class LLVMCodegen final : public ASTVisitor {
public:
    explicit LLVMCodegen(std::ostream &out);
    ~LLVMCodegen() override = default;

    void generate(const Program &program);

    // Declarations
    void visit(const Program &node) override;
    void visit(const IncludeDirective &node) override;
    void visit(const StructField &node) override;
    void visit(const StructDecl &node) override;
    void visit(const Param &node) override;
    void visit(const FunctionDecl &node) override;

    // Statements
    void visit(const BlockStmt &node) override;
    void visit(const VarDeclStmt &node) override;
    void visit(const AssignStmt &node) override;
    void visit(const ExprStmt &node) override;
    void visit(const IfStmt &node) override;
    void visit(const WhileStmt &node) override;
    void visit(const ForStmt &node) override;
    void visit(const ReturnStmt &node) override;
    void visit(const DeferStmt &node) override;

    // Expressions
    void visit(const BinaryExpr &node) override;
    void visit(const UnaryExpr &node) override;
    void visit(const SizeofExpr &node) override;
    void visit(const PostfixUpdateExpr &node) override;
    void visit(const CallExpr &node) override;
    void visit(const IndexExpr &node) override;
    void visit(const MemberAccessExpr &node) override;
    void visit(const IntLiteralExpr &node) override;
    void visit(const FloatLiteralExpr &node) override;
    void visit(const StringLiteralExpr &node) override;
    void visit(const CharLiteralExpr &node) override;
    void visit(const BoolLiteralExpr &node) override;
    void visit(const NullLiteralExpr &node) override;
    void visit(const ArrayLiteralExpr &node) override;
    void visit(const ListLiteralExpr &node) override;
    void visit(const AllocExpr &node) override;
    void visit(const IdentifierExpr &node) override;

    // Types
    void visit(const PrimitiveType &node) override;
    void visit(const NamedType &node) override;
    void visit(const ListType &node) override;
    void visit(const PointerType &node) override;
    void visit(const SliceType &node) override;
    void visit(const ArrayType &node) override;

    llvm::Module &module() { return *module_; }
    llvm::IRBuilder<> &builder() { return builder_; }
    llvm::LLVMContext &context() { return ctx_; }

private:
    llvm::Type *to_llvm_type(const Type &type);
    llvm::Type *to_llvm_type(PrimitiveKind kind);

    void push_scope();
    void pop_scope();
    bool declare_symbol(const std::string &name, llvm::AllocaInst *inst, llvm::Type *type);
    const LLVMSymbol *lookup_symbol(const std::string &name) const;

    void emit_runtime_decls();

    std::ostream &out_;
    llvm::LLVMContext ctx_;
    std::unique_ptr<llvm::Module> module_;
    llvm::IRBuilder<> builder_;
    llvm::Function *current_func_{nullptr};
    llvm::Value *last_val_{nullptr};
    std::vector<std::unordered_map<std::string, LLVMSymbol>> scopes_;
};
