#pragma once

class Program;
class IncludeDirective;
class StructField;
class StructDecl;
class Param;
class FunctionDecl;

class BlockStmt;
class VarDeclStmt;
class AssignStmt;
class ExprStmt;
class IfStmt;
class WhileStmt;
class ForStmt;
class ReturnStmt;
class DeferStmt;

class BinaryExpr;
class UnaryExpr;
class SizeofExpr;
class PostfixUpdateExpr;
class CallExpr;
class IndexExpr;
class MemberAccessExpr;
class IntLiteralExpr;
class FloatLiteralExpr;
class StringLiteralExpr;
class CharLiteralExpr;
class BoolLiteralExpr;
class NullLiteralExpr;
class ArrayLiteralExpr;
class ListLiteralExpr;
class AllocExpr;
class IdentifierExpr;

class PrimitiveType;
class NamedType;
class ListType;
class PointerType;
class SliceType;
class ArrayType;

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    virtual void visit(const Program &node) = 0;
    virtual void visit(const IncludeDirective &node) = 0;
    virtual void visit(const StructField &node) = 0;
    virtual void visit(const StructDecl &node) = 0;
    virtual void visit(const Param &node) = 0;
    virtual void visit(const FunctionDecl &node) = 0;

    virtual void visit(const BlockStmt &node) = 0;
    virtual void visit(const VarDeclStmt &node) = 0;
    virtual void visit(const AssignStmt &node) = 0;
    virtual void visit(const ExprStmt &node) = 0;
    virtual void visit(const IfStmt &node) = 0;
    virtual void visit(const WhileStmt &node) = 0;
    virtual void visit(const ForStmt &node) = 0;
    virtual void visit(const ReturnStmt &node) = 0;
    virtual void visit(const DeferStmt &node) = 0;

    virtual void visit(const BinaryExpr &node) = 0;
    virtual void visit(const UnaryExpr &node) = 0;
    virtual void visit(const SizeofExpr &node) = 0;
    virtual void visit(const PostfixUpdateExpr &node) = 0;
    virtual void visit(const CallExpr &node) = 0;
    virtual void visit(const IndexExpr &node) = 0;
    virtual void visit(const MemberAccessExpr &node) = 0;
    virtual void visit(const IntLiteralExpr &node) = 0;
    virtual void visit(const FloatLiteralExpr &node) = 0;
    virtual void visit(const StringLiteralExpr &node) = 0;
    virtual void visit(const CharLiteralExpr &node) = 0;
    virtual void visit(const BoolLiteralExpr &node) = 0;
    virtual void visit(const NullLiteralExpr &node) = 0;
    virtual void visit(const ArrayLiteralExpr &node) = 0;
    virtual void visit(const ListLiteralExpr &node) = 0;
    virtual void visit(const AllocExpr &node) = 0;
    virtual void visit(const IdentifierExpr &node) = 0;

    virtual void visit(const PrimitiveType &node) = 0;
    virtual void visit(const NamedType &node) = 0;
    virtual void visit(const ListType &node) = 0;
    virtual void visit(const PointerType &node) = 0;
    virtual void visit(const SliceType &node) = 0;
    virtual void visit(const ArrayType &node) = 0;
};
