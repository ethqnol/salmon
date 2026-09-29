#pragma once

#include "ast.h"
#include "token.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class ParseError final : public std::runtime_error {
public:
    ParseError(SourceLoc loc, const std::string &message)
        : std::runtime_error(message),
          loc_(std::move(loc)) {}

    ParseError(size_t line, size_t col, const std::string &message)
        : ParseError(SourceLoc{"<stdin>", line, col, 1}, message) {}

    const SourceLoc &loc() const { return loc_; }
    size_t line() const { return loc_.line; }
    size_t col() const { return loc_.col; }

private:
    SourceLoc loc_;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens, std::string filename = "<stdin>");

    std::unique_ptr<Program> parse_program();

private:
    std::unique_ptr<Decl> parse_decl();
    std::unique_ptr<IncludeDirective> parse_include();
    std::unique_ptr<StructDecl> parse_struct();
    std::unique_ptr<FunctionDecl> parse_func();

    std::unique_ptr<Type> parse_type();
    std::unique_ptr<Type> parse_base_type();
    bool is_type_start() const;
    bool is_var_decl() const;

    std::unique_ptr<Stmt> parse_stmt();
    std::unique_ptr<BlockStmt> parse_block();
    std::unique_ptr<VarDeclStmt> parse_var_decl();
    std::unique_ptr<Stmt> parse_assign_or_expr();
    std::unique_ptr<IfStmt> parse_if();
    std::unique_ptr<WhileStmt> parse_while();
    std::unique_ptr<ForStmt> parse_for();
    std::unique_ptr<ReturnStmt> parse_return();
    std::unique_ptr<DeferStmt> parse_defer();

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

    const Token &peek() const;
    const Token &peek_ahead(size_t dist) const;
    const Token &previous() const;
    const Token &advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool is_at_end() const;
    const Token &consume(TokenType type, const std::string &message);
    SourceLoc loc_for(const Token &token) const {
        return SourceLoc{filename_, token.line, token.col, token.lexeme.size()};
    }
    [[noreturn]] void error(const Token &token, const std::string &message) const;

    std::vector<Token> tokens_;
    size_t current_{0};
    std::string filename_{"<stdin>"};
};
