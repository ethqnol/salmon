#include "codegen/ir_codegen.h"

IRCodegen::IRCodegen(std::ostream &out) : out_(out) {}

void IRCodegen::generate(const Program &program) {
    program.accept(*this);
}

std::string IRCodegen::new_temp() {
    return "%t" + std::to_string(temp_seq_++);
}

std::string IRCodegen::new_label(const std::string &prefix) {
    return prefix + "_" + std::to_string(label_seq_++);
}

// Declarations
void IRCodegen::visit(const Program &node) {
    for (const auto &decl : node.decls()) {
        decl->accept(*this);
    }
}

void IRCodegen::visit(const IncludeDirective &node) {
    (void)node;
}

void IRCodegen::visit(const StructField &node) {
    (void)node;
}

void IRCodegen::visit(const StructDecl &node) {
    (void)node;
}

void IRCodegen::visit(const Param &node) {
    (void)node;
}

void IRCodegen::visit(const FunctionDecl &node) {
    (void)node;
}

// Statements
void IRCodegen::visit(const BlockStmt &node) {
    for (const auto &stmt : node.stmts()) {
        stmt->accept(*this);
    }
}

void IRCodegen::visit(const VarDeclStmt &node) {
    (void)node;
}

void IRCodegen::visit(const AssignStmt &node) {
    (void)node;
}

void IRCodegen::visit(const ExprStmt &node) {
    (void)node;
}

void IRCodegen::visit(const IfStmt &node) {
    (void)node;
}

void IRCodegen::visit(const WhileStmt &node) {
    (void)node;
}

void IRCodegen::visit(const ForStmt &node) {
    (void)node;
}

void IRCodegen::visit(const ReturnStmt &node) {
    (void)node;
}

void IRCodegen::visit(const DeferStmt &node) {
    (void)node;
}

// Expressions
void IRCodegen::visit(const BinaryExpr &node) {
    (void)node;
}

void IRCodegen::visit(const UnaryExpr &node) {
    (void)node;
}

void IRCodegen::visit(const SizeofExpr &node) {
    (void)node;
}

void IRCodegen::visit(const PostfixUpdateExpr &node) {
    (void)node;
}

void IRCodegen::visit(const CallExpr &node) {
    (void)node;
}

void IRCodegen::visit(const IndexExpr &node) {
    (void)node;
}

void IRCodegen::visit(const MemberAccessExpr &node) {
    (void)node;
}

void IRCodegen::visit(const IntLiteralExpr &node) {
    (void)node;
}

void IRCodegen::visit(const FloatLiteralExpr &node) {
    (void)node;
}

void IRCodegen::visit(const StringLiteralExpr &node) {
    (void)node;
}

void IRCodegen::visit(const CharLiteralExpr &node) {
    (void)node;
}

void IRCodegen::visit(const BoolLiteralExpr &node) {
    (void)node;
}

void IRCodegen::visit(const NullLiteralExpr &node) {
    (void)node;
}

void IRCodegen::visit(const ArrayLiteralExpr &node) {
    (void)node;
}

void IRCodegen::visit(const ListLiteralExpr &node) {
    (void)node;
}

void IRCodegen::visit(const AllocExpr &node) {
    (void)node;
}

void IRCodegen::visit(const IdentifierExpr &node) {
    (void)node;
}

// Types
void IRCodegen::visit(const PrimitiveType &node) {
    (void)node;
}

void IRCodegen::visit(const NamedType &node) {
    (void)node;
}

void IRCodegen::visit(const ListType &node) {
    (void)node;
}

void IRCodegen::visit(const PointerType &node) {
    (void)node;
}

void IRCodegen::visit(const SliceType &node) {
    (void)node;
}

void IRCodegen::visit(const ArrayType &node) {
    (void)node;
}
