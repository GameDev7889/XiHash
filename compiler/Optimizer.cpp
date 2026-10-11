#include "Optimizer.hpp"

#include <algorithm>

namespace {
bool isLiteral(const Expr& expr) {
    switch (expr.kind) {
        case ExprKind::IntLiteral:
        case ExprKind::FloatLiteral:
        case ExprKind::StringLiteral:
        case ExprKind::BoolLiteral:
        case ExprKind::CharLiteral:
            return true;
        default:
            return false;
    }
}

ExprPtr makeLiteral(const Value& value, const Expr& original) {
    ExprPtr literal;
    switch (value.type.kind) {
        case Type::Int: literal = std::make_unique<IntLiteralExpr>(original.pos, value.integer); break;
        case Type::Double: literal = std::make_unique<FloatLiteralExpr>(original.pos, value.real); break;
        case Type::String: literal = std::make_unique<StringLiteralExpr>(original.pos, value.text); break;
        case Type::Bool: literal = std::make_unique<BoolLiteralExpr>(original.pos, value.integer != 0); break;
        case Type::Char: literal = std::make_unique<CharLiteralExpr>(original.pos, static_cast<char>(value.integer)); break;
        default: return nullptr;
    }
    literal->type = original.type;
    literal->isConst = original.isConst;
    return literal;
}

bool hasTopLevelDeclaration(const std::vector<StmtPtr>& body) {
    return std::any_of(body.begin(), body.end(), [](const StmtPtr& stmt) {
        return stmt->kind == StmtKind::VarDecl;
    });
}
}

OptimizerStats Optimizer::optimize(Program& program) {
    stats = {};
    for (auto& function : program.functions) {
        optimizeBlock(function.body);
    }
    return stats;
}

void Optimizer::tryFold(ExprPtr& expr) {
    try {
        const Value value = evaluator.evaluate(*expr);
        if (value.type != expr->type) {
            return;
        }
        if (auto literal = makeLiteral(value, *expr)) {
            expr = std::move(literal);
            ++stats.foldedExpressions;
        }
    } catch (const XiHashError&) {
        // Leave the expression alone so the error is reported when the program runs.
    }
}

void Optimizer::simplifyLogical(ExprPtr& expr) {
    auto& binary = static_cast<BinaryExpr&>(*expr);
    if (binary.lhs->kind != ExprKind::BoolLiteral) {
        return;
    }
    const bool left = static_cast<const BoolLiteralExpr&>(*binary.lhs).value;
    const bool decidedByLeft = binary.op == BinaryOp::And ? !left : left;
    ExprPtr result = decidedByLeft ? std::move(binary.lhs) : std::move(binary.rhs);
    expr = std::move(result);
    ++stats.foldedExpressions;
}

void Optimizer::optimizeExpr(ExprPtr& expr) {
    switch (expr->kind) {
        case ExprKind::Unary: {
            auto& unary = static_cast<UnaryExpr&>(*expr);
            optimizeExpr(unary.operand);
            if (isLiteral(*unary.operand)) {
                tryFold(expr);
            }
            break;
        }
        case ExprKind::New:
            optimizeExpr(static_cast<NewExpr&>(*expr).init);
            break;
        case ExprKind::Deref:
            optimizeExpr(static_cast<DerefExpr&>(*expr).operand);
            break;
        case ExprKind::Call: {
            auto& call = static_cast<CallExpr&>(*expr);
            for (auto& arg : call.args) {
                optimizeExpr(arg);
            }
            break;
        }
        case ExprKind::Binary: {
            auto& binary = static_cast<BinaryExpr&>(*expr);
            optimizeExpr(binary.lhs);
            optimizeExpr(binary.rhs);
            if (isLiteral(*binary.lhs) && isLiteral(*binary.rhs)) {
                tryFold(expr);
            } else if (binary.op == BinaryOp::And || binary.op == BinaryOp::Or) {
                simplifyLogical(expr);
            }
            break;
        }
        default:
            break;
    }
}

void Optimizer::optimizeBlock(std::vector<StmtPtr>& body) {
    std::vector<StmtPtr> result;
    bool terminated = false;

    const auto append = [&](StmtPtr stmt) {
        if (terminated) {
            ++stats.removedStatements;
            return;
        }
        terminated = stmt->kind == StmtKind::Return;
        result.push_back(std::move(stmt));
    };

    for (auto& stmt : body) {
        switch (stmt->kind) {
            case StmtKind::VarDecl:
                optimizeExpr(static_cast<VarDeclStmt&>(*stmt).init);
                break;
            case StmtKind::Assign:
                optimizeExpr(static_cast<AssignStmt&>(*stmt).value);
                break;
            case StmtKind::WriteLine:
                optimizeExpr(static_cast<WriteLineStmt&>(*stmt).value);
                break;
            case StmtKind::Return:
                optimizeExpr(static_cast<ReturnStmt&>(*stmt).value);
                break;
            case StmtKind::Delete:
                optimizeExpr(static_cast<DeleteStmt&>(*stmt).operand);
                break;
            case StmtKind::ExprStmt:
                optimizeExpr(static_cast<ExprStmt&>(*stmt).expr);
                break;
            case StmtKind::If: {
                auto& ifStmt = static_cast<IfStmt&>(*stmt);
                optimizeExpr(ifStmt.condition);
                optimizeBlock(ifStmt.thenBody);
                optimizeBlock(ifStmt.elseBody);
                if (ifStmt.condition->kind == ExprKind::BoolLiteral) {
                    const bool taken = static_cast<const BoolLiteralExpr&>(*ifStmt.condition).value;
                    auto& branch = taken ? ifStmt.thenBody : ifStmt.elseBody;
                    auto& dropped = taken ? ifStmt.elseBody : ifStmt.thenBody;
                    // Splicing a branch with declarations into the parent block could clash with
                    // later declarations of the same name, so only the dead side is removed then.
                    if (!hasTopLevelDeclaration(branch)) {
                        ++stats.removedBranches;
                        for (auto& inner : branch) {
                            append(std::move(inner));
                        }
                        continue;
                    }
                    if (!dropped.empty()) {
                        ++stats.removedBranches;
                        dropped.clear();
                    }
                }
                break;
            }
            case StmtKind::While: {
                auto& whileStmt = static_cast<WhileStmt&>(*stmt);
                optimizeExpr(whileStmt.condition);
                optimizeBlock(whileStmt.body);
                break;
            }
        }
        append(std::move(stmt));
    }
    body = std::move(result);
}
