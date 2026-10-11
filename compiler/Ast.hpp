#ifndef XIHASH_AST_H
#define XIHASH_AST_H

#include <memory>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct SourcePos {
    int line = 1;
    int column = 1;
};

class XiHashError : public std::runtime_error {
public:
    XiHashError(SourcePos position, const std::string& message, std::string sourceFile = {})
        : std::runtime_error(message), position(position), sourceFile(std::move(sourceFile)) {}

    SourcePos position;
    std::string sourceFile;
};

// A type is a base kind, or a pointer to another type. `isUnsafe` marks a raw pointer that must
// be freed manually with `delete`; a pointer without it is a unique owner that frees automatically.
struct Type {
    enum Kind { Int, UInt, Float, UFloat, Double, UDouble, String, Bool, Char, Null, Pointer, Void };

    Type(Kind kind = Void) : kind(kind) {}

    Kind kind;
    bool isUnsafe = false;
    std::shared_ptr<const Type> pointee;
    std::string cppTypeName;

    friend bool operator==(const Type& a, const Type& b) {
        if (a.kind != b.kind) {
            return false;
        }
        if (a.kind != Pointer) {
            return true;
        }
        return a.isUnsafe == b.isUnsafe && a.cppTypeName == b.cppTypeName && *a.pointee == *b.pointee;
    }
};

inline Type makePointer(const Type& pointee, bool isUnsafe) {
    Type type(Type::Pointer);
    type.isUnsafe = isUnsafe;
    type.pointee = std::make_shared<const Type>(pointee);
    return type;
}

inline std::string typeName(const Type& type) {
    switch (type.kind) {
        case Type::Int: return "int";
        case Type::UInt: return "uint";
        case Type::Float: return "float";
        case Type::UFloat: return "ufloat";
        case Type::Double: return "double";
        case Type::UDouble: return "udouble";
        case Type::String: return "string";
        case Type::Bool: return "bool";
        case Type::Char: return "char";
        case Type::Null: return "null";
        case Type::Pointer: return (type.isUnsafe ? "unsafe " : "") + typeName(*type.pointee) + "*";
        case Type::Void: return "void";
    }
    return "?";
}

inline std::optional<Type> typeFromName(std::string_view name) {
    for (const Type::Kind kind : {Type::Int, Type::UInt, Type::Float, Type::UFloat, Type::Double,
                                  Type::UDouble, Type::String, Type::Bool, Type::Char, Type::Void}) {
        if (name == typeName(kind)) {
            return Type(kind);
        }
    }
    return std::nullopt;
}

inline bool isNumeric(const Type& type) {
    switch (type.kind) {
        case Type::String: case Type::Bool: case Type::Char:
        case Type::Null: case Type::Pointer: case Type::Void:
            return false;
        default:
            return true;
    }
}
inline bool isUnsigned(const Type& type) {
    return type == Type::UInt || type == Type::UFloat || type == Type::UDouble;
}
inline bool isFloating(const Type& type) {
    return type == Type::Float || type == Type::UFloat || type == Type::Double || type == Type::UDouble;
}
inline bool isPointer(const Type& type) { return type.kind == Type::Pointer; }
inline bool isSafePointer(const Type& type) { return type.kind == Type::Pointer && !type.isUnsafe; }

enum class ExprKind { IntLiteral, FloatLiteral, StringLiteral, BoolLiteral, CharLiteral, NullLiteral, Variable, Unary, Binary,
                      New, Deref, AddressOf, Move, Call };

// `type` and `isConst` are filled in by the Analyzer. A constant expression is built only from
// literals and may be implicitly converted to any compatible numeric type.
struct Expr {
    Expr(ExprKind kind, SourcePos pos) : kind(kind), pos(pos) {}
    virtual ~Expr() = default;

    ExprKind kind;
    SourcePos pos;
    Type type = Type::Void;
    bool isConst = false;
};
using ExprPtr = std::unique_ptr<Expr>;

struct IntLiteralExpr : Expr {
    IntLiteralExpr(SourcePos pos, long long value) : Expr(ExprKind::IntLiteral, pos), value(value) {}
    long long value;
};

struct FloatLiteralExpr : Expr {
    FloatLiteralExpr(SourcePos pos, double value) : Expr(ExprKind::FloatLiteral, pos), value(value) {}
    double value;
};

struct StringLiteralExpr : Expr {
    StringLiteralExpr(SourcePos pos, std::string value)
        : Expr(ExprKind::StringLiteral, pos), value(std::move(value)) {}
    std::string value;
};

struct BoolLiteralExpr : Expr {
    BoolLiteralExpr(SourcePos pos, bool value) : Expr(ExprKind::BoolLiteral, pos), value(value) {}
    bool value;
};

struct CharLiteralExpr : Expr {
    CharLiteralExpr(SourcePos pos, char value) : Expr(ExprKind::CharLiteral, pos), value(value) {}
    char value;
};

struct NullLiteralExpr : Expr {
    explicit NullLiteralExpr(SourcePos pos) : Expr(ExprKind::NullLiteral, pos) {}
};

struct VariableExpr : Expr {
    VariableExpr(SourcePos pos, std::string name) : Expr(ExprKind::Variable, pos), name(std::move(name)) {}
    std::string name;
};

struct UnaryExpr : Expr {
    UnaryExpr(SourcePos pos, char op, ExprPtr operand)
        : Expr(ExprKind::Unary, pos), op(op), operand(std::move(operand)) {}
    char op;
    ExprPtr operand;
};

// `new T(init)`: allocates a T on the heap. Whether the resulting pointer is safe or unsafe is decided
// by the pointer it initializes. `type` is set by the Analyzer.
struct NewExpr : Expr {
    NewExpr(SourcePos pos, Type allocType, ExprPtr init)
        : Expr(ExprKind::New, pos), allocType(std::move(allocType)), init(std::move(init)) {}
    Type allocType;
    ExprPtr init;
};

// `*p`
struct DerefExpr : Expr {
    DerefExpr(SourcePos pos, ExprPtr operand) : Expr(ExprKind::Deref, pos), operand(std::move(operand)) {}
    ExprPtr operand;
};

// `&x`: always produces an unsafe pointer.
struct AddressOfExpr : Expr {
    AddressOfExpr(SourcePos pos, ExprPtr operand) : Expr(ExprKind::AddressOf, pos), operand(std::move(operand)) {}
    ExprPtr operand;
};

// `move(p)`: transfers ownership out of a safe pointer, leaving it null.
struct MoveExpr : Expr {
    MoveExpr(SourcePos pos, ExprPtr operand) : Expr(ExprKind::Move, pos), operand(std::move(operand)) {}
    ExprPtr operand;
};

// A call to an `extern`-declared C/C++ function: `name(args)` (unqualified, requires a
// `using namespace ns;`) or `ns.name(args)` (qualified). `ns` is empty for an unqualified call.
// `paramTypes` (one per argument) and the resolved `name` are filled in by the Analyzer so the
// CodeGenerator can cast each argument to its real parameter type without re-resolving the call.
struct CallExpr : Expr {
    CallExpr(SourcePos pos, std::string ns, std::string name, std::vector<ExprPtr> args)
        : Expr(ExprKind::Call, pos), ns(std::move(ns)), name(std::move(name)), args(std::move(args)) {}
    std::string ns;
    std::string name;
    std::vector<ExprPtr> args;
    std::vector<Type> paramTypes;
    bool external = false;
    bool xiFunction = false;
};

enum class BinaryOp { Add, Sub, Mul, Div, Eq, Ne, Lt, Le, Gt, Ge, And, Or };

inline const char* opSymbol(BinaryOp op) {
    switch (op) {
        case BinaryOp::Add: return "+";
        case BinaryOp::Sub: return "-";
        case BinaryOp::Mul: return "*";
        case BinaryOp::Div: return "/";
        case BinaryOp::Eq: return "==";
        case BinaryOp::Ne: return "!=";
        case BinaryOp::Lt: return "<";
        case BinaryOp::Le: return "<=";
        case BinaryOp::Gt: return ">";
        case BinaryOp::Ge: return ">=";
        case BinaryOp::And: return "&&";
        case BinaryOp::Or: return "||";
    }
    return "?";
}

struct BinaryExpr : Expr {
    BinaryExpr(SourcePos pos, BinaryOp op, ExprPtr lhs, ExprPtr rhs)
        : Expr(ExprKind::Binary, pos), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
    BinaryOp op;
    ExprPtr lhs;
    ExprPtr rhs;
    // Type both operands are converted to before the operation (set by the Analyzer).
    Type operandType = Type::Void;
};

enum class StmtKind { VarDecl, Assign, WriteLine, Return, If, While, Delete, ExprStmt };

struct Stmt {
    Stmt(StmtKind kind, SourcePos pos) : kind(kind), pos(pos) {}
    virtual ~Stmt() = default;

    StmtKind kind;
    SourcePos pos;
};
using StmtPtr = std::unique_ptr<Stmt>;

struct VarDeclStmt : Stmt {
    VarDeclStmt(SourcePos pos, Type declType, std::string name, ExprPtr init, bool isConst, bool isStatic)
        : Stmt(StmtKind::VarDecl, pos), declType(declType), name(std::move(name)), init(std::move(init)),
          isConst(isConst), isStatic(isStatic) {}
    Type declType;
    std::string name;
    ExprPtr init;
    bool isConst;
    bool isStatic;
};

struct AssignStmt : Stmt {
    // `target` is a variable or a dereference (`*p = ...`).
    AssignStmt(SourcePos pos, ExprPtr target, ExprPtr value)
        : Stmt(StmtKind::Assign, pos), target(std::move(target)), value(std::move(value)) {}
    ExprPtr target;
    ExprPtr value;
};

struct WriteLineStmt : Stmt {
    WriteLineStmt(SourcePos pos, ExprPtr value, bool newline)
        : Stmt(StmtKind::WriteLine, pos), value(std::move(value)), newline(newline) {}
    ExprPtr value;
    bool newline; // false for Console.Write, which accepts only char
};

struct DeleteStmt : Stmt {
    DeleteStmt(SourcePos pos, ExprPtr operand) : Stmt(StmtKind::Delete, pos), operand(std::move(operand)) {}
    ExprPtr operand;
};

// A call used for its side effect, with the result discarded (e.g. `srand(42);`).
struct ExprStmt : Stmt {
    ExprStmt(SourcePos pos, ExprPtr expr) : Stmt(StmtKind::ExprStmt, pos), expr(std::move(expr)) {}
    ExprPtr expr;
};

struct ReturnStmt : Stmt {
    ReturnStmt(SourcePos pos, ExprPtr value) : Stmt(StmtKind::Return, pos), value(std::move(value)) {}
    ExprPtr value;
    Type returnType = Type::Int;
};

// `else if` is represented as an else block holding a single IfStmt.
struct IfStmt : Stmt {
    IfStmt(SourcePos pos, ExprPtr condition, std::vector<StmtPtr> thenBody, std::vector<StmtPtr> elseBody)
        : Stmt(StmtKind::If, pos), condition(std::move(condition)),
          thenBody(std::move(thenBody)), elseBody(std::move(elseBody)) {}
    ExprPtr condition;
    std::vector<StmtPtr> thenBody;
    std::vector<StmtPtr> elseBody;
};

struct WhileStmt : Stmt {
    WhileStmt(SourcePos pos, ExprPtr condition, std::vector<StmtPtr> body)
        : Stmt(StmtKind::While, pos), condition(std::move(condition)), body(std::move(body)) {}
    ExprPtr condition;
    std::vector<StmtPtr> body;
};

struct ExternParam {
    Type type;
    std::string name;
};

struct Function {
    SourcePos pos;
    Type returnType = Type::Int;
    std::string name;
    std::vector<ExternParam> params;
    std::vector<StmtPtr> body;
};

// One function signature inside an `extern` block, e.g. `double sqrt(double x);`.
struct ExternFunction {
    SourcePos pos;
    Type returnType;
    std::string name;
    std::vector<ExternParam> params;
};

// `extern name { ... }`: a group of C/C++ function signatures, callable as `name.function(...)`
// or, after `using namespace name;`, as `function(...)`.
struct ExternBlock {
    SourcePos pos;
    std::string name;
    std::vector<ExternFunction> functions;
};

struct Program {
    std::vector<std::pair<std::string, SourcePos>> imports;
    std::vector<std::string> cHeaders;         // from `importC <header>;`
    std::vector<std::filesystem::path> cSources;
    std::vector<std::string> linkLibraries;    // from `usingL <library>;`
    std::vector<ExternBlock> externBlocks;     // from `extern name { ... }`
    std::vector<std::pair<std::string, SourcePos>> openedNamespaces; // from `using namespace name;`
    std::vector<Function> functions;
};

#endif
