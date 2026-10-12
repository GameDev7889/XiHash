#include "Parser.h"

#include <cctype>
#include <iostream>
#include <string_view>

std::vector<std::unique_ptr<ASTNode>> Parser::parse(const std::string& fileSource) {
    std::vector<std::unique_ptr<ASTNode>> nodes;
    size_t position = 0;
    bool consoleNamespace = false;

    const auto atEnd = [&] { return position >= fileSource.size(); };
    const auto skipWhitespace = [&] {
        while (!atEnd() && std::isspace(static_cast<unsigned char>(fileSource[position]))) ++position;
    };
    const auto expect = [&](const char expected) {
        if (atEnd() || fileSource[position] != expected) return false;
        ++position;
        return true;
    };
    const auto consumeKeyword = [&](const std::string_view word) {
        if (fileSource.compare(position, word.size(), word) != 0) return false;
        const size_t end = position + word.size();
        if (end < fileSource.size() &&
            (std::isalnum(static_cast<unsigned char>(fileSource[end])) || fileSource[end] == '_')) return false;
        position = end;
        return true;
    };
    const auto readIdentifier = [&] {
        const size_t start = position;
        while (!atEnd() &&
               (std::isalnum(static_cast<unsigned char>(fileSource[position])) || fileSource[position] == '_')) ++position;
        return fileSource.substr(start, position - start);
    };
    const auto readQuotedLiteral = [&] {
        if (atEnd() || (fileSource[position] != '"' && fileSource[position] != '\'')) return std::string{};
        const char quote = fileSource[position++];
        const size_t start = position - 1;
        bool escaped = false;
        while (!atEnd()) {
            const char current = fileSource[position++];
            if (current == quote && !escaped) return fileSource.substr(start, position - start);
            if (current == '\\' && !escaped) escaped = true;
            else escaped = false;
        }
        return std::string{};
    };
    const auto fail = [&](const std::string& message) {
        std::cerr << "Parse error at position " << position << ": " << message << '\n';
        nodes.clear();
    };
    const auto parsePrint = [&](const bool printCharacter) {
        skipWhitespace();
        if (!expect('(')) return false;
        skipWhitespace();
        const std::string argument = readQuotedLiteral();
        if (argument.empty()) return false;

        if (printCharacter) {
            if (const size_t characterCount = argument.size() - 2; (argument.front() != '\'' && argument.front() != '"')
                || (characterCount != 1 && !(characterCount == 2 && argument[1] == '\\'))) return false;
            nodes.push_back(std::make_unique<PrintCNode>(argument));
        } else {
            if (argument.front() != '"') return false;
            nodes.push_back(std::make_unique<PrintNode>(argument));
        }

        return expect(')') && expect(';');
    };

    while (true) {
        skipWhitespace();
        if (atEnd()) return nodes;

        if (consumeKeyword("import")) {
            skipWhitespace();
            const std::string module = readIdentifier();
            if (module.empty() || !expect(';')) {
                fail("expected module name and ';'");
                return nodes;
            }
            if (module != "XiHash") {
                fail("unknown import '" + module + "'");
                return nodes;
            }
            nodes.push_back(std::make_unique<ImportNode>("iostream"));
        } else if (consumeKeyword("using")) {
            skipWhitespace();
            if (!consumeKeyword("namespace")) {
                fail("expected 'namespace'");
                return nodes;
            }
            skipWhitespace();
            const std::string name = readIdentifier();
            if (name.empty() || !expect(';')) {
                fail("expected namespace name and ';'");
                return nodes;
            }
            if (name != "Console") {
                fail("unknown namespace '" + name + "'");
                return nodes;
            }
            consoleNamespace = true;
            nodes.push_back(std::make_unique<UsingNamespaceNode>("std"));
        } else {
            bool qualified = false;
            if (consumeKeyword("Console")) {
                skipWhitespace();
                if (!expect('.')) {
                    fail("expected '.' after Console");
                    return nodes;
                }
                skipWhitespace();
                qualified = true;
            }

            bool printCharacter = false;
            if (consumeKeyword("Print")) {
                printCharacter = false;
            } else if (consumeKeyword("PrintC")) {
                printCharacter = true;
            } else {
                fail("expected Print, PrintC, import, or using statement");
                return nodes;
            }

            if (!qualified && !consoleNamespace) {
                fail("Print and PrintC require 'using namespace Console;' or the 'Console.' prefix");
                return nodes;
            }
            if (!parsePrint(printCharacter)) {
                fail(printCharacter ? "expected one character in PrintC('x');" : "expected a string in Print(\"text\");");
                return nodes;
            }
        }
    }
}
