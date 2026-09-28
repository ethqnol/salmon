#pragma once

#include "token.h"
#include <string>
#include <vector>

class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> tokenize();

private:
    char peek() const;
    char peek_next() const;
    char advance();
    bool match(char expected);
    bool is_at_end() const;

    void skip_whitespace();
    Token make_token(TokenType type) const;
    Token make_token(TokenType type, std::string lexeme) const;

    Token lex_ident();
    Token lex_number();
    Token lex_string();
    Token lex_char();

    std::string source_;
    size_t start_{0};
    size_t current_{0};
    size_t line_{1};
    size_t col_{1};
    size_t token_start_col_{1};
};