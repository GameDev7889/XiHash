#ifndef XIHASH_LEXER_H
#define XIHASH_LEXER_H

#include <string>
#include <string_view>
#include <vector>

#include "Ast.hpp"

enum class TokenKind {
    Identifier, IntLiteral, FloatLiteral, StringLiteral, CharLiteral,
    LParen, RParen, LBrace, RBrace, LBracket, RBracket, Semicolon, Comma, Assign, Plus, Minus, Star, Slash, Dot,
    Bang, Amp, EqEq, NotEq, Less, LessEq, Greater, GreaterEq, AmpAmp, PipePipe, End
};

struct Token {
    TokenKind kind;
    std::string text;
    SourcePos pos;
};

// Throws XiHashError on invalid characters, bad escapes and unterminated strings/comments.
std::vector<Token> lex(std::string_view source);

#endif
