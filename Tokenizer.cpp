#include "Tokenizer.h"

#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace {

// Recognized words are marked as keywords; all other names remain identifiers.
const std::unordered_set<std::string> keywords = {
    "import", "using", "namespace", "string"
};

// Restrict escapes to the ones the small XiHash example language supports.
bool isSupportedEscape(char escape) {
    switch (escape) {
        case 'n': case 'r': case 't': case '0': case '\\': case '\'': case '"':
            return true;
        default:
            return false;
    }
}

} // namespace

// Split source text into identifiers, literals, operators and punctuation.
std::vector<Token> Tokenizer::tokenize(std::string_view source) {
    std::vector<Token> tokens;
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;

    // Read one source character while keeping the current line and column accurate.
    const auto advance = [&]() {
        const char character = source[offset++];
        if (character == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
        return character;
    };

    // Build an error message that points back to the original XiHash source location.
    const auto fail = [&](std::size_t errorLine, std::size_t errorColumn, const std::string& message) -> void {
        throw std::runtime_error("line " + std::to_string(errorLine) + ", column " +
                                 std::to_string(errorColumn) + ": " + message);
    };

    while (offset < source.size()) {
        const char current = source[offset];

        // Whitespace separates tokens but does not become a token itself.
        if (std::isspace(static_cast<unsigned char>(current))) {
            advance();
            continue;
        }

        // Line comments run through the end of the current source line.
        if (current == '/' && offset + 1 < source.size() && source[offset + 1] == '/') {
            while (offset < source.size() && source[offset] != '\n') {
                advance();
            }
            continue;
        }

        // Block comments are also skipped, and must have a matching closing marker.
        if (current == '/' && offset + 1 < source.size() && source[offset + 1] == '*') {
            const std::size_t commentLine = line;
            const std::size_t commentColumn = column;
            advance();
            advance();
            bool closed = false;
            while (offset < source.size()) {
                if (source[offset] == '*' && offset + 1 < source.size() && source[offset + 1] == '/') {
                    advance();
                    advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                fail(commentLine, commentColumn, "unterminated block comment");
            }
            continue;
        }

        const std::size_t tokenLine = line;
        const std::size_t tokenColumn = column;

        // Names begin with a letter or underscore and may contain digits after that.
        if (std::isalpha(static_cast<unsigned char>(current)) || current == '_') {
            const std::size_t start = offset;
            while (offset < source.size()) {
                const unsigned char character = static_cast<unsigned char>(source[offset]);
                if (!std::isalnum(character) && character != '_') {
                    break;
                }
                advance();
            }
            std::string word(source.substr(start, offset - start));
            const TokenType type = keywords.contains(word) ? TokenType::TOKEN_KEYWORD : TokenType::TOKEN_IDENTIFIER;
            tokens.push_back({type, TokenVarTypes::VAR_NONE, std::move(word), tokenLine, tokenColumn});
            continue;
        }

        // String literals keep their quoted spelling so the C++ generator can reuse it.
        if (current == '"') {
            const std::size_t start = offset;
            advance();
            bool closed = false;
            while (offset < source.size()) {
                const char character = advance();
                if (character == '"') {
                    closed = true;
                    break;
                }
                if (character == '\n' || character == '\r') {
                    fail(tokenLine, tokenColumn, "newline inside string literal");
                }
                if (character == '\\') {
                    if (offset >= source.size()) {
                        break;
                    }
                    const char escape = advance();
                    if (!isSupportedEscape(escape)) {
                        fail(line, column - 1, "unsupported string escape sequence");
                    }
                }
            }
            if (!closed) {
                fail(tokenLine, tokenColumn, "unterminated string literal");
            }
            tokens.push_back({TokenType::TOKEN_LITERAL, TokenVarTypes::VAR_STRING,
                              std::string(source.substr(start, offset - start)), tokenLine, tokenColumn});
            continue;
        }

        // Character literals contain exactly one character or one escaped character.
        if (current == '\'') {
            const std::size_t start = offset;
            advance();
            if (offset >= source.size() || source[offset] == '\n' || source[offset] == '\r' || source[offset] == '\'') {
                fail(tokenLine, tokenColumn, "character literal must contain one character");
            }
            const char value = advance();
            if (value == '\\') {
                if (offset >= source.size()) {
                    fail(tokenLine, tokenColumn, "unterminated character literal");
                }
                const char escape = advance();
                if (!isSupportedEscape(escape)) {
                    fail(line, column - 1, "unsupported character escape sequence");
                }
            }
            if (offset >= source.size() || source[offset] != '\'') {
                fail(tokenLine, tokenColumn, "character literal must contain exactly one character");
            }
            advance();
            tokens.push_back({TokenType::TOKEN_LITERAL, TokenVarTypes::VAR_CHAR,
                              std::string(source.substr(start, offset - start)), tokenLine, tokenColumn});
            continue;
        }

        // Plus and equals are operators; dots and grouping marks are punctuation.
        if (current == '+' || current == '=') {
            tokens.push_back({TokenType::TOKEN_OPERATOR, TokenVarTypes::VAR_NONE,
                              std::string(1, advance()), tokenLine, tokenColumn});
            continue;
        }
        if (current == '.' || current == ';' || current == '(' || current == ')' ||
            current == '{' || current == '}' || current == ',') {
            tokens.push_back({TokenType::TOKEN_PUNCTUATION, TokenVarTypes::VAR_NONE,
                              std::string(1, advance()), tokenLine, tokenColumn});
            continue;
        }

        // Any other character is outside the syntax currently supported by this compiler.
        fail(tokenLine, tokenColumn, std::string("unexpected character '") + current + "'");
    }

    // An explicit end token simplifies parser lookahead at end of file.
    tokens.push_back({TokenType::TOKEN_END, TokenVarTypes::VAR_NONE, "", line, column});
    return tokens;
}
