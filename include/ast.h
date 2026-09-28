#pragma once

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

// Forward declarations
class ASTVisitor;
class ASTNode;
class Type;
class Decl;
class Stmt;
class Expr;
class BlockStmt;

enum class PrimitiveKind : uint8_t {
    Int,
    I8,
    I16,
    I32,
    I64,
    UInt,
    U8,
    U16,
    U32,
    U64,
    Float,
    F32,
    F64,
    Bool,
    Char,
    Void
};

std::string_view primitive_kind_str(PrimitiveKind kind);

enum class AssignOp : uint8_t {
    Assign,    // =
    AddAssign, // +=
    SubAssign, // -=
    MulAssign, // *=
    DivAssign  // /=
};

std::string_view assign_op_str(AssignOp op);

enum class BinaryOp : uint8_t {
    LogicalOr,
    LogicalAnd,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Add,
    Sub,
    Mul,
    Div,
    Mod
};

std::string_view binary_op_str(BinaryOp op);

enum class UnaryOp : uint8_t {
    AddressOf,   // &
    Dereference, // *
    LogicalNot,  // !
    Negate       // -
};

std::string_view unary_op_str(UnaryOp op);

enum class PostfixOp : uint8_t {
    PostIncrement, // ++
    PostDecrement  // --
};

std::string_view postfix_op_str(PostfixOp op);

// Base AST Node
class ASTNode {
public:
    virtual ~ASTNode() = default;

    virtual void accept(ASTVisitor &visitor) const = 0;

    virtual void emit_asdl(std::ostream &out, int indent = 0) const = 0;
    std::string to_asdl(int indent = 0) const;

    virtual void pretty_print(std::ostream &out, int indent = 0) const = 0;
    std::string to_pretty(int indent = 0) const;

    virtual void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const;
    std::string to_tree() const;
};

class Type : public ASTNode {
public:
    virtual ~Type() = default;
};

class PrimitiveType final : public Type {
public:
    explicit PrimitiveType(PrimitiveKind kind) : kind_(kind) {}

    PrimitiveKind kind() const { return kind_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;

private:
    PrimitiveKind kind_;
};

class NamedType final : public Type {
public:
    explicit NamedType(std::string name) : name_(std::move(name)) {}

    const std::string &name() const { return name_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;

private:
    std::string name_;
};

class ListType final : public Type {
public:
    explicit ListType(std::unique_ptr<Type> elem_type) : elem_type_(std::move(elem_type)) {}

    const Type &elem_type() const { return *elem_type_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;

private:
    std::unique_ptr<Type> elem_type_;
};

class PointerType final : public Type {
public:
    explicit PointerType(std::unique_ptr<Type> base_type) : base_type_(std::move(base_type)) {}

    const Type &base_type() const { return *base_type_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;

private:
    std::unique_ptr<Type> base_type_;
};

class SliceType final : public Type {
public:
    explicit SliceType(std::unique_ptr<Type> elem_type) : elem_type_(std::move(elem_type)) {}

    const Type &elem_type() const { return *elem_type_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;

private:
    std::unique_ptr<Type> elem_type_;
};

class ArrayType final : public Type {
public:
    ArrayType(std::unique_ptr<Type> elem_type, int64_t size)
        : elem_type_(std::move(elem_type)), size_(size) {}

    const Type &elem_type() const { return *elem_type_; }
    int64_t size() const { return size_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;

private:
    std::unique_ptr<Type> elem_type_;
    int64_t size_{0};
};

class Decl : public ASTNode {
public:
    virtual ~Decl() = default;
};

class IncludeDirective final : public Decl {
public:
    explicit IncludeDirective(std::string path) : path_(std::move(path)) {}

    const std::string &path() const { return path_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::string path_;
};

class StructField final : public ASTNode {
public:
    StructField(std::unique_ptr<Type> type, std::string name)
        : type_(std::move(type)), name_(std::move(name)) {}

    const Type &type() const { return *type_; }
    const std::string &name() const { return name_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::unique_ptr<Type> type_;
    std::string name_;
};

class StructDecl final : public Decl {
public:
    StructDecl(std::string name, std::vector<std::unique_ptr<StructField>> fields)
        : name_(std::move(name)), fields_(std::move(fields)) {}

    const std::string &name() const { return name_; }
    const std::vector<std::unique_ptr<StructField>> &fields() const { return fields_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::string name_;
    std::vector<std::unique_ptr<StructField>> fields_;
};

class Param final : public ASTNode {
public:
    Param(std::unique_ptr<Type> type, std::string name)
        : type_(std::move(type)), name_(std::move(name)) {}

    const Type &type() const { return *type_; }
    const std::string &name() const { return name_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::unique_ptr<Type> type_;
    std::string name_;
};

class FunctionDecl final : public Decl {
public:
    FunctionDecl(std::string name,
                 std::vector<std::unique_ptr<Param>> params,
                 std::unique_ptr<Type> ret_type,
                 std::unique_ptr<BlockStmt> body)
        : name_(std::move(name)), params_(std::move(params)), ret_type_(std::move(ret_type)), body_(std::move(body)) {}

    const std::string &name() const { return name_; }
    const std::vector<std::unique_ptr<Param>> &params() const { return params_; }
    const Type *ret_type() const { return ret_type_.get(); }
    const BlockStmt &body() const { return *body_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::string name_;
    std::vector<std::unique_ptr<Param>> params_;
    std::unique_ptr<Type> ret_type_;
    std::unique_ptr<BlockStmt> body_;
};

class Stmt : public ASTNode {
public:
    virtual ~Stmt() = default;
};

class BlockStmt final : public Stmt {
public:
    explicit BlockStmt(std::vector<std::unique_ptr<Stmt>> stmts) : stmts_(std::move(stmts)) {}

    const std::vector<std::unique_ptr<Stmt>> &stmts() const { return stmts_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::vector<std::unique_ptr<Stmt>> stmts_;
};

class VarDeclStmt final : public Stmt {
public:
    VarDeclStmt(std::unique_ptr<Type> type, std::string name, std::unique_ptr<Expr> init)
        : type_(std::move(type)), name_(std::move(name)), init_(std::move(init)) {}

    const Type &type() const { return *type_; }
    const std::string &name() const { return name_; }
    const Expr *init() const { return init_.get(); }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::unique_ptr<Type> type_;
    std::string name_;
    std::unique_ptr<Expr> init_;
};

class AssignStmt final : public Stmt {
public:
    AssignStmt(std::unique_ptr<Expr> target, AssignOp op, std::unique_ptr<Expr> value)
        : target_(std::move(target)), op_(op), value_(std::move(value)) {}

    const Expr &target() const { return *target_; }
    AssignOp op() const { return op_; }
    const Expr &value() const { return *value_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::unique_ptr<Expr> target_;
    AssignOp op_;
    std::unique_ptr<Expr> value_;
};

class ExprStmt final : public Stmt {
public:
    explicit ExprStmt(std::unique_ptr<Expr> expr) : expr_(std::move(expr)) {}

    const Expr &expr() const { return *expr_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::unique_ptr<Expr> expr_;
};

class IfStmt final : public Stmt {
public:
    IfStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> then_branch, std::unique_ptr<Stmt> else_branch)
        : cond_(std::move(cond)), then_branch_(std::move(then_branch)), else_branch_(std::move(else_branch)) {}

    const Expr &cond() const { return *cond_; }
    const Stmt &then_branch() const { return *then_branch_; }
    const Stmt *else_branch() const { return else_branch_.get(); }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::unique_ptr<Expr> cond_;
    std::unique_ptr<Stmt> then_branch_;
    std::unique_ptr<Stmt> else_branch_;
};

class WhileStmt final : public Stmt {
public:
    WhileStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> body)
        : cond_(std::move(cond)), body_(std::move(body)) {}

    const Expr &cond() const { return *cond_; }
    const Stmt &body() const { return *body_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::unique_ptr<Expr> cond_;
    std::unique_ptr<Stmt> body_;
};

class ForStmt final : public Stmt {
public:
    ForStmt(std::unique_ptr<Stmt> init,
            std::unique_ptr<Expr> cond,
            std::unique_ptr<Stmt> update,
            std::unique_ptr<Stmt> body)
        : init_(std::move(init)), cond_(std::move(cond)), update_(std::move(update)), body_(std::move(body)) {}

    const Stmt *init() const { return init_.get(); }
    const Expr *cond() const { return cond_.get(); }
    const Stmt *update() const { return update_.get(); }
    const Stmt &body() const { return *body_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::unique_ptr<Stmt> init_;
    std::unique_ptr<Expr> cond_;
    std::unique_ptr<Stmt> update_;
    std::unique_ptr<Stmt> body_;
};

class ReturnStmt final : public Stmt {
public:
    explicit ReturnStmt(std::unique_ptr<Expr> value) : value_(std::move(value)) {}

    const Expr *value() const { return value_.get(); }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::unique_ptr<Expr> value_;
};

class DeferStmt final : public Stmt {
public:
    explicit DeferStmt(std::unique_ptr<Stmt> stmt) : stmt_(std::move(stmt)) {}

    const Stmt &stmt() const { return *stmt_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::unique_ptr<Stmt> stmt_;
};

class Expr : public ASTNode {
public:
    virtual ~Expr() = default;
};

class BinaryExpr final : public Expr {
public:
    BinaryExpr(BinaryOp op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right)
        : op_(op), left_(std::move(left)), right_(std::move(right)) {}

    BinaryOp op() const { return op_; }
    const Expr &left() const { return *left_; }
    const Expr &right() const { return *right_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    BinaryOp op_;
    std::unique_ptr<Expr> left_;
    std::unique_ptr<Expr> right_;
};

class UnaryExpr final : public Expr {
public:
    UnaryExpr(UnaryOp op, std::unique_ptr<Expr> operand)
        : op_(op), operand_(std::move(operand)) {}

    UnaryOp op() const { return op_; }
    const Expr &operand() const { return *operand_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    UnaryOp op_;
    std::unique_ptr<Expr> operand_;
};

class SizeofExpr final : public Expr {
public:
    explicit SizeofExpr(std::unique_ptr<Expr> operand) : operand_(std::move(operand)) {}

    const Expr &operand() const { return *operand_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::unique_ptr<Expr> operand_;
};

class PostfixUpdateExpr final : public Expr {
public:
    PostfixUpdateExpr(PostfixOp op, std::unique_ptr<Expr> operand)
        : op_(op), operand_(std::move(operand)) {}

    PostfixOp op() const { return op_; }
    const Expr &operand() const { return *operand_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    PostfixOp op_;
    std::unique_ptr<Expr> operand_;
};

class CallExpr final : public Expr {
public:
    CallExpr(std::unique_ptr<Expr> callee, std::vector<std::unique_ptr<Expr>> args)
        : callee_(std::move(callee)), args_(std::move(args)) {}

    const Expr &callee() const { return *callee_; }
    const std::vector<std::unique_ptr<Expr>> &args() const { return args_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::unique_ptr<Expr> callee_;
    std::vector<std::unique_ptr<Expr>> args_;
};

class IndexExpr final : public Expr {
public:
    IndexExpr(std::unique_ptr<Expr> object, std::unique_ptr<Expr> index)
        : object_(std::move(object)), index_(std::move(index)) {}

    const Expr &object() const { return *object_; }
    const Expr &index() const { return *index_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::unique_ptr<Expr> object_;
    std::unique_ptr<Expr> index_;
};

// Unified member access for both dot (.) and arrow (->)
class MemberAccessExpr final : public Expr {
public:
    MemberAccessExpr(std::unique_ptr<Expr> object, std::string member)
        : object_(std::move(object)), member_(std::move(member)) {}

    const Expr &object() const { return *object_; }
    const std::string &member() const { return member_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::unique_ptr<Expr> object_;
    std::string member_;
};

class IntLiteralExpr final : public Expr {
public:
    explicit IntLiteralExpr(int64_t value) : value_(value) {}

    int64_t value() const { return value_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    int64_t value_{0};
};

class FloatLiteralExpr final : public Expr {
public:
    explicit FloatLiteralExpr(double value) : value_(value) {}

    double value() const { return value_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    double value_{0.0};
};

class StringLiteralExpr final : public Expr {
public:
    explicit StringLiteralExpr(std::string value) : value_(std::move(value)) {}

    const std::string &value() const { return value_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::string value_;
};

class CharLiteralExpr final : public Expr {
public:
    explicit CharLiteralExpr(char value) : value_(value) {}

    char value() const { return value_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    char value_{'\0'};
};

class BoolLiteralExpr final : public Expr {
public:
    explicit BoolLiteralExpr(bool value) : value_(value) {}

    bool value() const { return value_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    bool value_{false};
};

class NullLiteralExpr final : public Expr {
public:
    NullLiteralExpr() = default;
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;
};

class ArrayLiteralExpr final : public Expr {
public:
    explicit ArrayLiteralExpr(std::vector<std::unique_ptr<Expr>> elements) : elements_(std::move(elements)) {}

    const std::vector<std::unique_ptr<Expr>> &elements() const { return elements_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::vector<std::unique_ptr<Expr>> elements_;
};

class ListLiteralExpr final : public Expr {
public:
    explicit ListLiteralExpr(std::vector<std::unique_ptr<Expr>> elements) : elements_(std::move(elements)) {}

    const std::vector<std::unique_ptr<Expr>> &elements() const { return elements_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::vector<std::unique_ptr<Expr>> elements_;
};

class AllocExpr final : public Expr {
public:
    AllocExpr(std::unique_ptr<Type> alloc_type, std::unique_ptr<Expr> count)
        : alloc_type_(std::move(alloc_type)), count_(std::move(count)) {}

    const Type &alloc_type() const { return *alloc_type_; }
    const Expr *count() const { return count_.get(); }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::unique_ptr<Type> alloc_type_;
    std::unique_ptr<Expr> count_;
};

class IdentifierExpr final : public Expr {
public:
    explicit IdentifierExpr(std::string name) : name_(std::move(name)) {}

    const std::string &name() const { return name_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::string name_;
};

class Program final : public ASTNode {
public:
    explicit Program(std::vector<std::unique_ptr<Decl>> decls) : decls_(std::move(decls)) {}

    const std::vector<std::unique_ptr<Decl>> &decls() const { return decls_; }
    void accept(ASTVisitor &visitor) const override;
    void emit_asdl(std::ostream &out, int indent = 0) const override;
    void pretty_print(std::ostream &out, int indent = 0) const override;
    void print_tree(std::ostream &out, const std::string &prefix = "", bool is_last = true) const override;

private:
    std::vector<std::unique_ptr<Decl>> decls_;
};
