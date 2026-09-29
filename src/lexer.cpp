#include "lexer.h"
#include <cctype>
#include <sstream>
#include <unordered_map>

std::string_view token_type_str(TokenType type) {
    switch (type) {
    case TokenType::EndOfFile:
        return "EndOfFile";
    case TokenType::Invalid:
        return "Invalid";
    case TokenType::LeftParen:
        return "(";
    case TokenType::RightParen:
        return ")";
    case TokenType::LeftBrace:
        return "{";
    case TokenType::RightBrace:
        return "}";
    case TokenType::LeftBracket:
        return "[";
    case TokenType::RightBracket:
        return "]";
    case TokenType::Comma:
        return ",";
    case TokenType::Dot:
        return ".";
    case TokenType::Semicolon:
        return ";";
    case TokenType::Colon:
        return ":";
    case TokenType::Plus:
        return "+";
    case TokenType::Minus:
        return "-";
    case TokenType::Star:
        return "*";
    case TokenType::Slash:
        return "/";
    case TokenType::Percent:
        return "%";
    case TokenType::Bang:
        return "!";
    case TokenType::Ampersand:
        return "&";
    case TokenType::Equal:
        return "=";
    case TokenType::Less:
        return "<";
    case TokenType::Greater:
        return ">";
    case TokenType::Arrow:
        return "->";
    case TokenType::EqualEqual:
        return "==";
    case TokenType::BangEqual:
        return "!=";
    case TokenType::LessEqual:
        return "<=";
    case TokenType::GreaterEqual:
        return ">=";
    case TokenType::AmpersandAmpersand:
        return "&&";
    case TokenType::PipePipe:
        return "||";
    case TokenType::PlusPlus:
        return "++";
    case TokenType::MinusMinus:
        return "--";
    case TokenType::PlusEqual:
        return "+=";
    case TokenType::MinusEqual:
        return "-=";
    case TokenType::StarEqual:
        return "*=";
    case TokenType::SlashEqual:
        return "/=";
    case TokenType::Identifier:
        return "Identifier";
    case TokenType::IntegerLiteral:
        return "IntegerLiteral";
    case TokenType::FloatLiteral:
        return "FloatLiteral";
    case TokenType::StringLiteral:
        return "StringLiteral";
    case TokenType::CharLiteral:
        return "CharLiteral";
    case TokenType::Include:
        return "include";
    case TokenType::Struct:
        return "struct";
    case TokenType::Def:
        return "def";
    case TokenType::Return:
        return "return";
    case TokenType::Defer:
        return "defer";
    case TokenType::If:
        return "if";
    case TokenType::Else:
        return "else";
    case TokenType::While:
        return "while";
    case TokenType::For:
        return "for";
    case TokenType::Alloc:
        return "alloc";
    case TokenType::Sizeof:
        return "sizeof";
    case TokenType::True:
        return "true";
    case TokenType::False:
        return "false";
    case TokenType::Null:
        return "null";
    case TokenType::List:
        return "list";
    case TokenType::Int:
        return "int";
    case TokenType::I8:
        return "i8";
    case TokenType::I16:
        return "i16";
    case TokenType::I32:
        return "i32";
    case TokenType::I64:
        return "i64";
    case TokenType::UInt:
        return "uint";
    case TokenType::U8:
        return "u8";
    case TokenType::U16:
        return "u16";
    case TokenType::U32:
        return "u32";
    case TokenType::U64:
        return "u64";
    case TokenType::Float:
        return "float";
    case TokenType::F32:
        return "f32";
    case TokenType::F64:
        return "f64";
    case TokenType::Bool:
        return "bool";
    case TokenType::Char:
        return "char";
    case TokenType::Void:
        return "void";
    }
    return "Unknown";
}

std::string Token::to_string() const {
    std::ostringstream out;
    out << token_type_str(type) << "('" << lexeme << "', " << line << ":" << col << ")";
    return out.str();
}

Lexer::Lexer(std::string source) : source_(std::move(source)) {
}

char Lexer::peek() const {
    if (is_at_end()) {
        return '\0';
    }
    return source_[current_];
}

char Lexer::peek_next() const {
    if (current_ + 1 >= source_.length()) {
        return '\0';
    }
    return source_[current_ + 1];
}

char Lexer::advance() {
    char c = source_[current_++];
    if (c == '\n') {
        line_++;
        col_ = 1;
    } else {
        col_++;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (is_at_end() || source_[current_] != expected) {
        return false;
    }
    advance();
    return true;
}

bool Lexer::is_at_end() const {
    return current_ >= source_.length();
}

void Lexer::skip_whitespace() {
    while (!is_at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek_next() == '/') {
            while (!is_at_end() && peek() != '\n') {
                advance();
            }
        } else if (c == '/' && peek_next() == '*') {
            advance();
            advance();
            while (!is_at_end()) {
                if (peek() == '*' && peek_next() == '/') {
                    advance();
                    advance();
                    break;
                }
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::make_token(TokenType type) const {
    return make_token(type, source_.substr(start_, current_ - start_));
}

Token Lexer::make_token(TokenType type, std::string lexeme) const {
    Token token;
    token.type = type;
    token.lexeme = std::move(lexeme);
    token.line = line_;
    token.col = token_start_col_;
    return token;
}

Token Lexer::lex_ident() {
    while (!is_at_end() && (std::isalnum(peek()) || peek() == '_')) {
        advance();
    }

    std::string text = source_.substr(start_, current_ - start_);

    static const std::unordered_map<std::string, TokenType> keywords = {
        {"include", TokenType::Include},
        {"struct", TokenType::Struct},
        {"def", TokenType::Def},
        {"return", TokenType::Return},
        {"defer", TokenType::Defer},
        {"if", TokenType::If},
        {"else", TokenType::Else},
        {"while", TokenType::While},
        {"for", TokenType::For},
        {"alloc", TokenType::Alloc},
        {"sizeof", TokenType::Sizeof},
        {"true", TokenType::True},
        {"false", TokenType::False},
        {"null", TokenType::Null},
        {"list", TokenType::List},
        {"int", TokenType::Int},
        {"i8", TokenType::I8},
        {"i16", TokenType::I16},
        {"i32", TokenType::I32},
        {"i64", TokenType::I64},
        {"uint", TokenType::UInt},
        {"u8", TokenType::U8},
        {"u16", TokenType::U16},
        {"u32", TokenType::U32},
        {"u64", TokenType::U64},
        {"float", TokenType::Float},
        {"f32", TokenType::F32},
        {"f64", TokenType::F64},
        {"bool", TokenType::Bool},
        {"char", TokenType::Char},
        {"void", TokenType::Void}};

    auto it = keywords.find(text);
    if (it != keywords.end()) {
        return make_token(it->second, text);
    }
    return make_token(TokenType::Identifier, text);
}

Token Lexer::lex_number() {
    bool is_float = false;
    while (!is_at_end() && std::isdigit(peek())) {
        advance();
    }

    if (peek() == '.' && std::isdigit(peek_next())) {
        is_float = true;
        advance();
        while (!is_at_end() && std::isdigit(peek())) {
            advance();
        }
    }

    if (peek() == 'f' || peek() == 'F') {
        is_float = true;
        advance();
    }

    return make_token(is_float ? TokenType::FloatLiteral : TokenType::IntegerLiteral);
}

Token Lexer::lex_string() {
    std::string value;
    while (!is_at_end() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            if (is_at_end()) {
                break;
            }
            char esc = advance();
            switch (esc) {
            case 'n':
                value.push_back('\n');
                break;
            case 't':
                value.push_back('\t');
                break;
            case 'r':
                value.push_back('\r');
                break;
            case '\\':
                value.push_back('\\');
                break;
            case '"':
                value.push_back('"');
                break;
            case '0':
                value.push_back('\0');
                break;
            default:
                value.push_back(esc);
                break;
            }
        } else {
            value.push_back(advance());
        }
    }

    if (is_at_end()) {
        return make_token(TokenType::Invalid, "Unterminated string");
    }

    advance();
    return make_token(TokenType::StringLiteral, value);
}

Token Lexer::lex_char() {
    std::string value;
    if (!is_at_end() && peek() != '\'') {
        if (peek() == '\\') {
            advance();
            if (!is_at_end()) {
                char esc = advance();
                switch (esc) {
                case 'n':
                    value.push_back('\n');
                    break;
                case 't':
                    value.push_back('\t');
                    break;
                case 'r':
                    value.push_back('\r');
                    break;
                case '\\':
                    value.push_back('\\');
                    break;
                case '\'':
                    value.push_back('\'');
                    break;
                case '0':
                    value.push_back('\0');
                    break;
                default:
                    value.push_back(esc);
                    break;
                }
            }
        } else {
            value.push_back(advance());
        }
    }

    if (is_at_end() || peek() != '\'') {
        return make_token(TokenType::Invalid, "Unterminated char");
    }

    advance();
    return make_token(TokenType::CharLiteral, value);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skip_whitespace();
        if (is_at_end()) {
            token_start_col_ = col_;
            tokens.push_back(make_token(TokenType::EndOfFile, ""));
            break;
        }

        start_ = current_;
        token_start_col_ = col_;

        char c = advance();

        switch (c) {
        case '(':
            tokens.push_back(make_token(TokenType::LeftParen));
            break;
        case ')':
            tokens.push_back(make_token(TokenType::RightParen));
            break;
        case '{':
            tokens.push_back(make_token(TokenType::LeftBrace));
            break;
        case '}':
            tokens.push_back(make_token(TokenType::RightBrace));
            break;
        case '[':
            tokens.push_back(make_token(TokenType::LeftBracket));
            break;
        case ']':
            tokens.push_back(make_token(TokenType::RightBracket));
            break;
        case ',':
            tokens.push_back(make_token(TokenType::Comma));
            break;
        case '.':
            tokens.push_back(make_token(TokenType::Dot));
            break;
        case ';':
            tokens.push_back(make_token(TokenType::Semicolon));
            break;
        case ':':
            tokens.push_back(make_token(TokenType::Colon));
            break;
        case '%':
            tokens.push_back(make_token(TokenType::Percent));
            break;

        case '+':
            if (match('+')) {
                tokens.push_back(make_token(TokenType::PlusPlus, "++"));
            } else if (match('=')) {
                tokens.push_back(make_token(TokenType::PlusEqual, "+="));
            } else {
                tokens.push_back(make_token(TokenType::Plus, "+"));
            }
            break;

        case '-':
            if (match('>')) {
                tokens.push_back(make_token(TokenType::Arrow, "->"));
            } else if (match('-')) {
                tokens.push_back(make_token(TokenType::MinusMinus, "--"));
            } else if (match('=')) {
                tokens.push_back(make_token(TokenType::MinusEqual, "-="));
            } else {
                tokens.push_back(make_token(TokenType::Minus, "-"));
            }
            break;

        case '*':
            if (match('=')) {
                tokens.push_back(make_token(TokenType::StarEqual, "*="));
            } else {
                tokens.push_back(make_token(TokenType::Star, "*"));
            }
            break;

        case '/':
            if (match('=')) {
                tokens.push_back(make_token(TokenType::SlashEqual, "/="));
            } else {
                tokens.push_back(make_token(TokenType::Slash, "/"));
            }
            break;

        case '!':
            if (match('=')) {
                tokens.push_back(make_token(TokenType::BangEqual, "!="));
            } else {
                tokens.push_back(make_token(TokenType::Bang, "!"));
            }
            break;

        case '=':
            if (match('=')) {
                tokens.push_back(make_token(TokenType::EqualEqual, "=="));
            } else {
                tokens.push_back(make_token(TokenType::Equal, "="));
            }
            break;

        case '<':
            if (match('=')) {
                tokens.push_back(make_token(TokenType::LessEqual, "<="));
            } else {
                tokens.push_back(make_token(TokenType::Less, "<"));
            }
            break;

        case '>':
            if (match('=')) {
                tokens.push_back(make_token(TokenType::GreaterEqual, ">="));
            } else {
                tokens.push_back(make_token(TokenType::Greater, ">"));
            }
            break;

        case '&':
            if (match('&')) {
                tokens.push_back(make_token(TokenType::AmpersandAmpersand, "&&"));
            } else {
                tokens.push_back(make_token(TokenType::Ampersand, "&"));
            }
            break;

        case '|':
            if (match('|')) {
                tokens.push_back(make_token(TokenType::PipePipe, "||"));
            } else {
                tokens.push_back(make_token(TokenType::Invalid, "|"));
            }
            break;

        case '"':
            tokens.push_back(lex_string());
            break;

        case '\'':
            tokens.push_back(lex_char());
            break;

        default:
            if (std::isdigit(c)) {
                current_--;
                col_--;
                tokens.push_back(lex_number());
            } else if (std::isalpha(c) || c == '_') {
                current_--;
                col_--;
                tokens.push_back(lex_ident());
            } else {
                tokens.push_back(make_token(TokenType::Invalid, std::string(1, c)));
            }
            break;
        }
    }

    return tokens;
}
