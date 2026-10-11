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

const Expr* rootVariable(const Expr* expr) {
    while (expr && (expr->kind == ExprKind::Member || expr->kind == ExprKind::Index)) {
        expr = expr->kind == ExprKind::Member ? static_cast<const MemberExpr*>(expr)->object.get()
                                               : static_cast<const IndexExpr*>(expr)->array.get();
    }
    return expr && expr->kind == ExprKind::Variable ? expr : nullptr;
}

bool expressionEscapesAddress(const Expr& expr, const std::string& name);

bool statementEscapesAddress(const Stmt& stmt, const std::string& name) {
    const auto check = [&](const ExprPtr& expr) { return expr && expressionEscapesAddress(*expr, name); };
    switch (stmt.kind) {
        case StmtKind::VarDecl: return check(static_cast<const VarDeclStmt&>(stmt).init);
        case StmtKind::Assign: {
            const auto& assign = static_cast<const AssignStmt&>(stmt);
            return check(assign.target) || check(assign.value);
        }
        case StmtKind::WriteLine: return check(static_cast<const WriteLineStmt&>(stmt).value);
        case StmtKind::Return: return check(static_cast<const ReturnStmt&>(stmt).value);
        case StmtKind::Delete: return check(static_cast<const DeleteStmt&>(stmt).operand);
        case StmtKind::ExprStmt: return check(static_cast<const ExprStmt&>(stmt).expr);
        case StmtKind::If: {
            const auto& branch = static_cast<const IfStmt&>(stmt);
            if (check(branch.condition)) return true;
            return std::any_of(branch.thenBody.begin(), branch.thenBody.end(), [&](const StmtPtr& inner) {
                       return statementEscapesAddress(*inner, name);
                   }) ||
                   std::any_of(branch.elseBody.begin(), branch.elseBody.end(), [&](const StmtPtr& inner) {
                       return statementEscapesAddress(*inner, name);
                   });
        }
        case StmtKind::While: {
            const auto& loop = static_cast<const WhileStmt&>(stmt);
            return check(loop.condition) || std::any_of(loop.body.begin(), loop.body.end(), [&](const StmtPtr& inner) {
                return statementEscapesAddress(*inner, name);
            });
        }
    }
    return false;
}

bool expressionEscapesAddress(const Expr& expr, const std::string& name) {
    switch (expr.kind) {
        case ExprKind::AddressOf:
            if (rootVariable(static_cast<const AddressOfExpr&>(expr).operand.get())) {
                const auto* variable = static_cast<const VariableExpr*>(rootVariable(
                    static_cast<const AddressOfExpr&>(expr).operand.get()));
                if (variable->name == name) return true;
            }
            return expressionEscapesAddress(*static_cast<const AddressOfExpr&>(expr).operand, name);
        case ExprKind::Unary: return expressionEscapesAddress(*static_cast<const UnaryExpr&>(expr).operand, name);
        case ExprKind::Binary: {
            const auto& binary = static_cast<const BinaryExpr&>(expr);
            return expressionEscapesAddress(*binary.lhs, name) || expressionEscapesAddress(*binary.rhs, name);
        }
        case ExprKind::New: return expressionEscapesAddress(*static_cast<const NewExpr&>(expr).init, name);
        case ExprKind::Deref: return expressionEscapesAddress(*static_cast<const DerefExpr&>(expr).operand, name);
        case ExprKind::Move: return expressionEscapesAddress(*static_cast<const MoveExpr&>(expr).operand, name);
        case ExprKind::Call:
            return std::any_of(static_cast<const CallExpr&>(expr).args.begin(), static_cast<const CallExpr&>(expr).args.end(),
                               [&](const ExprPtr& arg) { return expressionEscapesAddress(*arg, name); });
        case ExprKind::ArrayLiteral:
            return std::any_of(static_cast<const ArrayLiteralExpr&>(expr).elements.begin(),
                               static_cast<const ArrayLiteralExpr&>(expr).elements.end(),
                               [&](const ExprPtr& element) { return expressionEscapesAddress(*element, name); });
        case ExprKind::Index: {
            const auto& index = static_cast<const IndexExpr&>(expr);
            return expressionEscapesAddress(*index.array, name) || expressionEscapesAddress(*index.index, name);
        }
        case ExprKind::Member: return expressionEscapesAddress(*static_cast<const MemberExpr&>(expr).object, name);
        case ExprKind::MethodCall: {
            const auto& call = static_cast<const MethodCallExpr&>(expr);
            return expressionEscapesAddress(*call.object, name) ||
                   std::any_of(call.args.begin(), call.args.end(), [&](const ExprPtr& arg) {
                       return expressionEscapesAddress(*arg, name);
                   });
        }
        default: return false;
    }
}

bool statementWritesVariable(const Stmt& stmt, const std::string& name) {
    switch (stmt.kind) {
        case StmtKind::Assign: {
            const auto& target = *static_cast<const AssignStmt&>(stmt).target;
            const Expr* root = rootVariable(&target);
            return root && static_cast<const VariableExpr*>(root)->name == name;
        }
        case StmtKind::If: {
            const auto& branch = static_cast<const IfStmt&>(stmt);
            return std::any_of(branch.thenBody.begin(), branch.thenBody.end(), [&](const StmtPtr& inner) {
                       return statementWritesVariable(*inner, name);
                   }) ||
                   std::any_of(branch.elseBody.begin(), branch.elseBody.end(), [&](const StmtPtr& inner) {
                       return statementWritesVariable(*inner, name);
                   });
        }
        case StmtKind::While:
            return std::any_of(static_cast<const WhileStmt&>(stmt).body.begin(),
                               static_cast<const WhileStmt&>(stmt).body.end(), [&](const StmtPtr& inner) {
                                   return statementWritesVariable(*inner, name);
                               });
        default: return false;
    }
}
}

OptimizerStats Optimizer::optimize(Program& program) {
    stats = {};
    structDeclarations.clear();
    for (const auto& declaration : program.structs) {
        structDeclarations.emplace(declaration.name, &declaration);
    }
    for (auto& function : program.functions) {
        promoteAggregates(function.body);
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
    if (!expr) return;
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
        case ExprKind::AddressOf:
            optimizeExpr(static_cast<AddressOfExpr&>(*expr).operand);
            break;
        case ExprKind::Move:
            optimizeExpr(static_cast<MoveExpr&>(*expr).operand);
            break;
        case ExprKind::Call: {
            auto& call = static_cast<CallExpr&>(*expr);
            for (auto& arg : call.args) {
                optimizeExpr(arg);
            }
            break;
        }
        case ExprKind::ArrayLiteral:
            for (auto& element : static_cast<ArrayLiteralExpr&>(*expr).elements) optimizeExpr(element);
            break;
        case ExprKind::Index: {
            auto& index = static_cast<IndexExpr&>(*expr);
            optimizeExpr(index.array);
            optimizeExpr(index.index);
            break;
        }
        case ExprKind::Member:
            optimizeExpr(static_cast<MemberExpr&>(*expr).object);
            break;
        case ExprKind::MethodCall: {
            auto& call = static_cast<MethodCallExpr&>(*expr);
            optimizeExpr(call.object);
            for (auto& arg : call.args) optimizeExpr(arg);
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

void Optimizer::promoteAggregates(std::vector<StmtPtr>& body) {
    for (std::size_t i = 0; i < body.size(); ++i) {
        if (body[i]->kind != StmtKind::VarDecl) continue;
        auto& declaration = static_cast<VarDeclStmt&>(*body[i]);
        if (declaration.declType.kind != Type::Struct || declaration.init || declaration.isConst || declaration.isStatic ||
            declaration.aggregatePromoted) continue;
        const auto structIt = structDeclarations.find(declaration.declType.structName);
        if (structIt == structDeclarations.end()) continue;
        const auto& fields = structIt->second->fields;
        if (i + fields.size() >= body.size()) continue;

        std::vector<ExprPtr> values(fields.size());
        std::vector<std::size_t> fieldOrder;
        fieldOrder.reserve(fields.size());
        std::vector<bool> assigned(fields.size(), false);
        bool candidate = true;
        for (std::size_t offset = 0; offset < fields.size(); ++offset) {
            Stmt& next = *body[i + 1 + offset];
            if (next.kind != StmtKind::Assign) { candidate = false; break; }
            auto& assignment = static_cast<AssignStmt&>(next);
            if (assignment.target->kind != ExprKind::Member || !assignment.value->isConst) {
                candidate = false;
                break;
            }
            auto& member = static_cast<MemberExpr&>(*assignment.target);
            if (member.object->kind != ExprKind::Variable ||
                static_cast<VariableExpr&>(*member.object).name != declaration.name) {
                candidate = false;
                break;
            }
            const auto field = std::find_if(fields.begin(), fields.end(), [&](const StructField& candidateField) {
                return candidateField.name == member.member;
            });
            if (field == fields.end()) { candidate = false; break; }
            const std::size_t fieldIndex = static_cast<std::size_t>(field - fields.begin());
            if (assigned[fieldIndex]) { candidate = false; break; }
            assigned[fieldIndex] = true;
            fieldOrder.push_back(fieldIndex);
        }
        if (!candidate || std::find(assigned.begin(), assigned.end(), false) != assigned.end()) continue;

        const std::size_t afterAssignments = i + 1 + fields.size();
        for (std::size_t later = afterAssignments; later < body.size(); ++later) {
            if (statementWritesVariable(*body[later], declaration.name) ||
                statementEscapesAddress(*body[later], declaration.name)) {
                candidate = false;
                break;
            }
        }
        if (!candidate) continue;

        for (std::size_t offset = 0; offset < fieldOrder.size(); ++offset) {
            auto& assignment = static_cast<AssignStmt&>(*body[i + 1 + offset]);
            values[fieldOrder[offset]] = std::move(assignment.value);
        }

        for (const auto& field : fields) declaration.aggregateTypes.push_back(field.type);
        declaration.aggregateValues = std::move(values);
        declaration.aggregatePromoted = true;
        declaration.isConst = true;
        body.erase(body.begin() + static_cast<std::ptrdiff_t>(i + 1),
                   body.begin() + static_cast<std::ptrdiff_t>(afterAssignments));
        ++stats.promotedAggregates;
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
                for (auto& value : static_cast<VarDeclStmt&>(*stmt).aggregateValues) optimizeExpr(value);
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
