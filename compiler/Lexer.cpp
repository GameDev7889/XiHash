#include "Lexer.hpp"

#include <cctype>
#include <optional>

namespace {
bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }

std::optional<TokenKind> twoCharToken(char first, char second) {
    if (second == '=') {
        switch (first) {
            case '=': return TokenKind::EqEq;
            case '!': return TokenKind::NotEq;
            case '<': return TokenKind::LessEq;
            case '>': return TokenKind::GreaterEq;
            default: break;
        }
    }
    if (first == '&' && second == '&') return TokenKind::AmpAmp;
    if (first == '|' && second == '|') return TokenKind::PipePipe;
    return std::nullopt;
}
}

std::vector<Token> lex(std::string_view source) {
    std::vector<Token> tokens;
    std::size_t i = 0;
    SourcePos pos;

    const auto peek = [&](std::size_t offset = 0) {
        return i + offset < source.size() ? source[i + offset] : '\0';
    };
    const auto advance = [&]() {
        if (source[i] == '\n') {
            ++pos.line;
            pos.column = 1;
        } else {
            ++pos.column;
        }
        return source[i++];
    };

    while (i < source.size()) {
        const char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
            continue;
        }
        if (c == '/' && peek(1) == '/') {
            while (i < source.size() && peek() != '\n') {
                advance();
            }
            continue;
        }
        if (c == '/' && peek(1) == '*') {
            const SourcePos start = pos;
            advance();
            advance();
            while (i < source.size() && !(peek() == '*' && peek(1) == '/')) {
                advance();
            }
            if (i >= source.size()) {
                throw XiHashError(start, "Unterminated block comment");
            }
            advance();
            advance();
            continue;
        }

        const SourcePos start = pos;
        if (isIdentStart(c)) {
            std::string text;
            while (isIdentStart(peek()) || isDigit(peek())) {
                text += advance();
            }
            tokens.push_back({TokenKind::Identifier, text, start});
        } else if (isDigit(c)) {
            std::string text;
            while (isDigit(peek())) {
                text += advance();
            }
            bool isFloat = false;
            if (peek() == '.' && isDigit(peek(1))) {
                isFloat = true;
                text += advance();
                while (isDigit(peek())) {
                    text += advance();
                }
            }
            tokens.push_back({isFloat ? TokenKind::FloatLiteral : TokenKind::IntLiteral, text, start});
        } else if (c == '"') {
            advance();
            std::string text;
            bool terminated = false;
            while (i < source.size()) {
                const char ch = advance();
                if (ch == '"') {
                    terminated = true;
                    break;
                }
                if (ch == '\n') {
                    break;
                }
                if (ch != '\\') {
                    text += ch;
                    continue;
                }
                const SourcePos escapePos = pos;
                if (i >= source.size()) {
                    break;
                }
                switch (advance()) {
                    case 'n': text += '\n'; break;
                    case 'r': text += '\r'; break;
                    case 't': text += '\t'; break;
                    case '\\': text += '\\'; break;
                    case '"': text += '"'; break;
                    default: throw XiHashError(escapePos, "Unsupported string escape");
                }
            }
            if (!terminated) {
                throw XiHashError(start, "Unterminated string literal");
            }
            tokens.push_back({TokenKind::StringLiteral, text, start});
        } else if (c == '\'') {
            advance();
            if (i >= source.size() || peek() == '\n') {
                throw XiHashError(start, "Unterminated character literal");
            }
            const char ch = advance();
            char value = ch;
            if (ch == '\'') {
                throw XiHashError(start, "Empty character literal");
            }
            if (ch == '\\') {
                const SourcePos escapePos = pos;
                if (i >= source.size()) {
                    throw XiHashError(start, "Unterminated character literal");
                }
                switch (advance()) {
                    case 'n': value = '\n'; break;
                    case 'r': value = '\r'; break;
                    case 't': value = '\t'; break;
                    case '0': value = '\0'; break;
                    case '\\': value = '\\'; break;
                    case '\'': value = '\''; break;
                    case '"': value = '"'; break;
                    default: throw XiHashError(escapePos, "Unsupported character escape");
                }
            }
            if (peek() != '\'') {
                throw XiHashError(start, "Character literal must contain exactly one character");
            }
            advance();
            tokens.push_back({TokenKind::CharLiteral, std::string(1, value), start});
        } else if (const auto twoChar = twoCharToken(c, peek(1))) {
            advance();
            advance();
            tokens.push_back({*twoChar, std::string(source.substr(i - 2, 2)), start});
        } else {
            TokenKind kind;
            switch (c) {
                case '(': kind = TokenKind::LParen; break;
                case ')': kind = TokenKind::RParen; break;
                case '{': kind = TokenKind::LBrace; break;
                case '}': kind = TokenKind::RBrace; break;
                case '[': kind = TokenKind::LBracket; break;
                case ']': kind = TokenKind::RBracket; break;
                case ';': kind = TokenKind::Semicolon; break;
                case ',': kind = TokenKind::Comma; break;
                case '=': kind = TokenKind::Assign; break;
                case '+': kind = TokenKind::Plus; break;
                case '-': kind = TokenKind::Minus; break;
                case '*': kind = TokenKind::Star; break;
                case '/': kind = TokenKind::Slash; break;
                case '.': kind = TokenKind::Dot; break;
                case '!': kind = TokenKind::Bang; break;
                case '&': kind = TokenKind::Amp; break;
                case '<': kind = TokenKind::Less; break;
                case '>': kind = TokenKind::Greater; break;
                default:
                    throw XiHashError(start, std::string("Unexpected character '") + c + "'");
            }
            tokens.push_back({kind, std::string(1, advance()), start});
        }
    }

    tokens.push_back({TokenKind::End, "", pos});
    return tokens;
}
