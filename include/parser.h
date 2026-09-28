#pragma once

#include "ast.h"
#include "token.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class ParseError final : public std::runtime_error {
public:
    ParseError(size_t line, size_t col, const std::string &message)
        : std::runtime_error("[" + std::to_string(line) + ":" + std::to_string(col) + "] Parse Error: " + message),
          line_(line), col_(col) {}

    size_t line() const { return line_; }
    size_t col() const { return col_; }

private:
    size_t line_;
    size_t col_;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    std::unique_ptr<Program> parse_program();

private:
    // Declarations
    std::unique_ptr<Decl> parse_decl();
    std::unique_ptr<IncludeDirective> parse_include();
    std::unique_ptr<StructDecl> parse_struct();
    std::unique_ptr<FunctionDecl> parse_func();

    // Types
    std::unique_ptr<Type> parse_type();
    std::unique_ptr<Type> parse_base_type();
    bool is_type_start() const;
    bool is_var_decl() const;

    // Statements
    std::unique_ptr<Stmt> parse_stmt();
    std::unique_ptr<BlockStmt> parse_block();
    std::unique_ptr<VarDeclStmt> parse_var_decl();
    std::unique_ptr<Stmt> parse_assign_or_expr();
    std::unique_ptr<IfStmt> parse_if();
    std::unique_ptr<WhileStmt> parse_while();
    std::unique_ptr<ForStmt> parse_for();
    std::unique_ptr<ReturnStmt> parse_return();
    std::unique_ptr<DeferStmt> parse_defer();

    // Expressions
    std::unique_ptr<Expr> parse_expr();
    std::unique_ptr<Expr> parse_logical_or();
    std::unique_ptr<Expr> parse_logical_and();
    std::unique_ptr<Expr> parse_equality();
    std::unique_ptr<Expr> parse_relational();
    std::unique_ptr<Expr> parse_additive();
    std::unique_ptr<Expr> parse_multiplicative();
    std::unique_ptr<Expr> parse_unary();
    std::unique_ptr<Expr> parse_postfix();
    std::unique_ptr<Expr> parse_primary();
    std::unique_ptr<AllocExpr> parse_alloc();
    std::unique_ptr<ArrayLiteralExpr> parse_array_lit();
    std::unique_ptr<ListLiteralExpr> parse_list_lit();

    // Helpers
    const Token &peek() const;
    const Token &peek_ahead(size_t dist) const;
    const Token &previous() const;
    const Token &advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool is_at_end() const;
    const Token &consume(TokenType type, const std::string &message);
    [[noreturn]] void error(const Token &token, const std::string &message) const;

    std::vector<Token> tokens_;
    size_t current_{0};
};
