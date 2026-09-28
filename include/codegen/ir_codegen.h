#pragma once

#include "ast.h"
#include "ast_visitor.h"
#include <ostream>
#include <string>

class IRCodegen final : public ASTVisitor {
public:
    explicit IRCodegen(std::ostream& out);

    void generate(const Program& program);

    // Declarations
    void visit(const Program& node) override;
    void visit(const IncludeDirective& node) override;
    void visit(const StructField& node) override;
    void visit(const StructDecl& node) override;
    void visit(const Param& node) override;
    void visit(const FunctionDecl& node) override;

    // Statements
    void visit(const BlockStmt& node) override;
    void visit(const VarDeclStmt& node) override;
    void visit(const AssignStmt& node) override;
    void visit(const ExprStmt& node) override;
    void visit(const IfStmt& node) override;
    void visit(const WhileStmt& node) override;
    void visit(const ForStmt& node) override;
    void visit(const ReturnStmt& node) override;
    void visit(const DeferStmt& node) override;

    // Expressions
    void visit(const BinaryExpr& node) override;
    void visit(const UnaryExpr& node) override;
    void visit(const SizeofExpr& node) override;
    void visit(const PostfixUpdateExpr& node) override;
    void visit(const CallExpr& node) override;
    void visit(const IndexExpr& node) override;
    void visit(const MemberAccessExpr& node) override;
    void visit(const IntLiteralExpr& node) override;
    void visit(const FloatLiteralExpr& node) override;
    void visit(const StringLiteralExpr& node) override;
    void visit(const CharLiteralExpr& node) override;
    void visit(const BoolLiteralExpr& node) override;
    void visit(const NullLiteralExpr& node) override;
    void visit(const ArrayLiteralExpr& node) override;
    void visit(const ListLiteralExpr& node) override;
    void visit(const AllocExpr& node) override;
    void visit(const IdentifierExpr& node) override;

    // Types
    void visit(const PrimitiveType& node) override;
    void visit(const NamedType& node) override;
    void visit(const ListType& node) override;
    void visit(const PointerType& node) override;
    void visit(const SliceType& node) override;
    void visit(const ArrayType& node) override;

private:
    std::string new_temp();
    std::string new_label(const std::string& prefix = "bb");

    std::ostream& out_;
    size_t temp_seq_ { 0 };
    size_t label_seq_ { 0 };
};
