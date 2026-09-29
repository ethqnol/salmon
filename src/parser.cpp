#include "parser.h"
#include "diagnostic.h"
#include "lexer.h"
#include <filesystem>
#include <fstream>
#include <sstream>

Parser::Parser(std::vector<Token> tokens, std::string filename,
               SourceManager *source_mgr,
               std::unordered_set<std::string> *included_files)
    : tokens_(std::move(tokens)),
      filename_(std::move(filename)),
      source_mgr_(source_mgr),
      included_files_(included_files ? included_files : &default_included_files_) {
}

const Token &Parser::peek() const {
    return tokens_[current_];
}

const Token &Parser::peek_ahead(size_t dist) const {
    if (current_ + dist >= tokens_.size()) {
        return tokens_.back();
    }
    return tokens_[current_ + dist];
}

const Token &Parser::previous() const {
    return tokens_[current_ - 1];
}

const Token &Parser::advance() {
    if (!is_at_end()) {
        current_++;
    }
    return previous();
}

bool Parser::check(TokenType type) const {
    if (is_at_end()) {
        return type == TokenType::EndOfFile;
    }
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::is_at_end() const {
    return peek().type == TokenType::EndOfFile;
}

const Token &Parser::consume(TokenType type, const std::string &message) {
    if (check(type)) {
        return advance();
    }
    error(peek(), message);
}

void Parser::error(const Token &token, const std::string &message,
                   const std::vector<std::string> &notes,
                   const std::vector<std::string> &suggestions) const {
    size_t len = token.lexeme.empty() ? 1 : token.lexeme.size();
    SourceLoc loc{filename_, token.line, token.col, len};
    std::string full_msg = message;
    if (token.type != TokenType::EndOfFile && !token.lexeme.empty() &&
        message.find("cannot find") == std::string::npos &&
        message.find("unknown standard library module") == std::string::npos) {
        full_msg += " (got '" + token.lexeme + "')";
    }
    throw ParseError(loc, full_msg, notes, suggestions);
}

void Parser::parse_include_and_merge(std::vector<std::unique_ptr<Decl>> &decls) {
    consume(TokenType::Include, "Expected 'include'");
    const auto &path_tok = consume(TokenType::StringLiteral, "Expected string literal after 'include'");
    std::string raw_path = path_tok.lexeme;

    static const std::unordered_set<std::string> known_std_modules = {
        "std/io",
        "std/mem",
        "std/math",
        "std/list"
    };

    std::vector<std::filesystem::path> candidates;
    std::vector<std::string> to_try = {raw_path};
    if (raw_path.size() < 4 || raw_path.substr(raw_path.size() - 4) != ".sal") {
        to_try.push_back(raw_path + ".sal");
    }

    for (const auto &p_str : to_try) {
        std::filesystem::path p(p_str);
        if (p.is_absolute()) {
            candidates.push_back(p);
        } else {
            if (filename_ != "<stdin>") {
                candidates.push_back(std::filesystem::path(filename_).parent_path() / p);
            }
            candidates.push_back(std::filesystem::current_path() / p);
            std::error_code ec;
            auto exe_path = std::filesystem::canonical("/proc/self/exe", ec);
            if (!ec) {
                candidates.push_back(exe_path.parent_path() / p);
                candidates.push_back(exe_path.parent_path().parent_path() / p);
            }
        }
    }

    std::filesystem::path resolved_path;
    bool found = false;
    for (const auto &c : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(c, ec)) {
            resolved_path = std::filesystem::canonical(c, ec);
            if (!ec) {
                found = true;
                break;
            }
        }
    }

    if (!found) {
        if (raw_path.rfind("std/", 0) == 0) {
            std::vector<std::string> std_candidates(known_std_modules.begin(), known_std_modules.end());
            std::string sim = DiagnosticEngine::find_similar(raw_path, std_candidates);
            std::vector<std::string> suggestions;
            if (!sim.empty()) {
                suggestions.push_back("did you mean '" + sim + "'?");
            }
            error(path_tok, "unknown standard library module '" + raw_path + "'", {}, suggestions);
        }
        error(path_tok, "cannot find include file '" + raw_path + "'");
    }

    std::string can_str = resolved_path.string();
    if (included_files_->find(can_str) != included_files_->end()) {
        return;
    }
    included_files_->insert(can_str);

    std::ifstream file(can_str);
    if (!file.is_open()) {
        error(path_tok, "cannot open include file '" + raw_path + "'");
    }
    std::stringstream buf;
    buf << file.rdbuf();
    std::string code = buf.str();

    if (source_mgr_) {
        source_mgr_->add_source(can_str, code);
    }

    Lexer inc_lexer(code);
    auto inc_tokens = inc_lexer.tokenize();
    for (const auto &tok : inc_tokens) {
        if (tok.type == TokenType::Invalid) {
            SourceLoc loc{can_str, tok.line, tok.col, tok.lexeme.size()};
            throw ParseError(loc, "Lexer error: " + tok.lexeme);
        }
    }

    Parser inc_parser(std::move(inc_tokens), can_str, source_mgr_, included_files_);
    auto inc_prog = inc_parser.parse_program();
    for (auto &d : inc_prog->take_decls()) {
        decls.push_back(std::move(d));
    }
}

std::unique_ptr<Program> Parser::parse_program() {
    std::vector<std::unique_ptr<Decl>> decls;
    while (!is_at_end()) {
        if (check(TokenType::Include)) {
            parse_include_and_merge(decls);
        } else {
            decls.push_back(parse_decl());
        }
    }
    return std::make_unique<Program>(std::move(decls));
}

std::unique_ptr<Decl> Parser::parse_decl() {
    if (check(TokenType::Include)) {
        return parse_include();
    }
    if (check(TokenType::Struct)) {
        return parse_struct();
    }
    if (check(TokenType::Def) || check(TokenType::Extern)) {
        return parse_func();
    }
    error(peek(), "Expected declaration ('include', 'struct', 'def', or 'extern')");
}

std::unique_ptr<IncludeDirective> Parser::parse_include() {
    consume(TokenType::Include, "Expected 'include'");
    const auto &path = consume(TokenType::StringLiteral, "Expected string literal after 'include'");
    auto dir = std::make_unique<IncludeDirective>(path.lexeme);
    dir->set_loc(loc_for(path));
    return dir;
}

std::unique_ptr<StructDecl> Parser::parse_struct() {
    consume(TokenType::Struct, "Expected 'struct'");
    const auto &name = consume(TokenType::Identifier, "Expected struct name");
    consume(TokenType::LeftBrace, "Expected '{' after struct name");

    std::vector<std::unique_ptr<StructField>> fields;
    while (!check(TokenType::RightBrace) && !is_at_end()) {
        auto type = parse_type();
        const auto &field_name = consume(TokenType::Identifier, "Expected field name");
        consume(TokenType::Semicolon, "Expected ';' after struct field");
        fields.push_back(std::make_unique<StructField>(std::move(type), field_name.lexeme));
    }

    consume(TokenType::RightBrace, "Expected '}' after struct fields");
    return std::make_unique<StructDecl>(name.lexeme, std::move(fields));
}

std::unique_ptr<FunctionDecl> Parser::parse_func() {
    bool is_extern = match(TokenType::Extern);
    consume(TokenType::Def, "Expected 'def'");
    const auto &name = consume(TokenType::Identifier, "Expected function name");
    consume(TokenType::LeftParen, "Expected '(' after function name");

    std::vector<std::unique_ptr<Param>> params;
    bool is_vararg = false;
    if (!check(TokenType::RightParen)) {
        do {
            if (match(TokenType::Ellipsis)) {
                is_vararg = true;
                break;
            }
            auto param_type = parse_type();
            const auto &param_name = consume(TokenType::Identifier, "Expected parameter name");
            params.push_back(std::make_unique<Param>(std::move(param_type), param_name.lexeme));
            if (match(TokenType::Comma)) {
                if (match(TokenType::Ellipsis)) {
                    is_vararg = true;
                    break;
                }
            } else {
                break;
            }
        } while (!check(TokenType::RightParen) && !is_at_end());
    }

    consume(TokenType::RightParen, "Expected ')' after parameters");

    std::unique_ptr<Type> ret_type = nullptr;
    if (match(TokenType::Arrow)) {
        ret_type = parse_type();
    }

    if (is_extern) {
        consume(TokenType::Semicolon, "Expected ';' after extern function declaration");
        auto fn = std::make_unique<FunctionDecl>(name.lexeme, std::move(params), std::move(ret_type), nullptr, true, is_vararg);
        fn->set_loc(loc_for(name));
        return fn;
    }

    auto body = parse_block();
    auto fn = std::make_unique<FunctionDecl>(name.lexeme, std::move(params), std::move(ret_type), std::move(body), false, is_vararg);
    fn->set_loc(loc_for(name));
    return fn;
}

bool Parser::is_type_start() const {
    TokenType type = peek().type;
    switch (type) {
    case TokenType::Int:
    case TokenType::I8:
    case TokenType::I16:
    case TokenType::I32:
    case TokenType::I64:
    case TokenType::UInt:
    case TokenType::U8:
    case TokenType::U16:
    case TokenType::U32:
    case TokenType::U64:
    case TokenType::Float:
    case TokenType::F32:
    case TokenType::F64:
    case TokenType::Bool:
    case TokenType::Char:
    case TokenType::String:
    case TokenType::Void:
    case TokenType::List:
    case TokenType::Identifier:
        return true;
    default:
        return false;
    }
}

bool Parser::is_var_decl() const {
    TokenType first = peek().type;
    switch (first) {
    case TokenType::Int:
    case TokenType::I8:
    case TokenType::I16:
    case TokenType::I32:
    case TokenType::I64:
    case TokenType::UInt:
    case TokenType::U8:
    case TokenType::U16:
    case TokenType::U32:
    case TokenType::U64:
    case TokenType::Float:
    case TokenType::F32:
    case TokenType::F64:
    case TokenType::Bool:
    case TokenType::Char:
    case TokenType::String:
    case TokenType::Void:
    case TokenType::List:
        return true;
    case TokenType::Identifier: {
        size_t off = 1;
        while (true) {
            TokenType t = peek_ahead(off).type;
            if (t == TokenType::Star) {
                off++;
            } else if (t == TokenType::LeftBracket) {
                off++;
                if (peek_ahead(off).type == TokenType::IntegerLiteral) {
                    off++;
                }
                if (peek_ahead(off).type == TokenType::RightBracket) {
                    off++;
                } else {
                    return false;
                }
            } else {
                break;
            }
        }
        if (peek_ahead(off).type == TokenType::Identifier) {
            TokenType next_after_ident = peek_ahead(off + 1).type;
            return next_after_ident == TokenType::Equal || next_after_ident == TokenType::Semicolon || next_after_ident == TokenType::Comma;
        }
        return false;
    }
    default:
        return false;
    }
}

std::unique_ptr<Type> Parser::parse_base_type() {
    if (match(TokenType::Int))
        return std::make_unique<PrimitiveType>(PrimitiveKind::Int);
    if (match(TokenType::I8))
        return std::make_unique<PrimitiveType>(PrimitiveKind::I8);
    if (match(TokenType::I16))
        return std::make_unique<PrimitiveType>(PrimitiveKind::I16);
    if (match(TokenType::I32))
        return std::make_unique<PrimitiveType>(PrimitiveKind::I32);
    if (match(TokenType::I64))
        return std::make_unique<PrimitiveType>(PrimitiveKind::I64);
    if (match(TokenType::UInt))
        return std::make_unique<PrimitiveType>(PrimitiveKind::UInt);
    if (match(TokenType::U8))
        return std::make_unique<PrimitiveType>(PrimitiveKind::U8);
    if (match(TokenType::U16))
        return std::make_unique<PrimitiveType>(PrimitiveKind::U16);
    if (match(TokenType::U32))
        return std::make_unique<PrimitiveType>(PrimitiveKind::U32);
    if (match(TokenType::U64))
        return std::make_unique<PrimitiveType>(PrimitiveKind::U64);
    if (match(TokenType::Float))
        return std::make_unique<PrimitiveType>(PrimitiveKind::Float);
    if (match(TokenType::F32))
        return std::make_unique<PrimitiveType>(PrimitiveKind::F32);
    if (match(TokenType::F64))
        return std::make_unique<PrimitiveType>(PrimitiveKind::F64);
    if (match(TokenType::Bool))
        return std::make_unique<PrimitiveType>(PrimitiveKind::Bool);
    if (match(TokenType::Char))
        return std::make_unique<PrimitiveType>(PrimitiveKind::Char);
    if (match(TokenType::String))
        return std::make_unique<PrimitiveType>(PrimitiveKind::String);
    if (match(TokenType::Void))
        return std::make_unique<PrimitiveType>(PrimitiveKind::Void);

    if (match(TokenType::List)) {
        consume(TokenType::Less, "Expected '<' after 'list'");
        auto elem = parse_type();
        consume(TokenType::Greater, "Expected '>' after list element type");
        return std::make_unique<ListType>(std::move(elem));
    }

    if (check(TokenType::Identifier)) {
        const auto &name = advance();
        return std::make_unique<NamedType>(name.lexeme);
    }

    error(peek(), "Expected type name");
}

std::unique_ptr<Type> Parser::parse_type() {
    auto type = parse_base_type();

    while (true) {
        if (match(TokenType::Star)) {
            type = std::make_unique<PointerType>(std::move(type));
        } else if (match(TokenType::LeftBracket)) {
            if (match(TokenType::RightBracket)) {
                type = std::make_unique<SliceType>(std::move(type));
            } else {
                const auto &size_tok = consume(TokenType::IntegerLiteral, "Expected integer literal for array size");
                consume(TokenType::RightBracket, "Expected ']' after array size");
                type = std::make_unique<ArrayType>(std::move(type), std::stoll(size_tok.lexeme));
            }
        } else {
            break;
        }
    }

    return type;
}

std::unique_ptr<BlockStmt> Parser::parse_block() {
    consume(TokenType::LeftBrace, "Expected '{'");
    std::vector<std::unique_ptr<Stmt>> stmts;
    while (!check(TokenType::RightBrace) && !is_at_end()) {
        stmts.push_back(parse_stmt());
    }
    consume(TokenType::RightBrace, "Expected '}'");
    return std::make_unique<BlockStmt>(std::move(stmts));
}

std::unique_ptr<Stmt> Parser::parse_stmt() {
    if (check(TokenType::LeftBrace)) {
        return parse_block();
    }
    if (check(TokenType::If)) {
        return parse_if();
    }
    if (check(TokenType::While)) {
        return parse_while();
    }
    if (check(TokenType::For)) {
        return parse_for();
    }
    if (check(TokenType::Return)) {
        return parse_return();
    }
    if (check(TokenType::Defer)) {
        return parse_defer();
    }
    if (is_var_decl()) {
        return parse_var_decl();
    }
    return parse_assign_or_expr();
}

std::unique_ptr<VarDeclStmt> Parser::parse_var_decl() {
    auto type = parse_type();
    const auto &name = consume(TokenType::Identifier, "Expected variable name");

    std::unique_ptr<Expr> init = nullptr;
    if (match(TokenType::Equal)) {
        init = parse_expr();
    }

    consume(TokenType::Semicolon, "Expected ';' after variable declaration");
    auto stmt = std::make_unique<VarDeclStmt>(std::move(type), name.lexeme, std::move(init));
    stmt->set_loc(loc_for(name));
    return stmt;
}

std::unique_ptr<Stmt> Parser::parse_assign_or_expr() {
    auto target = parse_postfix();

    AssignOp op = AssignOp::Assign;
    bool is_assign = false;

    if (match(TokenType::Equal)) {
        op = AssignOp::Assign;
        is_assign = true;
    } else if (match(TokenType::PlusEqual)) {
        op = AssignOp::AddAssign;
        is_assign = true;
    } else if (match(TokenType::MinusEqual)) {
        op = AssignOp::SubAssign;
        is_assign = true;
    } else if (match(TokenType::StarEqual)) {
        op = AssignOp::MulAssign;
        is_assign = true;
    } else if (match(TokenType::SlashEqual)) {
        op = AssignOp::DivAssign;
        is_assign = true;
    }

    if (is_assign) {
        SourceLoc loc = target->loc();
        auto val = parse_expr();
        consume(TokenType::Semicolon, "Expected ';' after assignment");
        auto stmt = std::make_unique<AssignStmt>(std::move(target), op, std::move(val));
        stmt->set_loc(loc);
        return stmt;
    }

    consume(TokenType::Semicolon, "Expected ';' after expression statement");
    auto stmt = std::make_unique<ExprStmt>(std::move(target));
    stmt->set_loc(stmt->expr().loc());
    return stmt;
}

std::unique_ptr<IfStmt> Parser::parse_if() {
    consume(TokenType::If, "Expected 'if'");
    consume(TokenType::LeftParen, "Expected '(' after 'if'");
    auto cond = parse_expr();
    consume(TokenType::RightParen, "Expected ')' after condition");

    auto then_branch = parse_block();

    std::unique_ptr<Stmt> else_branch = nullptr;
    if (match(TokenType::Else)) {
        if (check(TokenType::If)) {
            else_branch = parse_if();
        } else {
            else_branch = parse_block();
        }
    }

    return std::make_unique<IfStmt>(std::move(cond), std::move(then_branch), std::move(else_branch));
}

std::unique_ptr<WhileStmt> Parser::parse_while() {
    consume(TokenType::While, "Expected 'while'");
    consume(TokenType::LeftParen, "Expected '(' after 'while'");
    auto cond = parse_expr();
    consume(TokenType::RightParen, "Expected ')' after condition");

    auto body = parse_block();
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body));
}

std::unique_ptr<ForStmt> Parser::parse_for() {
    consume(TokenType::For, "Expected 'for'");
    consume(TokenType::LeftParen, "Expected '(' after 'for'");

    std::unique_ptr<Stmt> init = nullptr;
    if (match(TokenType::Semicolon)) {
        init = nullptr;
    } else if (is_var_decl()) {
        init = parse_var_decl();
    } else {
        init = parse_assign_or_expr();
    }

    std::unique_ptr<Expr> cond = nullptr;
    if (!check(TokenType::Semicolon)) {
        cond = parse_expr();
    }
    consume(TokenType::Semicolon, "Expected ';' after loop condition");

    std::unique_ptr<Stmt> update = nullptr;
    if (!check(TokenType::RightParen)) {
        auto target = parse_postfix();
        if (match(TokenType::Equal)) {
            auto val = parse_expr();
            update = std::make_unique<AssignStmt>(std::move(target), AssignOp::Assign, std::move(val));
        } else if (match(TokenType::PlusEqual)) {
            auto val = parse_expr();
            update = std::make_unique<AssignStmt>(std::move(target), AssignOp::AddAssign, std::move(val));
        } else if (match(TokenType::MinusEqual)) {
            auto val = parse_expr();
            update = std::make_unique<AssignStmt>(std::move(target), AssignOp::SubAssign, std::move(val));
        } else if (match(TokenType::StarEqual)) {
            auto val = parse_expr();
            update = std::make_unique<AssignStmt>(std::move(target), AssignOp::MulAssign, std::move(val));
        } else if (match(TokenType::SlashEqual)) {
            auto val = parse_expr();
            update = std::make_unique<AssignStmt>(std::move(target), AssignOp::DivAssign, std::move(val));
        } else {
            update = std::make_unique<ExprStmt>(std::move(target));
        }
    }
    consume(TokenType::RightParen, "Expected ')' after for loop header");

    auto body = parse_block();
    return std::make_unique<ForStmt>(std::move(init), std::move(cond), std::move(update), std::move(body));
}

std::unique_ptr<ReturnStmt> Parser::parse_return() {
    consume(TokenType::Return, "Expected 'return'");
    std::unique_ptr<Expr> val = nullptr;
    if (!check(TokenType::Semicolon)) {
        val = parse_expr();
    }
    consume(TokenType::Semicolon, "Expected ';' after return statement");
    return std::make_unique<ReturnStmt>(std::move(val));
}

std::unique_ptr<DeferStmt> Parser::parse_defer() {
    consume(TokenType::Defer, "Expected 'defer'");
    auto stmt = parse_stmt();
    return std::make_unique<DeferStmt>(std::move(stmt));
}

std::unique_ptr<Expr> Parser::parse_expr() {
    return parse_logical_or();
}

std::unique_ptr<Expr> Parser::parse_logical_or() {
    auto expr = parse_logical_and();
    while (match(TokenType::PipePipe)) {
        auto right = parse_logical_and();
        expr = std::make_unique<BinaryExpr>(BinaryOp::LogicalOr, std::move(expr), std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_logical_and() {
    auto expr = parse_equality();
    while (match(TokenType::AmpersandAmpersand)) {
        auto right = parse_equality();
        expr = std::make_unique<BinaryExpr>(BinaryOp::LogicalAnd, std::move(expr), std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_equality() {
    auto expr = parse_relational();
    while (true) {
        if (match(TokenType::EqualEqual)) {
            auto right = parse_relational();
            expr = std::make_unique<BinaryExpr>(BinaryOp::Equal, std::move(expr), std::move(right));
        } else if (match(TokenType::BangEqual)) {
            auto right = parse_relational();
            expr = std::make_unique<BinaryExpr>(BinaryOp::NotEqual, std::move(expr), std::move(right));
        } else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_relational() {
    auto expr = parse_additive();
    while (true) {
        if (match(TokenType::Less)) {
            auto right = parse_additive();
            expr = std::make_unique<BinaryExpr>(BinaryOp::Less, std::move(expr), std::move(right));
        } else if (match(TokenType::LessEqual)) {
            auto right = parse_additive();
            expr = std::make_unique<BinaryExpr>(BinaryOp::LessEqual, std::move(expr), std::move(right));
        } else if (match(TokenType::Greater)) {
            auto right = parse_additive();
            expr = std::make_unique<BinaryExpr>(BinaryOp::Greater, std::move(expr), std::move(right));
        } else if (match(TokenType::GreaterEqual)) {
            auto right = parse_additive();
            expr = std::make_unique<BinaryExpr>(BinaryOp::GreaterEqual, std::move(expr), std::move(right));
        } else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_additive() {
    auto expr = parse_multiplicative();
    while (true) {
        if (match(TokenType::Plus)) {
            auto right = parse_multiplicative();
            expr = std::make_unique<BinaryExpr>(BinaryOp::Add, std::move(expr), std::move(right));
        } else if (match(TokenType::Minus)) {
            auto right = parse_multiplicative();
            expr = std::make_unique<BinaryExpr>(BinaryOp::Sub, std::move(expr), std::move(right));
        } else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_multiplicative() {
    auto expr = parse_unary();
    while (true) {
        if (match(TokenType::Star)) {
            auto right = parse_unary();
            expr = std::make_unique<BinaryExpr>(BinaryOp::Mul, std::move(expr), std::move(right));
        } else if (match(TokenType::Slash)) {
            auto right = parse_unary();
            expr = std::make_unique<BinaryExpr>(BinaryOp::Div, std::move(expr), std::move(right));
        } else if (match(TokenType::Percent)) {
            auto right = parse_unary();
            expr = std::make_unique<BinaryExpr>(BinaryOp::Mod, std::move(expr), std::move(right));
        } else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_unary() {
    if (match(TokenType::Ampersand)) {
        const auto &tok = previous();
        auto expr = std::make_unique<UnaryExpr>(UnaryOp::AddressOf, parse_unary());
        expr->set_loc(loc_for(tok));
        return expr;
    }
    if (match(TokenType::Star)) {
        const auto &tok = previous();
        auto expr = std::make_unique<UnaryExpr>(UnaryOp::Dereference, parse_unary());
        expr->set_loc(loc_for(tok));
        return expr;
    }
    if (match(TokenType::Bang)) {
        const auto &tok = previous();
        auto expr = std::make_unique<UnaryExpr>(UnaryOp::LogicalNot, parse_unary());
        expr->set_loc(loc_for(tok));
        return expr;
    }
    if (match(TokenType::Minus)) {
        const auto &tok = previous();
        auto expr = std::make_unique<UnaryExpr>(UnaryOp::Negate, parse_unary());
        expr->set_loc(loc_for(tok));
        return expr;
    }
    if (match(TokenType::Sizeof)) {
        const auto &tok = previous();
        if (check(TokenType::LeftParen) && !is_at_end()) {
            TokenType inner = peek_ahead(1).type;
            switch (inner) {
            case TokenType::Int:
            case TokenType::I8:
            case TokenType::I16:
            case TokenType::I32:
            case TokenType::I64:
            case TokenType::UInt:
            case TokenType::U8:
            case TokenType::U16:
            case TokenType::U32:
            case TokenType::U64:
            case TokenType::Float:
            case TokenType::F32:
            case TokenType::F64:
            case TokenType::Bool:
            case TokenType::Char:
            case TokenType::Void: {
                advance();
                const auto &type_tok = advance();
                consume(TokenType::RightParen, "Expected ')' after type in sizeof");
                auto ident = std::make_unique<IdentifierExpr>(type_tok.lexeme);
                ident->set_loc(loc_for(type_tok));
                auto expr = std::make_unique<SizeofExpr>(std::move(ident));
                expr->set_loc(loc_for(tok));
                return expr;
            }
            default:
                break;
            }
        }
        auto expr = std::make_unique<SizeofExpr>(parse_unary());
        expr->set_loc(loc_for(tok));
        return expr;
    }
    return parse_postfix();
}

std::unique_ptr<Expr> Parser::parse_postfix() {
    auto expr = parse_primary();

    while (true) {
        if (match(TokenType::LeftBracket)) {
            SourceLoc loc = expr->loc();
            auto idx = parse_expr();
            consume(TokenType::RightBracket, "Expected ']' after index");
            expr = std::make_unique<IndexExpr>(std::move(expr), std::move(idx));
            expr->set_loc(loc);
        } else if (match(TokenType::LeftParen)) {
            SourceLoc loc = expr->loc();
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenType::RightParen)) {
                do {
                    args.push_back(parse_expr());
                } while (match(TokenType::Comma));
            }
            consume(TokenType::RightParen, "Expected ')' after argument list");
            expr = std::make_unique<CallExpr>(std::move(expr), std::move(args));
            expr->set_loc(loc);
        } else if (match(TokenType::Dot)) {
            const auto &mem = consume(TokenType::Identifier, "Expected identifier after member access");
            expr = std::make_unique<MemberAccessExpr>(std::move(expr), mem.lexeme);
            expr->set_loc(loc_for(mem));
        } else if (match(TokenType::Arrow)) {
            throw ParseError(loc_for(previous()), "use '.' instead of '->' for member access");
        } else if (match(TokenType::PlusPlus)) {
            SourceLoc loc = expr->loc();
            expr = std::make_unique<PostfixUpdateExpr>(PostfixOp::PostIncrement, std::move(expr));
            expr->set_loc(loc);
        } else if (match(TokenType::MinusMinus)) {
            SourceLoc loc = expr->loc();
            expr = std::make_unique<PostfixUpdateExpr>(PostfixOp::PostDecrement, std::move(expr));
            expr->set_loc(loc);
        } else {
            break;
        }
    }

    return expr;
}

std::unique_ptr<AllocExpr> Parser::parse_alloc() {
    const auto &start_tok = consume(TokenType::Alloc, "Expected 'alloc'");
    consume(TokenType::Less, "Expected '<' after 'alloc'");
    auto type = parse_type();
    consume(TokenType::Greater, "Expected '>' after type in alloc");

    std::unique_ptr<Expr> count = nullptr;
    if (match(TokenType::LeftParen)) {
        if (!check(TokenType::RightParen)) {
            count = parse_expr();
        }
        consume(TokenType::RightParen, "Expected ')' after alloc argument");
    }

    auto expr = std::make_unique<AllocExpr>(std::move(type), std::move(count));
    expr->set_loc(loc_for(start_tok));
    return expr;
}

std::unique_ptr<ArrayLiteralExpr> Parser::parse_array_lit() {
    const auto &start_tok = consume(TokenType::LeftBracket, "Expected '['");
    std::vector<std::unique_ptr<Expr>> elements;
    if (!check(TokenType::RightBracket)) {
        do {
            elements.push_back(parse_expr());
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RightBracket, "Expected ']' after array literal");
    auto expr = std::make_unique<ArrayLiteralExpr>(std::move(elements));
    expr->set_loc(loc_for(start_tok));
    return expr;
}

std::unique_ptr<ListLiteralExpr> Parser::parse_list_lit() {
    const auto &start_tok = consume(TokenType::List, "Expected 'list'");
    consume(TokenType::LeftBrace, "Expected '{' after 'list'");
    std::vector<std::unique_ptr<Expr>> elements;
    if (!check(TokenType::RightBrace)) {
        do {
            elements.push_back(parse_expr());
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RightBrace, "Expected '}' after list literal");
    auto expr = std::make_unique<ListLiteralExpr>(std::move(elements));
    expr->set_loc(loc_for(start_tok));
    return expr;
}

std::unique_ptr<Expr> Parser::parse_primary() {
    if (check(TokenType::Identifier)) {
        const auto &tok = advance();
        auto expr = std::make_unique<IdentifierExpr>(tok.lexeme);
        expr->set_loc(loc_for(tok));
        return expr;
    }
    if (check(TokenType::IntegerLiteral)) {
        const auto &tok = advance();
        auto expr = std::make_unique<IntLiteralExpr>(std::stoll(tok.lexeme));
        expr->set_loc(loc_for(tok));
        return expr;
    }
    if (check(TokenType::FloatLiteral)) {
        const auto &tok = advance();
        auto expr = std::make_unique<FloatLiteralExpr>(std::stod(tok.lexeme));
        expr->set_loc(loc_for(tok));
        return expr;
    }
    if (check(TokenType::StringLiteral)) {
        const auto &tok = advance();
        auto expr = std::make_unique<StringLiteralExpr>(tok.lexeme);
        expr->set_loc(loc_for(tok));
        return expr;
    }
    if (check(TokenType::CharLiteral)) {
        const auto &tok = advance();
        char c = tok.lexeme.empty() ? '\0' : tok.lexeme[0];
        auto expr = std::make_unique<CharLiteralExpr>(c);
        expr->set_loc(loc_for(tok));
        return expr;
    }
    if (match(TokenType::True)) {
        auto expr = std::make_unique<BoolLiteralExpr>(true);
        expr->set_loc(loc_for(previous()));
        return expr;
    }
    if (match(TokenType::False)) {
        auto expr = std::make_unique<BoolLiteralExpr>(false);
        expr->set_loc(loc_for(previous()));
        return expr;
    }
    if (match(TokenType::Null)) {
        auto expr = std::make_unique<NullLiteralExpr>();
        expr->set_loc(loc_for(previous()));
        return expr;
    }
    if (match(TokenType::LeftParen)) {
        auto expr = parse_expr();
        consume(TokenType::RightParen, "Expected ')' after grouped expression");
        return expr;
    }
    if (check(TokenType::LeftBracket)) {
        return parse_array_lit();
    }
    if (check(TokenType::List)) {
        return parse_list_lit();
    }
    if (check(TokenType::Alloc)) {
        return parse_alloc();
    }

    error(peek(), "Expected expression");
}
