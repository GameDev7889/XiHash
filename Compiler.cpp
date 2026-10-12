#include "Compiler.h"

#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "Tokenizer.h"

namespace {

// The compiler only needs to distinguish strings and characters for this example.
enum class ExpressionType {
    String,
    Character
};

// Expression stores both the generated C++ and the value kind used for simple checks.
struct Expression {
    std::string cpp;
    ExpressionType type;
};

// Parser consumes tokens and writes one C++ statement for each XiHash statement.
class Parser {
public:
    explicit Parser(const std::vector<Token>& sourceTokens) : tokens(sourceTokens) {}

    std::string compile() {
        // Imports and namespace directives describe XiHash APIs and need no C++ output.
        while (isKeyword("import") || isKeyword("using")) {
            if (isKeyword("import")) {
                parseImport();
            } else {
                parseUsingNamespace();
            }
        }

        // Parse all executable statements in source order.
        while (!atEnd()) {
            parseStatement();
        }
        return statements.str();
    }

private:
    const std::vector<Token>& tokens;
    std::size_t index = 0;
    std::ostringstream statements;
    std::unordered_map<std::string, ExpressionType> variables;

    // Return the current token; the tokenizer always appends an end token.
    const Token& current() const {
        return tokens[index];
    }

    // Check a token's spelling, independent of whether it is punctuation or a keyword.
    bool is(std::string_view spelling) const {
        return current().lexeme == spelling;
    }

    // Check whether the current token is one of the language keywords.
    bool isKeyword(std::string_view spelling) const {
        return current().type == TokenType::TOKEN_KEYWORD && is(spelling);
    }

    // Check for the final token in the token stream.
    bool atEnd() const {
        return current().type == TokenType::TOKEN_END;
    }

    // Report syntax errors with the XiHash source position attached.
    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error("line " + std::to_string(current().line) + ", column " +
                                 std::to_string(current().column) + ": " + message);
    }

    // Require the next token to have a particular spelling, then consume it.
    Token expect(std::string_view spelling, const char* description) {
        if (!is(spelling)) {
            fail(std::string("expected ") + description);
        }
        return tokens[index++];
    }

    // Require an identifier and consume it.
    Token expectIdentifier(const char* description) {
        if (current().type != TokenType::TOKEN_IDENTIFIER) {
            fail(std::string("expected ") + description);
        }
        return tokens[index++];
    }

    // Accept the XiHash library import used by the example file.
    void parseImport() {
        ++index; // Consume `import`.
        const Token library = expectIdentifier("the XiHash library name");
        if (library.lexeme != "XiHash") {
            fail("this compiler currently supports only 'import XiHash;'");
        }
        expect(";", "';' after the import");
    }

    // Accept the Console namespace directive used for unqualified Console calls.
    void parseUsingNamespace() {
        ++index; // Consume `using`.
        expect("namespace", "'namespace' after 'using'");
        const Token name = expectIdentifier("a namespace name");
        if (name.lexeme != "Console") {
            fail("this compiler currently supports only 'using namespace Console;'");
        }
        expect(";", "';' after the namespace directive");
    }

    // Parse either a string declaration or a Console output call.
    void parseStatement() {
        if (isKeyword("string")) {
            parseStringDeclaration();
            return;
        }

        if (current().type == TokenType::TOKEN_IDENTIFIER) {
            parseConsoleCall();
            return;
        }

        fail("expected a string declaration or Console output call");
    }

    // Parse `string identifier = expression;` and remember the declared variable.
    void parseStringDeclaration() {
        ++index; // Consume `string`.
        const Token variable = expectIdentifier("a variable name");
        if (variables.contains(variable.lexeme)) {
            fail("variable '" + variable.lexeme + "' is already declared");
        }
        expect("=", "'=' after the variable name");
        const Expression initializer = parseExpression();
        if (initializer.type != ExpressionType::String) {
            fail("a string variable must be initialized with a string value");
        }
        expect(";", "';' after the variable declaration");

        variables.emplace(variable.lexeme, ExpressionType::String);
        statements << "    std::string " << variable.lexeme << " = " << initializer.cpp << ";\n";
    }

    // Parse `Console.Print(...)`, `Print(...)`, and their PrintC counterparts.
    void parseConsoleCall() {
        std::string functionName = current().lexeme;
        ++index;

        // A `Console.` prefix is optional because the file also imports that namespace.
        if (is(".")) {
            ++index;
            functionName = expectIdentifier("a Console function name").lexeme;
        }

        if (functionName != "Print" && functionName != "PrintC") {
            fail("unknown Console function '" + functionName + "'");
        }

        expect("(", "'(' after the Console function name");
        const Expression value = parseExpression();
        expect(")", "')' after the Console argument");
        expect(";", "';' after the Console call");

        // PrintC is character-only; Print accepts either a string or a character.
        if (functionName == "PrintC") {
            if (value.type != ExpressionType::Character) {
                fail("PrintC expects a character value");
            }
            statements << "    std::cout.put(" << value.cpp << ");\n";
        } else {
            statements << "    std::cout << " << value.cpp << ";\n";
        }
    }

    // Parse a left-to-right sequence of string concatenations.
    Expression parseExpression() {
        Expression expression = parsePrimary();
        while (is("+")) {
            ++index;
            const Expression right = parsePrimary();
            if (expression.type != ExpressionType::String || right.type != ExpressionType::String) {
                fail("'+' concatenation currently requires string values");
            }
            expression = {"(" + expression.cpp + " + " + right.cpp + ")", ExpressionType::String};
        }
        return expression;
    }

    // Parse a literal, a previously declared string variable, or ReadLine().
    Expression parsePrimary() {
        if (current().type == TokenType::TOKEN_LITERAL) {
            const Token literal = tokens[index++];
            if (literal.varType == TokenVarTypes::VAR_STRING) {
                return {literal.lexeme, ExpressionType::String};
            }
            if (literal.varType == TokenVarTypes::VAR_CHAR) {
                return {literal.lexeme, ExpressionType::Character};
            }
            fail("unsupported literal type");
        }

        if (current().type == TokenType::TOKEN_IDENTIFIER) {
            const Token name = tokens[index++];

            // Support the `Console.ReadLine()` spelling as well as imported `ReadLine()`.
            if (name.lexeme == "Console" && is(".")) {
                ++index;
                const Token function = expectIdentifier("a Console function name");
                if (function.lexeme != "ReadLine") {
                    fail("only Console.ReadLine() is allowed inside an expression");
                }
                return parseReadLineCall();
            }

            // `ReadLine()` consumes one whole input line and returns it as a string.
            if (name.lexeme == "ReadLine" && is("(")) {
                return parseReadLineCall();
            }

            const auto variable = variables.find(name.lexeme);
            if (variable == variables.end()) {
                fail("unknown variable or function '" + name.lexeme + "'");
            }
            return {name.lexeme, variable->second};
        }

        if (is("(")) {
            ++index;
            Expression inner = parseExpression();
            expect(")", "')' after the expression");
            return {"(" + inner.cpp + ")", inner.type};
        }

        fail("expected a string or character expression");
    }

    // Turn a no-argument ReadLine call into the generated C++ input helper call.
    Expression parseReadLineCall() {
        expect("(", "'(' after ReadLine");
        expect(")", "')' after ReadLine");
        return {"xiReadLine()", ExpressionType::String};
    }
};

} // namespace

// Add the small C++ runtime and entry point around the translated statements.
std::string compileToCpp(std::string_view source) {
    const std::vector<Token> tokens = Tokenizer::tokenize(source);
    Parser parser(tokens);
    const std::string body = parser.compile();

    std::ostringstream cpp;
    cpp << "#include <iostream>\n"
           "#include <string>\n\n"
           "// Read one console line after flushing any prompt printed without a newline.\n"
           "static std::string xiReadLine() {\n"
           "    std::cout.flush();\n"
           "    std::string line;\n"
           "    std::getline(std::cin, line);\n"
           "    return line;\n"
           "}\n\n"
           "// XiHash top-level statements execute in this generated C++ entry point.\n"
           "int main() {\n"
        << body <<
           "    return 0;\n"
           "}\n";
    return cpp.str();
}
