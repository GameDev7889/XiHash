#include "CodeGenerator.hpp"

#include <format>
#include <sstream>

namespace {
std::string indentOf(int level) { return std::string(static_cast<std::size_t>(level) * 4, ' '); }

std::string escapeString(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 2);
    for (const char c : value) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            case '\0': result += "\\0"; break;
            default: result += c;
        }
    }
    return result;
}

std::string escapeChar(char value) {
    switch (value) {
        case '\'': return "\\'";
        case '\\': return "\\\\";
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
        case '\0': return "\\0";
        default: return std::string(1, value);
    }
}

// Formats a double so it always parses back as a C++ floating-point literal (i.e. it contains a
// '.' or an exponent), using the shortest round-tripping representation.
std::string formatFloatLiteral(double value) {
    std::string text = std::format("{}", value);
    if (text.find_first_of(".eEnN") == std::string::npos) {
        text += ".0";
    }
    return text;
}

// `else if` is represented as an else block holding a single IfStmt; detect that so we can emit
// a natural `else if (...)` chain instead of `else { if (...) { ... } }`.
bool isElseIfChain(const std::vector<StmtPtr>& elseBody) {
    return elseBody.size() == 1 && elseBody.front()->kind == StmtKind::If;
}
}

std::string CodeGenerator::genType(const Type& type) {
    switch (type.kind) {
        case Type::Int: return "std::int32_t";
        case Type::UInt: return "std::uint32_t";
        case Type::Float: return "float";
        case Type::UFloat: return "float";
        case Type::Double: return "double";
        case Type::UDouble: return "double";
        case Type::String: return "std::string";
        case Type::Bool: return "bool";
        case Type::Char: return "char";
        case Type::Null: return "std::nullptr_t";
        case Type::Pointer:
            if (!type.cppTypeName.empty()) {
                return type.cppTypeName;
            }
            if (type.isUnsafe) {
                return genType(*type.pointee) + "*";
            }
            return "std::unique_ptr<" + genType(*type.pointee) + ">";
        case Type::Void: return "void";
    }
    return "void";
}

std::string CodeGenerator::castTo(const Type& type, const std::string& code) {
    if (!isNumeric(type)) {
        return code;
    }
    return "static_cast<" + genType(type) + ">(" + code + ")";
}

std::string CodeGenerator::genNew(const NewExpr& expr) {
    const std::string pointeeType = genType(*expr.type.pointee);
    const std::string initCode = castTo(expr.allocType, genExpr(*expr.init));
    if (expr.type.isUnsafe) {
        return "new " + pointeeType + "(" + initCode + ")";
    }
    return "std::make_unique<" + pointeeType + ">(" + initCode + ")";
}

std::string CodeGenerator::genCall(const CallExpr& expr) {
    // The namespace is a XiHash-only organizational label (the real function lives in whatever
    // namespace the included header puts it in, almost always the global namespace), so it's
    // dropped here; only the resolved function name and casted arguments are emitted.
    std::string result = (expr.xiFunction ? "xihash_fn_" : "") + expr.name + "(";
    for (std::size_t i = 0; i < expr.args.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        const std::string argument = genExpr(*expr.args[i]);
        if (expr.external && expr.paramTypes[i] == Type::String) {
            // Extern string parameters bridge to the const char* convention used by C APIs.
            result += "(" + argument + ").c_str()";
        } else {
            result += castTo(expr.paramTypes[i], argument);
        }
    }
    result += ")";
    return result;
}

std::string CodeGenerator::genExpr(const Expr& expr) {
    switch (expr.kind) {
        case ExprKind::IntLiteral:
            return std::to_string(static_cast<const IntLiteralExpr&>(expr).value);
        case ExprKind::FloatLiteral:
            return formatFloatLiteral(static_cast<const FloatLiteralExpr&>(expr).value);
        case ExprKind::StringLiteral:
            return "std::string(\"" + escapeString(static_cast<const StringLiteralExpr&>(expr).value) + "\")";
        case ExprKind::BoolLiteral:
            return static_cast<const BoolLiteralExpr&>(expr).value ? "true" : "false";
        case ExprKind::CharLiteral:
            return "'" + escapeChar(static_cast<const CharLiteralExpr&>(expr).value) + "'";
        case ExprKind::NullLiteral:
            return "nullptr";
        case ExprKind::Variable:
            return static_cast<const VariableExpr&>(expr).name;
        case ExprKind::Unary: {
            const auto& unary = static_cast<const UnaryExpr&>(expr);
            return std::string("(") + unary.op + genExpr(*unary.operand) + ")";
        }
        case ExprKind::New:
            return genNew(static_cast<const NewExpr&>(expr));
        case ExprKind::Deref:
            return "(*" + genExpr(*static_cast<const DerefExpr&>(expr).operand) + ")";
        case ExprKind::AddressOf:
            return "(&" + genExpr(*static_cast<const AddressOfExpr&>(expr).operand) + ")";
        case ExprKind::Move:
            return "std::move(" + genExpr(*static_cast<const MoveExpr&>(expr).operand) + ")";
        case ExprKind::Call:
            return genCall(static_cast<const CallExpr&>(expr));
        case ExprKind::Binary:
            return genBinary(static_cast<const BinaryExpr&>(expr));
    }
    return "";
}

std::string CodeGenerator::genBinary(const BinaryExpr& expr) {
    const std::string lhsCode = genExpr(*expr.lhs);
    const std::string rhsCode = genExpr(*expr.rhs);
    const std::string op = opSymbol(expr.op);

    if (expr.op == BinaryOp::And || expr.op == BinaryOp::Or) {
        return "(" + lhsCode + " " + op + " " + rhsCode + ")";
    }
    if (expr.operandType == Type::String) {
        const std::string left = expr.lhs->type == Type::String ? lhsCode : "xihash::toStr(" + lhsCode + ")";
        const std::string right = expr.rhs->type == Type::String ? rhsCode : "xihash::toStr(" + rhsCode + ")";
        return "(" + left + " + " + right + ")";
    }
    if (isNumeric(expr.operandType)) {
        return "(" + castTo(expr.operandType, lhsCode) + " " + op + " " + castTo(expr.operandType, rhsCode) + ")";
    }
    // Bool, Char, Pointer (and pointer-vs-null) operands already match exactly; no cast needed.
    return "(" + lhsCode + " " + op + " " + rhsCode + ")";
}

std::string CodeGenerator::genStmt(const Stmt& stmt, int indent) {
    const std::string pad = indentOf(indent);
    switch (stmt.kind) {
        case StmtKind::VarDecl: {
            const auto& decl = static_cast<const VarDeclStmt&>(stmt);
            return pad + (decl.isStatic ? "static " : "") + (decl.isConst ? "const " : "") +
                   genType(decl.declType) + " " + decl.name + " = " +
                   castTo(decl.declType, genExpr(*decl.init)) + ";\n";
        }
        case StmtKind::Assign: {
            const auto& assign = static_cast<const AssignStmt&>(stmt);
            return pad + genExpr(*assign.target) + " = " +
                   castTo(assign.target->type, genExpr(*assign.value)) + ";\n";
        }
        case StmtKind::WriteLine: {
            const auto& write = static_cast<const WriteLineStmt&>(stmt);
            const std::string function = write.newline ? "WriteLine" : "Write";
            return pad + "xihash::Console::" + function + "(" + genExpr(*write.value) + ");\n";
        }
        case StmtKind::Delete: {
            const auto& del = static_cast<const DeleteStmt&>(stmt);
            const std::string operand = genExpr(*del.operand);
            return pad + "delete " + operand + ";\n" + pad + operand + " = nullptr;\n";
        }
        case StmtKind::ExprStmt: {
            const auto& exprStmt = static_cast<const ExprStmt&>(stmt);
            return pad + genExpr(*exprStmt.expr) + ";\n";
        }
        case StmtKind::Return: {
            const auto& ret = static_cast<const ReturnStmt&>(stmt);
            return pad + "return " + castTo(ret.returnType, genExpr(*ret.value)) + ";\n";
        }
        case StmtKind::If: {
            const auto& ifStmt = static_cast<const IfStmt&>(stmt);
            std::string result = pad + "if (" + genExpr(*ifStmt.condition) + ") {\n" +
                                  genBlock(ifStmt.thenBody, indent + 1) + pad + "}";
            if (ifStmt.elseBody.empty()) {
                return result + "\n";
            }
            if (isElseIfChain(ifStmt.elseBody)) {
                // Drop the leading indentation of the nested if; it continues this line.
                std::string nested = genStmt(*ifStmt.elseBody.front(), indent);
                nested.erase(0, pad.size());
                return result + " else " + nested;
            }
            return result + " else {\n" + genBlock(ifStmt.elseBody, indent + 1) + pad + "}\n";
        }
        case StmtKind::While: {
            const auto& whileStmt = static_cast<const WhileStmt&>(stmt);
            return pad + "while (" + genExpr(*whileStmt.condition) + ") {\n" +
                   genBlock(whileStmt.body, indent + 1) + pad + "}\n";
        }
    }
    return "";
}

std::string CodeGenerator::genBlock(const std::vector<StmtPtr>& body, int indent) {
    std::string result;
    for (const auto& stmt : body) {
        result += genStmt(*stmt, indent);
    }
    return result;
}

CodeGenerator::Output CodeGenerator::generate(const Program& program) {
    const Function* main = nullptr;
    for (const auto& function : program.functions) {
        if (function.name == "main") {
            main = &function;
        }
    }

    Output output;
    output.runtimeHeader =
        "#ifndef XIHASH_RUNTIME_HPP\n"
        "#define XIHASH_RUNTIME_HPP\n"
        "\n"
        "// Generated by the XiHash compiler. Small helpers used by the generated .cpp file.\n"
        "#include <cstdint>\n"
        "#include <format>\n"
        "#include <iostream>\n"
        "#include <memory>\n"
        "#include <string>\n"
        "\n"
        "namespace xihash {\n"
        "\n"
        "// Converts a value to its textual form for string concatenation (`+`) and Console.WriteLine.\n"
        "inline std::string toStr(const std::string& value) { return value; }\n"
        "inline std::string toStr(bool value) { return value ? \"true\" : \"false\"; }\n"
        "inline std::string toStr(char value) { return std::string(1, value); }\n"
        "inline std::string toStr(std::int32_t value) { return std::to_string(value); }\n"
        "inline std::string toStr(std::uint32_t value) { return std::to_string(value); }\n"
        "inline std::string toStr(float value) { return std::format(\"{}\", value); }\n"
        "inline std::string toStr(double value) { return std::format(\"{}\", value); }\n"
        "inline std::string toStr(std::nullptr_t) { return \"null\"; }\n"
        "template <typename T>\n"
        "inline std::string toStr(T* pointer) {\n"
        "    return pointer ? std::format(\"0x{:x}\", reinterpret_cast<std::uintptr_t>(pointer)) : \"null\";\n"
        "}\n"
        "template <typename T>\n"
        "inline std::string toStr(const std::unique_ptr<T>& pointer) { return toStr(pointer.get()); }\n"
        "\n"
        "namespace Console {\n"
        "inline void Write(char value) { std::cout << value; }\n"
        "template <typename T>\n"
        "inline void WriteLine(const T& value) { std::cout << toStr(value) << '\\n'; }\n"
        "}\n"
        "\n"
        "}\n"
        "\n"
        "#endif\n";

    std::ostringstream source;
    source << "// Generated by the XiHash compiler. Do not edit; re-run XiHash on the .xi source instead.\n"
              "#include <cstdint>\n"
              "#include <memory>\n"
              "#include <string>\n"
              "#include <utility>\n"
              "\n"
              "#include \"XiHashRuntime.hpp\"\n";

    if (!program.cHeaders.empty()) {
        source << "\n// Headers requested with 'usingC <...>;'\n";
        for (const auto& header : program.cHeaders) {
            source << "#include <" << header << ">\n";
        }
    }
    source << "\n";

    for (const auto& function : program.functions) {
        source << "static " << genType(function.returnType) << " xihash_fn_" << function.name << "(";
        for (std::size_t i = 0; i < function.params.size(); ++i) {
            if (i > 0) source << ", ";
            source << genType(function.params[i].type) << " " << function.params[i].name;
        }
        source << ");\n";
    }
    source << "\n";
    for (const auto& function : program.functions) {
        source << "static " << genType(function.returnType) << " xihash_fn_" << function.name << "(";
        for (std::size_t i = 0; i < function.params.size(); ++i) {
            if (i > 0) source << ", ";
            source << genType(function.params[i].type) << " " << function.params[i].name;
        }
        source << ") {\n" << genBlock(function.body, 1) << "}\n\n";
    }
    if (main) {
        source << "int main() { return xihash_fn_main(); }\n";
    } else {
        source << "int main() { return 0; }\n";
    }

    output.source = source.str();
    return output;
}
