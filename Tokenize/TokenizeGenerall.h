#ifndef XIHASH_TOKENIZEGENERALL_H
#define XIHASH_TOKENIZEGENERALL_H

#include <cstddef>
#include <cstdint>
#include <string>

// TokenType describes the syntactic role of a token found in a XiHash source file.
enum class TokenType : std::uint8_t {
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER,
    TOKEN_LITERAL,
    TOKEN_OPERATOR,
    TOKEN_PUNCTUATION,
    TOKEN_END
};

// TokenVarTypes identifies which kind of literal a TOKEN_LITERAL represents.
enum class TokenVarTypes : std::uint8_t {
    VAR_NONE,
    VAR_INT,
    VAR_FLOAT,
    VAR_STRING,
    VAR_BOOL,
    VAR_CHAR
};

// Token stores the original source spelling and where it appeared for useful errors.
struct Token {
    TokenType type = TokenType::TOKEN_END;
    TokenVarTypes varType = TokenVarTypes::VAR_NONE;
    std::string lexeme;
    std::size_t line = 1;
    std::size_t column = 1;
};

#endif // XIHASH_TOKENIZEGENERALL_H
