#include "Parser.hpp"

#include <algorithm>
#include <charconv>
#include <optional>
#include <cctype>
#include <limits>
#include <unordered_set>

namespace {
// Binary operator precedence levels, lowest to highest. Level 6 is unary.
constexpr int unaryLevel = 6;

std::optional<BinaryOp> binaryOpFor(TokenKind kind, int level) {
    switch (level) {
        case 0: if (kind == TokenKind::PipePipe) return BinaryOp::Or; break;
        case 1: if (kind == TokenKind::AmpAmp) return BinaryOp::And; break;
        case 2:
            if (kind == TokenKind::EqEq) return BinaryOp::Eq;
            if (kind == TokenKind::NotEq) return BinaryOp::Ne;
            break;
        case 3:
            if (kind == TokenKind::Less) return BinaryOp::Lt;
            if (kind == TokenKind::LessEq) return BinaryOp::Le;
            if (kind == TokenKind::Greater) return BinaryOp::Gt;
            if (kind == TokenKind::GreaterEq) return BinaryOp::Ge;
            break;
        case 4:
            if (kind == TokenKind::Plus) return BinaryOp::Add;
            if (kind == TokenKind::Minus) return BinaryOp::Sub;
            break;
        case 5:
            if (kind == TokenKind::Star) return BinaryOp::Mul;
            if (kind == TokenKind::Slash) return BinaryOp::Div;
            break;
        default: break;
    }
    return std::nullopt;
}

bool isReserved(const std::string& word) {
    return typeFromName(word) || word == "return" || word == "using" || word == "importC" || word == "if" || word == "else"
        || word == "true" || word == "false" || word == "null" || word == "new" || word == "delete"
        || word == "unsafe" || word == "move" || word == "extern"
        || word == "usingL" || word == "namespace" || word == "void" || word == "type"
        || word == "external"
        || word == "while" || word == "import" || word == "const" || word == "static" || word == "Array"
        || word == "struct" || word == "in" || word == "readonly";
}

bool isHeaderPathToken(TokenKind kind) {
    return kind == TokenKind::Identifier || kind == TokenKind::Slash || kind == TokenKind::Dot
        || kind == TokenKind::Minus;
}

bool isLibraryName(const std::string& name) {
    return !name.empty() && std::all_of(name.begin(), name.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_' || c == '.' || c == '+' || c == '-';
    });
}
}

bool Parser::isIdentifier(std::string_view text) const {
    return current().kind == TokenKind::Identifier && current().text == text;
}

Token Parser::expect(TokenKind kind, const char* description) {
    if (current().kind != kind) {
        throw XiHashError(current().pos, std::string("Expected ") + description);
    }
    return tokens[index++];
}

Token Parser::expectIdentifier(const char* description) {
    Token token = expect(TokenKind::Identifier, description);
    if (isReserved(token.text)) {
        throw XiHashError(token.pos, "'" + token.text + "' is a reserved word");
    }
    return token;
}

Program Parser::parseProgram() {
    Program program;
    while (true) {
        if (isIdentifier("import")) {
            if (peekNext().kind == TokenKind::Identifier && peekNext().text == "namespace") {
                parseUsingNamespace(program);
            } else if (peekNext().kind == TokenKind::StringLiteral) {
                parseImport(program);
            } else if (peekNext().kind == TokenKind::Identifier) {
                parseNamedImport(program);
            } else {
                skipUsing();
            }
        } else if (isIdentifier("type")) {
            parseTypeAlias();
        } else if (isIdentifier("struct")) {
            parseStruct(program);
        } else if (isIdentifier("using")) {
            if (peekNext().kind == TokenKind::Identifier && peekNext().text == "namespace") {
                parseUsingNamespace(program);
            } else {
                skipUsing();
            }
        } else if (isIdentifier("importC")) {
            parseUsingC(program);
        } else if (isIdentifier("usingL")) {
            parseUsingLibrary(program);
        } else if (isIdentifier("extern")) {
            parseExternBlock(program);
        } else {
            break;
        }
    }
    while (current().kind != TokenKind::End) {
        program.functions.push_back(parseFunction());
    }
    return program;
}

void Parser::parseStruct(Program& program) {
    const SourcePos pos = current().pos;
    ++index;
    const Token name = expectIdentifier("a struct name");
    if (typeAliases.contains(name.text) || typeFromName(name.text)) {
        throw XiHashError(name.pos, "Type '" + name.text + "' is already declared");
    }
    expect(TokenKind::LBrace, "'{' after struct name");
    StructDecl declaration;
    declaration.pos = pos;
    declaration.name = name.text;
    std::unordered_set<std::string> fieldNames;
    while (current().kind != TokenKind::RBrace) {
        if (current().kind == TokenKind::End) throw XiHashError(current().pos, "Expected '}' to close struct");
        const Type fieldType = parseType();
        const Token fieldName = expectIdentifier("a struct field name");
        expect(TokenKind::Semicolon, "';' after struct field");
        if (fieldType == Type::Void || fieldType.kind == Type::Struct || fieldType.kind == Type::Pointer) {
            throw XiHashError(fieldName.pos, "Struct fields must have a concrete non-pointer value type");
        }
        if (!fieldNames.insert(fieldName.text).second) {
            throw XiHashError(fieldName.pos, "Struct field '" + fieldName.text + "' is already declared");
        }
        declaration.fields.push_back({fieldType, fieldName.text});
    }
    ++index;
    if (declaration.fields.empty()) throw XiHashError(pos, "A struct must have at least one field");
    typeAliases.emplace(name.text, makeStruct(name.text));
    program.structs.push_back(std::move(declaration));
}

void Parser::parseNamedImport(Program& program) {
    const SourcePos pos = current().pos;
    ++index;
    const Token root = expect(TokenKind::Identifier, "a module namespace");
    expect(TokenKind::Dot, "'.' in a module name");
    const Token name = expect(TokenKind::Identifier, "a module name");
    expect(TokenKind::Semicolon, "';' after module import");
    if (root.text != "XiHash" || name.text != "Numerals") {
        throw XiHashError(pos, "Unknown XiHash module '" + root.text + "." + name.text + "'");
    }
    if (typeAliases.contains("Vector2")) return;
    typeAliases.emplace("Vector2", makeVector(Type::Float, 2));
    typeAliases.emplace("Vector3", makeVector(Type::Float, 3));
    typeAliases.emplace("Vector4", makeVector(Type::Float, 4));
    for (const auto& suffix : {std::string("i"), std::string("u"), std::string("d")}) {
        const Type scalar = suffix == "i" ? Type::Int : suffix == "u" ? Type::UInt : Type::Double;
        for (const int n : {2, 3, 4}) {
            typeAliases.emplace("Vector" + std::to_string(n) + suffix, makeVector(scalar, n));
        }
    }
    program.cHeaders.emplace_back("XiHash/Numerals.hpp");
    ExternBlock numerals;
    numerals.pos = pos;
    numerals.name = "Numerals";
    const auto add = [&](std::string fn, Type ret, std::vector<ExternParam> params) {
        numerals.functions.push_back({pos, std::move(ret), std::move(fn), std::move(params)});
    };
    const auto param = [](Type type, const char* name) { return ExternParam{std::move(type), name}; };
    add("Vector2", makeVector(Type::Float, 2), {param(Type::Float, "x"), param(Type::Float, "y")});
    add("Vector3", makeVector(Type::Float, 3), {param(Type::Float, "x"), param(Type::Float, "y"), param(Type::Float, "z")});
    add("Vector4", makeVector(Type::Float, 4), {param(Type::Float, "x"), param(Type::Float, "y"), param(Type::Float, "z"), param(Type::Float, "w")});
    for (const auto& suffix : {std::string("i"), std::string("u"), std::string("d")}) {
        const Type scalar = suffix == "i" ? Type::Int : suffix == "u" ? Type::UInt : Type::Double;
        for (const int n : {2, 3, 4}) {
            std::vector<ExternParam> params;
            for (int i = 0; i < n; ++i) params.push_back(param(scalar, "component"));
            add("Vector" + std::to_string(n) + suffix, makeVector(scalar, n), std::move(params));
        }
    }
    for (const int n : {2, 3, 4}) {
        const Type vec = makeVector(Type::Float, n);
        const auto vectorParam = [&, vec](const char* label) { return param(vec, label); };
        add("add" + std::to_string(n), vec, {vectorParam("left"), vectorParam("right")});
        add("subtract" + std::to_string(n), vec, {vectorParam("left"), vectorParam("right")});
        add("scale" + std::to_string(n), vec, {vectorParam("value"), param(Type::Float, "scalar")});
        add("dot" + std::to_string(n), Type::Float, {vectorParam("left"), vectorParam("right")});
        add("length" + std::to_string(n), Type::Float, {vectorParam("value")});
        add("normalize" + std::to_string(n), vec, {vectorParam("value")});
    }
    const Type vec3 = makeVector(Type::Float, 3);
    add("cross3", vec3, {param(vec3, "left"), param(vec3, "right")});
    for (const auto& suffix : {std::string("i"), std::string("u"), std::string("d")}) {
        const Type scalar = suffix == "i" ? Type::Int : suffix == "u" ? Type::UInt : Type::Double;
        for (const int n : {2, 3, 4}) {
            const Type vec = makeVector(scalar, n);
            const auto vectorParam = [&, vec](const char* label) { return param(vec, label); };
            add("add" + std::to_string(n) + suffix, vec, {vectorParam("left"), vectorParam("right")});
            add("subtract" + std::to_string(n) + suffix, vec, {vectorParam("left"), vectorParam("right")});
            add("scale" + std::to_string(n) + suffix, vec, {vectorParam("value"), param(scalar, "scalar")});
            add("dot" + std::to_string(n) + suffix, scalar, {vectorParam("left"), vectorParam("right")});
            add("length" + std::to_string(n) + suffix, scalar, {vectorParam("value")});
            add("normalize" + std::to_string(n) + suffix, vec, {vectorParam("value")});
        }
        const Type three = makeVector(scalar, 3);
        add("cross3" + suffix, three, {param(three, "left"), param(three, "right")});
    }
    program.externBlocks.push_back(std::move(numerals));
}

void Parser::parseImport(Program& program) {
    const SourcePos pos = current().pos;
    ++index;
    const Token path = expect(TokenKind::StringLiteral, "a quoted XiHash source path");
    if (path.text.empty()) {
        throw XiHashError(path.pos, "Import path cannot be empty");
    }
    expect(TokenKind::Semicolon, "';' after import");
    program.imports.emplace_back(path.text, pos);
}

void Parser::skipUsing() {
    ++index;
    while (current().kind != TokenKind::Semicolon) {
        if (current().kind == TokenKind::End) {
            throw XiHashError(current().pos, "Expected ';' after using directive");
        }
        ++index;
    }
    ++index;
}

// `import namespace name;` opens a previously declared `extern name { ... }` block so its
// functions can be called unqualified.
void Parser::parseUsingNamespace(Program& program) {
    ++index; // 'import'
    ++index; // 'namespace'
    const Token name = expect(TokenKind::Identifier, "a namespace name");
    expect(TokenKind::Semicolon, "';' after import namespace");
    program.openedNamespaces.emplace_back(name.text, name.pos);
}

// `importC <header>;` includes a system header; `importC "file.cpp";` compiles a source file.
void Parser::parseUsingC(Program& program) {
    ++index; // 'importC'
    if (current().kind == TokenKind::StringLiteral) {
        const Token path = tokens[index++];
        if (path.text.empty()) throw XiHashError(path.pos, "C/C++ import path cannot be empty");
        expect(TokenKind::Semicolon, "';' after importC");
        program.cSources.emplace_back(path.text);
        return;
    }
    expect(TokenKind::Less, "'<' or a quoted source path after importC");
    std::string header;
    while (current().kind != TokenKind::Greater) {
        if (!isHeaderPathToken(current().kind)) {
            throw XiHashError(current().pos, "Invalid character in header name");
        }
        header += tokens[index++].text;
    }
    if (header.empty()) {
        throw XiHashError(current().pos, "Expected a header name");
    }
    expect(TokenKind::Greater, "'>' after header name");
    expect(TokenKind::Semicolon, "';' after importC");
    program.cHeaders.push_back(std::move(header));
}

void Parser::parseUsingLibrary(Program& program) {
    ++index; // 'usingL'
    expect(TokenKind::Less, "'<' after usingL");
    std::string library;
    while (current().kind != TokenKind::Greater) {
        if (current().kind != TokenKind::Identifier && current().kind != TokenKind::Dot
            && current().kind != TokenKind::Plus && current().kind != TokenKind::Minus) {
            throw XiHashError(current().pos, "Invalid character in library name");
        }
        library += tokens[index++].text;
    }
    if (!isLibraryName(library)) {
        throw XiHashError(current().pos, "Invalid or empty library name");
    }
    expect(TokenKind::Greater, "'>' after library name");
    expect(TokenKind::Semicolon, "';' after usingL");
    program.linkLibraries.push_back(std::move(library));
}

void Parser::parseTypeAlias() {
    ++index; // 'type'
    const Token name = expectIdentifier("a type name");
    expect(TokenKind::Assign, "'=' after type name");
    Type type;
    if (isIdentifier("external")) {
        ++index;
        const Token nativeName = expectIdentifier("an external type name");
        if (current().kind != TokenKind::Star) {
            throw XiHashError(current().pos, "External types must currently be opaque pointer types");
        }
        ++index;
        type = makePointer(Type::Void, true);
        type.cppTypeName = "::" + nativeName.text + "*";
    } else {
        type = parseType();
    }
    expect(TokenKind::Semicolon, "';' after type alias");
    if (typeAliases.contains(name.text) || typeFromName(name.text)) {
        throw XiHashError(name.pos, "Type '" + name.text + "' is already declared");
    }
    typeAliases.emplace(name.text, std::move(type));
}

// `extern name { returnType function(type param, ...); ... }` declares the signatures of
// functions that already exist in an included header, so calls to them can be type-checked.
void Parser::parseExternBlock(Program& program) {
    const SourcePos pos = current().pos;
    ++index; // 'extern'
    const Token name = expect(TokenKind::Identifier, "an extern block name");
    expect(TokenKind::LBrace, "'{' after extern block name");

    ExternBlock block;
    block.pos = pos;
    block.name = name.text;

    while (current().kind != TokenKind::RBrace) {
        if (current().kind == TokenKind::End) {
            throw XiHashError(current().pos, "Expected '}' to close extern block");
        }
        ExternFunction function;
        function.pos = current().pos;
        function.returnType = parseReturnType();
        function.name = expectIdentifier("a function name").text;
        expect(TokenKind::LParen, "'(' after function name");
        if (current().kind != TokenKind::RParen) {
            while (true) {
                const Type paramType = parseType();
                const Token paramName = expectIdentifier("a parameter name");
                function.params.push_back({paramType, paramName.text});
                if (current().kind == TokenKind::Comma) {
                    ++index;
                    continue;
                }
                break;
            }
        }
        expect(TokenKind::RParen, "')'");
        expect(TokenKind::Semicolon, "';' after extern function signature");
        block.functions.push_back(std::move(function));
    }
    ++index; // '}'
    program.externBlocks.push_back(std::move(block));
}

bool Parser::startsType() const {
    return current().kind == TokenKind::Identifier &&
        (current().text == "unsafe" || current().text == "Array" || typeFromName(current().text) || typeAliases.contains(current().text));
}

// [unsafe] base-type {'*'}. `unsafe` applies to the outermost pointer.
Type Parser::parseType() {
    bool isUnsafe = false;
    if (isIdentifier("unsafe")) {
        ++index;
        isUnsafe = true;
    }
    const Token baseToken = expect(TokenKind::Identifier, "a type");
    if (baseToken.text == "Array") {
        expect(TokenKind::Less, "'<' after Array");
        Type element = parseType();
        std::size_t size = 0;
        if (current().kind == TokenKind::Comma) {
            ++index;
            const Token sizeToken = expect(TokenKind::IntLiteral, "a fixed array length");
            unsigned long long parsed = 0;
            const auto result = std::from_chars(sizeToken.text.data(), sizeToken.text.data() + sizeToken.text.size(), parsed);
            if (result.ec != std::errc() || parsed == 0 || parsed > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
                throw XiHashError(sizeToken.pos, "Array length must be a positive integer");
            }
            size = static_cast<std::size_t>(parsed);
        }
        expect(TokenKind::Greater, "'>' after Array type");
        if (element == Type::Void || element.kind == Type::Array) {
            throw XiHashError(baseToken.pos, "Array elements must have a non-array, non-void type");
        }
        Type array = makeArray(element, size);
        bool isPointerType = false;
        while (current().kind == TokenKind::Star) {
            ++index;
            array = makePointer(array, false);
            isPointerType = true;
        }
        if (isUnsafe) {
            if (!isPointerType) throw XiHashError(baseToken.pos, "'unsafe' can only be applied to a pointer type");
            array.isUnsafe = true;
        }
        return array;
    }
    const auto alias = typeAliases.find(baseToken.text);
    const auto base = alias != typeAliases.end() ? std::optional<Type>(alias->second) : typeFromName(baseToken.text);
    if (!base) {
        throw XiHashError(baseToken.pos, "Unknown type '" + baseToken.text + "'");
    }
    Type type = *base;
    bool isPointerType = false;
    while (current().kind == TokenKind::Star) {
        ++index;
        type = makePointer(type, false);
        isPointerType = true;
    }
    if (isUnsafe) {
        if (!isPointerType) {
            throw XiHashError(baseToken.pos, "'unsafe' can only be applied to a pointer type");
        }
        type.isUnsafe = true;
    }
    return type;
}

// Like parseType(), but also accepts a bare `void` (only meaningful as an extern function's
// return type, since typeFromName() now recognizes it for parseType() to use directly).
Type Parser::parseReturnType() {
    return parseType();
}

Function Parser::parseFunction() {
    Function function;
    function.pos = current().pos;
    function.returnType = parseType();
    function.name = expectIdentifier("a function name").text;
    expect(TokenKind::LParen, "'('");
    if (current().kind != TokenKind::RParen) {
        while (true) {
            const Type paramType = parseType();
            const Token paramName = expectIdentifier("a parameter name");
            function.params.push_back({paramType, paramName.text});
            if (current().kind != TokenKind::Comma) {
                break;
            }
            ++index;
        }
    }
    expect(TokenKind::RParen, "')'");
    function.body = parseBlock();
    return function;
}

std::vector<StmtPtr> Parser::parseBlock() {
    expect(TokenKind::LBrace, "'{'");
    std::vector<StmtPtr> body;
    while (current().kind != TokenKind::RBrace) {
        if (current().kind == TokenKind::End) {
            throw XiHashError(current().pos, "Expected '}'");
        }
        body.push_back(parseStatement());
    }
    ++index;
    return body;
}

StmtPtr Parser::parseIf() {
    const SourcePos pos = current().pos;
    ++index;
    expect(TokenKind::LParen, "'(' after if");
    auto condition = parseExpression();
    expect(TokenKind::RParen, "')' after condition");
    auto thenBody = parseBlock();
    std::vector<StmtPtr> elseBody;
    if (isIdentifier("else")) {
        ++index;
        if (isIdentifier("if")) {
            elseBody.push_back(parseIf());
        } else {
            elseBody = parseBlock();
        }
    }
    return std::make_unique<IfStmt>(pos, std::move(condition), std::move(thenBody), std::move(elseBody));
}

StmtPtr Parser::parseWhile() {
    const SourcePos pos = current().pos;
    ++index;
    expect(TokenKind::LParen, "'(' after while");
    auto condition = parseExpression();
    expect(TokenKind::RParen, "')' after condition");
    auto body = parseBlock();
    return std::make_unique<WhileStmt>(pos, std::move(condition), std::move(body));
}

StmtPtr Parser::parseStatement() {
    const SourcePos pos = current().pos;
    if (current().kind == TokenKind::Star) {
        auto target = parseUnary();
        expect(TokenKind::Assign, "'=' after dereferenced pointer");
        auto value = parseExpression();
        expect(TokenKind::Semicolon, "';' after assignment");
        return std::make_unique<AssignStmt>(pos, std::move(target), std::move(value));
    }
    if (current().kind != TokenKind::Identifier) {
        throw XiHashError(pos, "Expected a statement");
    }

    if (isIdentifier("delete")) {
        ++index;
        auto operand = parseExpression();
        expect(TokenKind::Semicolon, "';' after delete");
        return std::make_unique<DeleteStmt>(pos, std::move(operand));
    }

    if (isIdentifier("if")) {
        return parseIf();
    }

    if (isIdentifier("while")) {
        return parseWhile();
    }

    if (isIdentifier("return")) {
        ++index;
        auto value = parseExpression();
        expect(TokenKind::Semicolon, "';' after return");
        return std::make_unique<ReturnStmt>(pos, std::move(value));
    }

    if (isIdentifier("Console")) {
        ++index;
        expect(TokenKind::Dot, "'.' after Console");
        const Token member = expect(TokenKind::Identifier, "'WriteLine' or 'Write'");
        if (member.text != "WriteLine" && member.text != "Write") {
            throw XiHashError(member.pos, "Unknown Console function '" + member.text + "'");
        }
        expect(TokenKind::LParen, "'(' after Console function");
        auto value = parseExpression();
        expect(TokenKind::RParen, "')'");
        expect(TokenKind::Semicolon, "';' after Console call");
        return std::make_unique<WriteLineStmt>(pos, std::move(value), member.text == "WriteLine");
    }

    bool isConst = false;
    bool isStatic = false;
    bool isReadonly = false;
    while (isIdentifier("const") || isIdentifier("static") || isIdentifier("readonly")) {
        if (isIdentifier("const")) {
            if (isConst) {
                throw XiHashError(current().pos, "Duplicate 'const' modifier");
            }
            isConst = true;
        } else if (isIdentifier("static")) {
            if (isStatic) {
                throw XiHashError(current().pos, "Duplicate 'static' modifier");
            }
            isStatic = true;
        } else {
            if (isReadonly) {
                throw XiHashError(current().pos, "Duplicate 'readonly' modifier");
            }
            isReadonly = true;
        }
        ++index;
    }
    if (isConst || isStatic || isReadonly || startsType()) {
        if (!startsType()) {
            throw XiHashError(current().pos, "Expected a type after declaration modifier");
        }
        const Type declType = parseType();
        const Token name = expectIdentifier("a variable name");
        if (current().kind == TokenKind::Semicolon) {
            if (declType.kind != Type::Struct) {
                throw XiHashError(current().pos, "Only struct variables may be declared without an initializer");
            }
            ++index;
            return std::make_unique<VarDeclStmt>(pos, declType, name.text, nullptr, isConst, isStatic, isReadonly);
        }
        expect(TokenKind::Assign, "'=' (variables must be initialized)");
        auto init = parseExpression();
        expect(TokenKind::Semicolon, "';' after declaration");
        return std::make_unique<VarDeclStmt>(pos, declType, name.text, std::move(init), isConst, isStatic, isReadonly);
    }

    if (peekNext().kind == TokenKind::Assign) {
        const Token name = tokens[index];
        index += 2;
        auto value = parseExpression();
        expect(TokenKind::Semicolon, "';' after assignment");
        return std::make_unique<AssignStmt>(pos, std::make_unique<VariableExpr>(name.pos, name.text),
                                            std::move(value));
    }

    // Index assignment or a call used for its side effect.
    if (current().kind == TokenKind::Identifier && !isReserved(current().text) &&
        (peekNext().kind == TokenKind::LParen || peekNext().kind == TokenKind::Dot ||
         peekNext().kind == TokenKind::LBracket)) {
        auto expr = parseExpression();
        if (current().kind == TokenKind::Assign) {
            ++index;
            auto value = parseExpression();
            expect(TokenKind::Semicolon, "';' after assignment");
            return std::make_unique<AssignStmt>(pos, std::move(expr), std::move(value));
        }
        expect(TokenKind::Semicolon, "';' after expression");
        return std::make_unique<ExprStmt>(pos, std::move(expr));
    }

    throw XiHashError(pos, "Unexpected '" + current().text + "'");
}

ExprPtr Parser::parseExpression() {
    return parseBinary(0);
}

ExprPtr Parser::parseBinary(int level) {
    if (level == unaryLevel) {
        return parseUnary();
    }
    auto lhs = parseBinary(level + 1);
    while (true) {
        const auto op = level == 3 && current().kind == TokenKind::Identifier && current().text == "in"
            ? std::optional<BinaryOp>(BinaryOp::In)
            : binaryOpFor(current().kind, level);
        if (!op) break;
        const Token token = tokens[index++];
        auto rhs = parseBinary(level + 1);
        lhs = std::make_unique<BinaryExpr>(token.pos, *op, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

ExprPtr Parser::parseUnary() {
    if (current().kind == TokenKind::Minus || current().kind == TokenKind::Bang) {
        const Token op = tokens[index++];
        return std::make_unique<UnaryExpr>(op.pos, op.text[0], parseUnary());
    }
    if (current().kind == TokenKind::Star) {
        const Token op = tokens[index++];
        return std::make_unique<DerefExpr>(op.pos, parseUnary());
    }
    if (current().kind == TokenKind::Amp) {
        const Token op = tokens[index++];
        return std::make_unique<AddressOfExpr>(op.pos, parseUnary());
    }
    return parsePostfix(parsePrimary());
}

ExprPtr Parser::parsePostfix(ExprPtr expression) {
    while (current().kind == TokenKind::LBracket || current().kind == TokenKind::Dot ||
           current().kind == TokenKind::Arrow) {
        if (current().kind == TokenKind::LBracket) {
            const SourcePos pos = current().pos;
            ++index;
            auto subscript = parseExpression();
            expect(TokenKind::RBracket, "']' after array index");
            expression = std::make_unique<IndexExpr>(pos, std::move(expression), std::move(subscript));
            continue;
        }
        const SourcePos pos = current().pos;
        const bool isArrow = current().kind == TokenKind::Arrow;
        ++index;
        const Token member = expect(TokenKind::Identifier, "a member or method name");
        if (isArrow) {
            if (member.text != "sizeOf") {
                throw XiHashError(member.pos, "The '->' operator only supports sizeOf()");
            }
            expect(TokenKind::LParen, "'(' after sizeOf");
            expect(TokenKind::RParen, "')' after sizeOf");
            expression = std::make_unique<SizeOfExpr>(pos, std::move(expression));
            continue;
        }
        const std::string* baseName = expression->kind == ExprKind::Variable
            ? &static_cast<VariableExpr&>(*expression).name : nullptr;
        if (current().kind == TokenKind::LParen) {
            static const std::unordered_set<std::string> vectorMethods = {
                "length", "Length", "Dot", "dot", "Add", "add", "Subtract", "subtract",
                "Scale", "scale", "Normalize", "normalize", "Cross", "cross"
            };
            if (baseName && typeAliases.contains(*baseName)) {
                const Type vector = typeAliases.at(*baseName);
                const std::string suffix = vector.pointee->kind == Type::Float ? "" :
                    vector.pointee->kind == Type::Int ? "i" : vector.pointee->kind == Type::UInt ? "u" : "d";
                const std::string dim = std::to_string(vector.vectorSize);
                std::string fn;
                if (member.text == "Dot" || member.text == "dot") fn = "dot" + dim + suffix;
                else if (member.text == "Length" || member.text == "length") fn = "length" + dim + suffix;
                else if (member.text == "Add" || member.text == "add") fn = "add" + dim + suffix;
                else if (member.text == "Subtract" || member.text == "subtract") fn = "subtract" + dim + suffix;
                else if (member.text == "Scale" || member.text == "scale") fn = "scale" + dim + suffix;
                else if (member.text == "Normalize" || member.text == "normalize") fn = "normalize" + dim + suffix;
                else if (member.text == "Cross" || member.text == "cross") fn = "cross3" + suffix;
                if (!fn.empty()) {
                    expression = std::make_unique<CallExpr>(pos, "Numerals", fn, parseArgs());
                    continue;
                }
            }
            if (vectorMethods.contains(member.text)) {
                expression = std::make_unique<MethodCallExpr>(pos, std::move(expression), member.text, parseArgs());
            } else if (baseName) {
                expression = std::make_unique<CallExpr>(pos, *baseName, member.text, parseArgs());
            } else {
                throw XiHashError(pos, "A namespace call must start with a namespace name");
            }
        } else {
            expression = std::make_unique<MemberExpr>(pos, std::move(expression), member.text);
        }
    }
    return expression;
}

ExprPtr Parser::parsePrimary() {
    const Token token = current();
    switch (token.kind) {
        case TokenKind::IntLiteral: {
            ++index;
            long long value = 0;
            const auto result = std::from_chars(token.text.data(), token.text.data() + token.text.size(), value);
            if (result.ec != std::errc()) {
                throw XiHashError(token.pos, "Integer literal is too large");
            }
            return std::make_unique<IntLiteralExpr>(token.pos, value);
        }
        case TokenKind::FloatLiteral:
            ++index;
            return std::make_unique<FloatLiteralExpr>(token.pos, std::stod(token.text));
        case TokenKind::CharLiteral:
            ++index;
            return std::make_unique<CharLiteralExpr>(token.pos, token.text[0]);
        case TokenKind::StringLiteral:
            ++index;
            return std::make_unique<StringLiteralExpr>(token.pos, token.text);
        case TokenKind::LParen: {
            ++index;
            auto inner = parseExpression();
            expect(TokenKind::RParen, "')'");
            return inner;
        }
        case TokenKind::LBracket: {
            ++index;
            std::vector<ExprPtr> elements;
            if (current().kind != TokenKind::RBracket) {
                while (true) {
                    elements.push_back(parseExpression());
                    if (current().kind != TokenKind::Comma) break;
                    ++index;
                }
            }
            expect(TokenKind::RBracket, "']' after array literal");
            return std::make_unique<ArrayLiteralExpr>(token.pos, std::move(elements));
        }
        case TokenKind::Identifier:
            if (token.text == "SUCCESS" || token.text == "FAILURE") {
                ++index;
                return std::make_unique<IntLiteralExpr>(token.pos, token.text == "SUCCESS" ? 0 : 1);
            }
            ++index;
            if (token.text == "null") {
                return std::make_unique<NullLiteralExpr>(token.pos);
            }
            if (token.text == "new") {
                if (isIdentifier("unsafe")) {
                    throw XiHashError(current().pos, "'unsafe' belongs on the pointer variable, not on 'new'");
                }
                Type allocType = parseType();
                expect(TokenKind::LParen, "'(' after the type in 'new'");
                auto init = parseExpression();
                expect(TokenKind::RParen, "')'");
                return std::make_unique<NewExpr>(token.pos, std::move(allocType), std::move(init));
            }
            if (token.text == "move") {
                expect(TokenKind::LParen, "'(' after move");
                auto operand = parseExpression();
                expect(TokenKind::RParen, "')'");
                return std::make_unique<MoveExpr>(token.pos, std::move(operand));
            }
            if (token.text == "true" || token.text == "false") {
                return std::make_unique<BoolLiteralExpr>(token.pos, token.text == "true");
            }
            if (isReserved(token.text)) {
                throw XiHashError(token.pos, "Unexpected '" + token.text + "' in expression");
            }
            if (current().kind == TokenKind::LParen) {
                return std::make_unique<CallExpr>(token.pos, std::string(), token.text, parseArgs());
            }
            return std::make_unique<VariableExpr>(token.pos, token.text);
        default:
            throw XiHashError(token.pos, "Expected an expression");
    }
}

std::vector<ExprPtr> Parser::parseArgs() {
    expect(TokenKind::LParen, "'(' to call a function");
    std::vector<ExprPtr> args;
    if (current().kind != TokenKind::RParen) {
        while (true) {
            args.push_back(parseExpression());
            if (current().kind == TokenKind::Comma) {
                ++index;
                continue;
            }
            break;
        }
    }
    expect(TokenKind::RParen, "')' after arguments");
    return args;
}
