#pragma once

#include <cstdint>
#include <string>
#include <string_view>

enum class TokenType : uint8_t {
    EndOfFile,
    Invalid,

    // Delimiters
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    Comma,
    Dot,
    Semicolon,
    Colon,

    // Operators
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Bang,
    Ampersand,
    Equal,
    Less,
    Greater,

    // Compound Operators
    Arrow,              // ->
    EqualEqual,         // ==
    BangEqual,          // !=
    LessEqual,          // <=
    GreaterEqual,       // >=
    AmpersandAmpersand, // &&
    PipePipe,           // ||
    PlusPlus,           // ++
    MinusMinus,         // --
    PlusEqual,          // +=
    MinusEqual,         // -=
    StarEqual,          // *=
    SlashEqual,         // /=

    // Literals
    Identifier,
    IntegerLiteral,
    FloatLiteral,
    StringLiteral,
    CharLiteral,

    // Keywords
    Include,
    Struct,
    Def,
    Return,
    Defer,
    If,
    Else,
    While,
    For,
    Alloc,
    Sizeof,
    True,
    False,
    Null,
    List,

    // Primitive Types
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

struct Token {
    TokenType type{TokenType::Invalid};
    std::string lexeme;
    size_t line{1};
    size_t col{1};

    std::string to_string() const;
};

std::string_view token_type_str(TokenType type);