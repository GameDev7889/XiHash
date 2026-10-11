#include "Analyzer.hpp"

#include <unordered_set>

namespace {
int rank(const Type& type) {
    switch (type.kind) {
        case Type::Int: case Type::UInt: return 0;
        case Type::Float: case Type::UFloat: return 1;
        default: return 2;
    }
}
}

bool isConvertible(const Type& to, const Expr& expr) {
    if (to.kind == Type::Pointer) {
        return expr.type.kind == Type::Null || expr.type == to;
    }
    if (to == Type::String || to == Type::Bool || to == Type::Char) {
        return expr.type == to;
    }
    if (!isNumeric(to) || !isNumeric(expr.type)) {
        return false;
    }
    if (expr.isConst) {
        // Integer constants fit any numeric type; floating constants only floating types.
        // Range and sign are checked when the value is converted.
        return expr.type == Type::Int || isFloating(to);
    }
    if (to == expr.type) {
        return true;
    }
    return isUnsigned(to) == isUnsigned(expr.type) && rank(to) > rank(expr.type);
}

void Analyzer::registerExterns(Program& program) {
    for (const auto& block : program.externBlocks) {
        if (!externBlocks.emplace(block.name, &block).second) {
            throw XiHashError(block.pos, "Extern block '" + block.name + "' is already declared");
        }
        std::unordered_set<std::string> functionNames;
        for (const auto& function : block.functions) {
            if (!functionNames.insert(function.name).second) {
                throw XiHashError(function.pos, "Function '" + function.name + "' is already declared in extern '" +
                                                     block.name + "'");
            }
            const auto requireRawPointer = [&](const Type& type, const std::string& what) {
                if (isPointer(type) && !type.isUnsafe) {
                    throw XiHashError(function.pos, std::string("Extern function '") + function.name + "' " + what +
                                                         " must use an 'unsafe' pointer; C functions don't understand ownership");
                }
            };
            requireRawPointer(function.returnType, "return type");
            for (const auto& param : function.params) {
                requireRawPointer(param.type, "parameter '" + param.name + "'");
            }
        }
    }
    for (const auto& [name, pos] : program.openedNamespaces) {
        if (!externBlocks.contains(name)) {
            throw XiHashError(pos, "Unknown namespace '" + name + "'; declare it with 'extern " + name + " { ... }'");
        }
        openNamespaces.push_back(name);
    }
}

void Analyzer::registerFunctions(Program& program) {
    for (const auto& function : program.functions) {
        if (!xiFunctions.emplace(function.name, &function).second) {
            throw XiHashError(function.pos, "Function '" + function.name + "' is already defined");
        }
        std::unordered_set<std::string> parameterNames;
        for (const auto& param : function.params) {
            if (!parameterNames.insert(param.name).second) {
                throw XiHashError(function.pos, "Parameter '" + param.name + "' is already declared");
            }
            if (param.type == Type::Void) {
                throw XiHashError(function.pos, "Function parameters cannot have type void");
            }
        }
    }
}

void Analyzer::analyze(Program& program) {
    registerExterns(program);
    registerFunctions(program);
    const Function* entry = nullptr;
    for (auto& function : program.functions) {
        analyzeFunction(function);
        if (function.name == "main") {
            entry = &function;
        }
    }
    if (!entry) {
        throw XiHashError({1, 1}, "Program has no 'main' function");
    }
    if (entry->returnType != Type::Int) {
        throw XiHashError(entry->pos, "'main' must return int");
    }
    if (!entry->params.empty()) {
        throw XiHashError(entry->pos, "'main' must not have parameters");
    }
}

void Analyzer::analyzeFunction(Function& function) {
    scopes.clear();
    scopes.emplace_back();
    for (const auto& param : function.params) {
        scopes.back().emplace(param.name, VariableInfo{param.type, false});
    }
    analyzeBlock(function.body, function);
}

void Analyzer::analyzeBlock(std::vector<StmtPtr>& body, const Function& function) {
    scopes.emplace_back();
    for (auto& stmt : body) {
        analyzeStatement(*stmt, function);
    }
    scopes.pop_back();
}

std::optional<Type> Analyzer::lookup(const std::string& name) const {
    for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
        if (const auto variable = scope->find(name); variable != scope->end()) {
            return variable->second.type;
        }
    }
    return std::nullopt;
}

void Analyzer::requireConvertible(const Type& to, const Expr& expr, const char* context) {
    if (!isConvertible(to, expr)) {
        throw XiHashError(expr.pos, std::string("Cannot convert ") + typeName(expr.type) + " to " +
                                        typeName(to) + " in " + context);
    }
}

bool Analyzer::isLvalue(const Expr& expr) {
    return expr.kind == ExprKind::Variable || expr.kind == ExprKind::Deref;
}

void Analyzer::analyzeInitializer(ExprPtr& expr, const Type& target, const char* context) {
    if (expr->kind == ExprKind::New) {
        analyzeNew(static_cast<NewExpr&>(*expr), target, context);
        return;
    }
    if (expr->kind == ExprKind::Move) {
        analyzeMove(static_cast<MoveExpr&>(*expr));
        requireConvertible(target, *expr, context);
        return;
    }
    analyzeExpr(*expr);
    if (isSafePointer(target) && isSafePointer(expr->type) && isLvalue(*expr)) {
        throw XiHashError(expr->pos, "A unique pointer cannot be copied; use move(...) to transfer ownership");
    }
    requireConvertible(target, *expr, context);
}

void Analyzer::analyzeNew(NewExpr& expr, const Type& target, const char* context) {
    if (target.kind != Type::Pointer) {
        throw XiHashError(expr.pos, std::string("Cannot convert ") + typeName(makePointer(expr.allocType, false)) +
                                        " to " + typeName(target) + " in " + context);
    }
    expr.type = makePointer(expr.allocType, target.isUnsafe);
    if (expr.type != target) {
        throw XiHashError(expr.pos, std::string("Cannot convert ") + typeName(expr.type) + " to " +
                                        typeName(target) + " in " + context);
    }
    analyzeInitializer(expr.init, expr.allocType, "new");
}

void Analyzer::analyzeMove(MoveExpr& expr) {
    analyzeExpr(*expr.operand);
    if (!isLvalue(*expr.operand) || !isSafePointer(expr.operand->type)) {
        throw XiHashError(expr.pos, "move(...) requires a variable holding a safe (unique) pointer");
    }
    expr.type = expr.operand->type;
}

void Analyzer::analyzeStatement(Stmt& stmt, const Function& function) {
    switch (stmt.kind) {
        case StmtKind::VarDecl: {
            auto& decl = static_cast<VarDeclStmt&>(stmt);
            if (lookup(decl.name)) {
                throw XiHashError(decl.pos, "Variable '" + decl.name + "' is already declared");
            }
            analyzeInitializer(decl.init, decl.declType, "declaration");
            if (decl.isConst && !decl.init->isConst) {
                throw XiHashError(decl.init->pos, "A const initializer must be a compile-time constant expression");
            }
            scopes.back()[decl.name] = VariableInfo{decl.declType, decl.isConst};
            break;
        }
        case StmtKind::Assign: {
            auto& assign = static_cast<AssignStmt&>(stmt);
            analyzeExpr(*assign.target);
            if (assign.target->kind == ExprKind::Variable) {
                const auto& name = static_cast<const VariableExpr&>(*assign.target).name;
                for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
                    const auto variable = scope->find(name);
                    if (variable != scope->end()) {
                        if (variable->second.isConst) {
                            throw XiHashError(assign.target->pos, "Cannot assign to const variable '" + name + "'");
                        }
                        break;
                    }
                }
            }
            analyzeInitializer(assign.value, assign.target->type, "assignment");
            break;
        }
        case StmtKind::Delete: {
            auto& del = static_cast<DeleteStmt&>(stmt);
            analyzeExpr(*del.operand);
            const Type& type = del.operand->type;
            if (isSafePointer(type)) {
                throw XiHashError(del.pos, "'delete' requires an unsafe pointer; safe pointers are freed automatically");
            }
            if (!isPointer(type)) {
                throw XiHashError(del.pos, "'delete' requires an unsafe pointer, got " + typeName(type));
            }
            break;
        }
        case StmtKind::WriteLine:
        {
            auto& write = static_cast<WriteLineStmt&>(stmt);
            analyzeExpr(*write.value);
            if (write.value->type == Type::Void) {
                throw XiHashError(write.value->pos, "Cannot print a value of type void");
            }
            if (!write.newline && write.value->type != Type::Char) {
                throw XiHashError(write.value->pos, std::string("Console.Write requires char, got ") +
                                                        typeName(write.value->type));
            }
            break;
        }
        case StmtKind::If: {
            auto& ifStmt = static_cast<IfStmt&>(stmt);
            analyzeExpr(*ifStmt.condition);
            if (ifStmt.condition->type != Type::Bool) {
                throw XiHashError(ifStmt.condition->pos, std::string("Condition must be bool, got ") +
                                                             typeName(ifStmt.condition->type));
            }
            analyzeBlock(ifStmt.thenBody, function);
            analyzeBlock(ifStmt.elseBody, function);
            break;
        }
        case StmtKind::While: {
            auto& whileStmt = static_cast<WhileStmt&>(stmt);
            analyzeExpr(*whileStmt.condition);
            if (whileStmt.condition->type != Type::Bool) {
                throw XiHashError(whileStmt.condition->pos, std::string("Condition must be bool, got ") +
                                                               typeName(whileStmt.condition->type));
            }
            analyzeBlock(whileStmt.body, function);
            break;
        }
        case StmtKind::Return: {
            auto& ret = static_cast<ReturnStmt&>(stmt);
            analyzeExpr(*ret.value);
            ret.returnType = function.returnType;
            requireConvertible(function.returnType, *ret.value, "return");
            break;
        }
        case StmtKind::ExprStmt: {
            auto& exprStmt = static_cast<ExprStmt&>(stmt);
            analyzeExpr(*exprStmt.expr);
            break;
        }
    }
}

void Analyzer::analyzeExpr(Expr& expr) {
    switch (expr.kind) {
        case ExprKind::IntLiteral:
            expr.type = Type::Int;
            expr.isConst = true;
            break;
        case ExprKind::FloatLiteral:
            expr.type = Type::Double;
            expr.isConst = true;
            break;
        case ExprKind::StringLiteral:
            expr.type = Type::String;
            expr.isConst = true;
            break;
        case ExprKind::BoolLiteral:
            expr.type = Type::Bool;
            expr.isConst = true;
            break;
        case ExprKind::CharLiteral:
            expr.type = Type::Char;
            expr.isConst = true;
            break;
        case ExprKind::NullLiteral:
            expr.type = Type::Null;
            expr.isConst = true;
            break;
        case ExprKind::New:
            throw XiHashError(expr.pos, "'new' can only initialize a pointer variable, assign to a pointer, or appear inside another 'new'");
        case ExprKind::Move:
            throw XiHashError(expr.pos, "move(...) can only initialize a pointer variable or assign to a pointer");
        case ExprKind::Deref: {
            auto& deref = static_cast<DerefExpr&>(expr);
            analyzeExpr(*deref.operand);
            if (deref.operand->type.kind == Type::Null) {
                throw XiHashError(expr.pos, "Cannot dereference null");
            }
            if (!isPointer(deref.operand->type)) {
                throw XiHashError(expr.pos, "Cannot dereference " + typeName(deref.operand->type) + ", it is not a pointer");
            }
            expr.type = *deref.operand->type.pointee;
            break;
        }
        case ExprKind::AddressOf: {
            auto& addressOf = static_cast<AddressOfExpr&>(expr);
            analyzeExpr(*addressOf.operand);
            if (!isLvalue(*addressOf.operand)) {
                throw XiHashError(expr.pos, "Can only take the address of a variable or a dereferenced pointer");
            }
            expr.type = makePointer(addressOf.operand->type, true);
            break;
        }
        case ExprKind::Variable: {
            const auto& name = static_cast<VariableExpr&>(expr).name;
            bool found = false;
            for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
                const auto variable = scope->find(name);
                if (variable != scope->end()) {
                    expr.type = variable->second.type;
                    expr.isConst = variable->second.isConst;
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw XiHashError(expr.pos, "Unknown variable '" + name + "'");
            }
            break;
        }
        case ExprKind::Unary: {
            auto& unary = static_cast<UnaryExpr&>(expr);
            analyzeExpr(*unary.operand);
            const Expr& operand = *unary.operand;
            if (unary.op == '!') {
                if (operand.type != Type::Bool) {
                    throw XiHashError(expr.pos, std::string("Operator '!' requires bool, got ") +
                                                    typeName(operand.type));
                }
                expr.type = Type::Bool;
                expr.isConst = operand.isConst;
                break;
            }
            if (!isNumeric(operand.type)) {
                throw XiHashError(expr.pos, std::string("Cannot negate ") + typeName(operand.type));
            }
            if (isUnsigned(operand.type)) {
                throw XiHashError(expr.pos, std::string("Cannot negate unsigned type ") + typeName(operand.type));
            }
            expr.type = operand.type;
            expr.isConst = operand.isConst;
            break;
        }
        case ExprKind::Binary:
            analyzeBinary(static_cast<BinaryExpr&>(expr));
            break;
        case ExprKind::Call:
            analyzeCall(static_cast<CallExpr&>(expr));
            break;
    }
}

void Analyzer::analyzeCall(CallExpr& expr) {
    if (expr.ns.empty()) {
        const auto xiFunction = xiFunctions.find(expr.name);
        if (xiFunction != xiFunctions.end()) {
            const Function& function = *xiFunction->second;
            if (expr.args.size() != function.params.size()) {
                throw XiHashError(expr.pos, "Function '" + function.name + "' expects " +
                                                 std::to_string(function.params.size()) + " argument(s), got " +
                                                 std::to_string(expr.args.size()));
            }
            expr.xiFunction = true;
            expr.paramTypes.clear();
            for (std::size_t i = 0; i < expr.args.size(); ++i) {
                analyzeExpr(*expr.args[i]);
                requireConvertible(function.params[i].type, *expr.args[i], "function call argument");
                expr.paramTypes.push_back(function.params[i].type);
            }
            expr.type = function.returnType;
            return;
        }
    }
    const ExternFunction* function = nullptr;
    if (!expr.ns.empty()) {
        const auto blockIt = externBlocks.find(expr.ns);
        if (blockIt == externBlocks.end()) {
            throw XiHashError(expr.pos, "Unknown namespace '" + expr.ns + "'");
        }
        for (const auto& candidate : blockIt->second->functions) {
            if (candidate.name == expr.name) {
                function = &candidate;
                break;
            }
        }
        if (!function) {
            throw XiHashError(expr.pos, "Unknown function '" + expr.ns + "." + expr.name + "'");
        }
    } else {
        std::vector<const ExternFunction*> matches;
        for (const auto& ns : openNamespaces) {
            const auto blockIt = externBlocks.find(ns);
            if (blockIt == externBlocks.end()) {
                continue;
            }
            for (const auto& candidate : blockIt->second->functions) {
                if (candidate.name == expr.name) {
                    matches.push_back(&candidate);
                }
            }
        }
        if (matches.empty()) {
            throw XiHashError(expr.pos, "Unknown function '" + expr.name +
                                             "'; qualify it with a namespace or add 'using namespace ...;'");
        }
        if (matches.size() > 1) {
            throw XiHashError(expr.pos, "Call to '" + expr.name + "' is ambiguous between multiple opened namespaces");
        }
        function = matches.front();
    }

    if (expr.args.size() != function->params.size()) {
        throw XiHashError(expr.pos, "Function '" + function->name + "' expects " +
                                         std::to_string(function->params.size()) + " argument(s), got " +
                                         std::to_string(expr.args.size()));
    }
    expr.name = function->name;
    expr.external = true;
    for (std::size_t i = 0; i < expr.args.size(); ++i) {
        analyzeExpr(*expr.args[i]);
        requireConvertible(function->params[i].type, *expr.args[i], "function call argument");
        expr.paramTypes.push_back(function->params[i].type);
    }
    expr.type = function->returnType;
}

Type Analyzer::unifyNumeric(const BinaryExpr& expr) {
    const Expr& lhs = *expr.lhs;
    const Expr& rhs = *expr.rhs;
    if (lhs.type == rhs.type) {
        return lhs.type;
    }
    if (lhs.isConst && rhs.isConst) {
        return Type::Double;
    }
    if (lhs.isConst) {
        requireConvertible(rhs.type, lhs, "expression");
        return rhs.type;
    }
    if (rhs.isConst) {
        requireConvertible(lhs.type, rhs, "expression");
        return lhs.type;
    }
    if (isUnsigned(lhs.type) != isUnsigned(rhs.type)) {
        throw XiHashError(expr.pos, std::string("Cannot mix signed and unsigned types (") + typeName(lhs.type) +
                                        " and " + typeName(rhs.type) + ") without a constant");
    }
    return rank(lhs.type) >= rank(rhs.type) ? lhs.type : rhs.type;
}

void Analyzer::analyzeBinary(BinaryExpr& expr) {
    analyzeExpr(*expr.lhs);
    analyzeExpr(*expr.rhs);
    const Expr& lhs = *expr.lhs;
    const Expr& rhs = *expr.rhs;
    const auto fail = [&](const std::string& message) {
        throw XiHashError(expr.pos, message);
    };

    switch (expr.op) {
        case BinaryOp::And:
        case BinaryOp::Or:
            if (lhs.type != Type::Bool || rhs.type != Type::Bool) {
                fail(std::string("Operator '") + opSymbol(expr.op) + "' requires bool operands, got " +
                     typeName(lhs.type) + " and " + typeName(rhs.type));
            }
            expr.operandType = Type::Bool;
            expr.type = Type::Bool;
            expr.isConst = lhs.isConst && rhs.isConst;
            return;

        case BinaryOp::Eq:
        case BinaryOp::Ne:
        case BinaryOp::Lt:
        case BinaryOp::Le:
        case BinaryOp::Gt:
        case BinaryOp::Ge: {
            const bool equality = expr.op == BinaryOp::Eq || expr.op == BinaryOp::Ne;
            const auto isPointerLike = [](const Type& type) {
                return type.kind == Type::Pointer || type.kind == Type::Null;
            };
            if (isPointerLike(lhs.type) || isPointerLike(rhs.type)) {
                const bool bothPointers = lhs.type.kind == Type::Pointer && rhs.type.kind == Type::Pointer;
                if (!isPointerLike(lhs.type) || !isPointerLike(rhs.type) || (bothPointers && lhs.type != rhs.type)) {
                    fail("Cannot compare " + typeName(lhs.type) + " with " + typeName(rhs.type));
                }
                if (!equality) {
                    fail(std::string("Operator '") + opSymbol(expr.op) + "' cannot be used with pointers");
                }
                expr.operandType = Type::Pointer;
                expr.type = Type::Bool;
                return;
            }
            const auto isNonNumeric = [](Type type) {
                return type == Type::Bool || type == Type::String || type == Type::Char;
            };
            if (isNonNumeric(lhs.type) || isNonNumeric(rhs.type)) {
                if (lhs.type != rhs.type) {
                    fail(std::string("Cannot compare ") + typeName(lhs.type) + " with " + typeName(rhs.type));
                }
                if (!equality && lhs.type != Type::Char) {
                    fail(std::string("Operator '") + opSymbol(expr.op) + "' cannot be used with " +
                         typeName(lhs.type));
                }
                expr.operandType = lhs.type;
            } else {
                expr.operandType = unifyNumeric(expr);
            }
            expr.type = Type::Bool;
            expr.isConst = lhs.isConst && rhs.isConst;
            return;
        }

        default:
            break;
    }

    if (lhs.type == Type::String || rhs.type == Type::String) {
        if (expr.op != BinaryOp::Add) {
            fail(std::string("Operator '") + opSymbol(expr.op) + "' cannot be used with string");
        }
        expr.operandType = Type::String;
        expr.type = Type::String;
        expr.isConst = lhs.isConst && rhs.isConst;
        return;
    }
    if (!isNumeric(lhs.type) || !isNumeric(rhs.type)) {
        fail(std::string("Operator '") + opSymbol(expr.op) + "' cannot be used with " +
             typeName(isNumeric(lhs.type) ? rhs.type : lhs.type));
    }

    expr.operandType = unifyNumeric(expr);
    expr.type = expr.operandType;
    expr.isConst = lhs.isConst && rhs.isConst;
}
