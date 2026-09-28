#include "codegen/asm_codegen.h"

AsmCodegen::AsmCodegen(std::ostream &out) : out_(out) {}

void AsmCodegen::generate(const Program &program) {
    program.accept(*this);
}

std::string AsmCodegen::new_label(const std::string &prefix) {
    return "." + prefix + "_" + std::to_string(label_seq_++);
}

void AsmCodegen::emit_prologue(int stack_size) {
    (void)stack_size;
}

void AsmCodegen::emit_epilogue() {
}

// Declarations
void AsmCodegen::visit(const Program &node) {
    for (const auto &decl : node.decls()) {
        decl->accept(*this);
    }
}

void AsmCodegen::visit(const IncludeDirective &node) {
    (void)node;
}

void AsmCodegen::visit(const StructField &node) {
    (void)node;
}

void AsmCodegen::visit(const StructDecl &node) {
    (void)node;
}

void AsmCodegen::visit(const Param &node) {
    (void)node;
}

void AsmCodegen::visit(const FunctionDecl &node) {
    (void)node;
}

// Statements
void AsmCodegen::visit(const BlockStmt &node) {
    for (const auto &stmt : node.stmts()) {
        stmt->accept(*this);
    }
}

void AsmCodegen::visit(const VarDeclStmt &node) {
    (void)node;
}

void AsmCodegen::visit(const AssignStmt &node) {
    (void)node;
}

void AsmCodegen::visit(const ExprStmt &node) {
    (void)node;
}

void AsmCodegen::visit(const IfStmt &node) {
    (void)node;
}

void AsmCodegen::visit(const WhileStmt &node) {
    (void)node;
}

void AsmCodegen::visit(const ForStmt &node) {
    (void)node;
}

void AsmCodegen::visit(const ReturnStmt &node) {
    (void)node;
}

void AsmCodegen::visit(const DeferStmt &node) {
    (void)node;
}

// Expressions
void AsmCodegen::visit(const BinaryExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const UnaryExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const SizeofExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const PostfixUpdateExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const CallExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const IndexExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const MemberAccessExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const IntLiteralExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const FloatLiteralExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const StringLiteralExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const CharLiteralExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const BoolLiteralExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const NullLiteralExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const ArrayLiteralExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const ListLiteralExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const AllocExpr &node) {
    (void)node;
}

void AsmCodegen::visit(const IdentifierExpr &node) {
    (void)node;
}

// Types
void AsmCodegen::visit(const PrimitiveType &node) {
    (void)node;
}

void AsmCodegen::visit(const NamedType &node) {
    (void)node;
}

void AsmCodegen::visit(const ListType &node) {
    (void)node;
}

void AsmCodegen::visit(const PointerType &node) {
    (void)node;
}

void AsmCodegen::visit(const SliceType &node) {
    (void)node;
}

void AsmCodegen::visit(const ArrayType &node) {
    (void)node;
}
