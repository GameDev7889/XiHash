#ifndef XIHASH_PARSER_H
#define XIHASH_PARSER_H

#include <vector>
#include <unordered_map>

#include "Ast.hpp"
#include "Lexer.hpp"

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

    // Throws XiHashError on syntax errors.
    Program parseProgram();

private:
    const Token& current() const { return tokens[index]; }
    const Token& peekNext() const { return tokens[std::min(index + 1, tokens.size() - 1)]; }
    bool isIdentifier(std::string_view text) const;
    Token expect(TokenKind kind, const char* description);
    Token expectIdentifier(const char* description);

    void skipUsing();
    void parseUsingNamespace(Program& program);
    void parseUsingC(Program& program);
    void parseUsingLibrary(Program& program);
    void parseImport(Program& program);
    void parseExternBlock(Program& program);
    void parseTypeAlias();
    Type parseType();
    Type parseReturnType();
    bool startsType() const;
    Function parseFunction();
    StmtPtr parseStatement();
    StmtPtr parseIf();
    StmtPtr parseWhile();
    std::vector<StmtPtr> parseBlock();
    ExprPtr parseExpression();
    ExprPtr parseBinary(int level);
    ExprPtr parseUnary();
    ExprPtr parsePrimary();
    std::vector<ExprPtr> parseArgs();

    std::vector<Token> tokens;
    std::size_t index = 0;
    std::unordered_map<std::string, Type> typeAliases;
};

#endif
