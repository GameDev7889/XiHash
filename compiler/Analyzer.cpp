#include "Analyzer.hpp"

#include <algorithm>
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
    if (to.kind == Type::Array) return expr.type == to;
    if (to.kind == Type::Vector) return expr.type == to;
    if (to.kind == Type::Struct) return expr.type == to;
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
        if (block.name == "Numerals" && externBlocks.contains(block.name)) continue;
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

void Analyzer::registerStructs(Program& program) {
    structDeclarations.clear();
    for (const auto& declaration : program.structs) {
        if (!structDeclarations.emplace(declaration.name, &declaration).second) {
            throw XiHashError(declaration.pos, "Struct '" + declaration.name + "' is already declared");
        }
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
    registerStructs(program);
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
        VariableInfo variable{param.type, false};
        markStructInitialized(variable);
        scopes.back().emplace(param.name, std::move(variable));
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

Analyzer::VariableInfo* Analyzer::findVariable(const std::string& name) {
    for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
        const auto variable = scope->find(name);
        if (variable != scope->end()) return &variable->second;
    }
    return nullptr;
}

const StructField* Analyzer::findStructField(const Type& type, const std::string& name) const {
    if (type.kind != Type::Struct) return nullptr;
    const auto declaration = structDeclarations.find(type.structName);
    if (declaration == structDeclarations.end()) return nullptr;
    const auto field = std::find_if(declaration->second->fields.begin(), declaration->second->fields.end(),
                                    [&](const StructField& candidate) { return candidate.name == name; });
    return field == declaration->second->fields.end() ? nullptr : &*field;
}

void Analyzer::markStructInitialized(VariableInfo& variable) {
    if (variable.type.kind != Type::Struct) return;
    const auto declaration = structDeclarations.find(variable.type.structName);
    if (declaration == structDeclarations.end()) return;
    variable.assignedFields.clear();
    for (const auto& field : declaration->second->fields) variable.assignedFields.insert(field.name);
    variable.initialized = true;
}

std::vector<std::unordered_map<std::string, Analyzer::VariableInfo>> Analyzer::mergeStates(
    const std::vector<std::unordered_map<std::string, VariableInfo>>& left,
    const std::vector<std::unordered_map<std::string, VariableInfo>>& right) {
    auto merged = left;
    for (std::size_t i = 0; i < merged.size() && i < right.size(); ++i) {
        for (auto& [name, variable] : merged[i]) {
            const auto other = right[i].find(name);
            if (other == right[i].end()) continue;
            variable.initialized = variable.initialized && other->second.initialized;
            for (auto field = variable.assignedFields.begin(); field != variable.assignedFields.end();) {
                if (!other->second.assignedFields.contains(*field)) field = variable.assignedFields.erase(field);
                else ++field;
            }
        }
    }
    return merged;
}

void Analyzer::requireConvertible(const Type& to, const Expr& expr, const char* context) {
    if (!isConvertible(to, expr)) {
        throw XiHashError(expr.pos, std::string("Cannot convert ") + typeName(expr.type) + " to " +
                                        typeName(to) + " in " + context);
    }
}

bool Analyzer::isLvalue(const Expr& expr) {
    return expr.kind == ExprKind::Variable || expr.kind == ExprKind::Deref || expr.kind == ExprKind::Index ||
           expr.kind == ExprKind::Member;
}

void Analyzer::analyzeArrayLiteral(ArrayLiteralExpr& expr, const Type* target) {
    const Type* elementType = target ? target->pointee.get() : nullptr;
    const std::size_t targetSize = target && target->kind == Type::Vector ? target->vectorSize :
                                   target && target->kind == Type::Array ? target->arraySize : 0;
    if (targetSize && expr.elements.size() != targetSize) {
        throw XiHashError(expr.pos, "Array literal has " + std::to_string(expr.elements.size()) +
                                     " elements, but the type requires " + std::to_string(targetSize));
    }
    if (!elementType && expr.elements.empty()) {
        throw XiHashError(expr.pos, "Cannot infer the element type of an empty array literal");
    }
    bool constant = true;
    for (auto& element : expr.elements) {
        analyzeExpr(*element);
        if (!elementType) elementType = &element->type;
        if (element->type != *elementType && !isConvertible(*elementType, *element)) {
            throw XiHashError(element->pos, "Array literal elements must have a consistent type");
        }
        constant = constant && element->isConst;
    }
    expr.type = target ? *target : makeArray(*elementType, expr.elements.size());
    expr.isConst = constant;
}

void Analyzer::analyzeInitializer(ExprPtr& expr, const Type& target, const char* context) {
    if (expr->kind == ExprKind::ArrayLiteral && (target.kind == Type::Array || target.kind == Type::Vector)) {
        analyzeArrayLiteral(static_cast<ArrayLiteralExpr&>(*expr), &target);
        return;
    }
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
    if (expr.operand->kind == ExprKind::Variable) {
        const auto& name = static_cast<VariableExpr&>(*expr.operand).name;
        if (VariableInfo* variable = findVariable(name); variable && (variable->isConst || variable->isReadonly)) {
            throw XiHashError(expr.pos, "Cannot move from readonly variable '" + name + "'");
        }
    }
    expr.type = expr.operand->type;
}

void Analyzer::analyzeMember(MemberExpr& member, bool forWrite) {
    if (member.object->kind == ExprKind::Variable) {
        const auto& name = static_cast<VariableExpr&>(*member.object).name;
        if (VariableInfo* variable = findVariable(name); variable && variable->type.kind == Type::Struct) {
            const StructField* field = findStructField(variable->type, member.member);
            if (!field) {
                throw XiHashError(member.pos, "Struct '" + variable->type.structName + "' has no field '" + member.member + "'");
            }
            if (!forWrite && !variable->assignedFields.contains(member.member)) {
                throw XiHashError(member.pos, "Struct field '" + name + "." + member.member + "' is read before it is assigned");
            }
            member.object->type = variable->type;
            member.object->isConst = variable->isConst;
            member.memberType = field->type;
            member.components.clear();
            member.writable = true;
            member.type = field->type;
            member.isConst = variable->isConst;
            return;
        }
    }

    analyzeExpr(*member.object);
    if (member.object->type.kind != Type::Vector) {
        throw XiHashError(member.pos, "Member access requires a struct field or vector swizzle");
    }
    const std::string& text = member.member;
    if (text.empty() || text.size() > 4) {
        throw XiHashError(member.pos, "A vector swizzle must select between one and four components");
    }
    const bool color = text.find_first_of("rgba") != std::string::npos;
    const char* alphabet = color ? "rgba" : "xyzw";
    if ((color && text.find_first_of("xyzw") != std::string::npos) ||
        (!color && text.find_first_of("rgba") != std::string::npos)) {
        throw XiHashError(member.pos, "A swizzle cannot mix xyzw and rgba component names");
    }
    member.components.clear();
    for (const char component : text) {
        const char* found = std::find(alphabet, alphabet + 4, component);
        if (found == alphabet + 4 || static_cast<std::size_t>(found - alphabet) >= member.object->type.vectorSize) {
            throw XiHashError(member.pos, "Swizzle component is not available on " + typeName(member.object->type));
        }
        member.components.push_back(static_cast<std::size_t>(found - alphabet));
    }
    member.writable = true;
    if (member.components.size() > 1) {
        auto assigned = member.components;
        std::sort(assigned.begin(), assigned.end());
        if (std::adjacent_find(assigned.begin(), assigned.end()) != assigned.end()) {
            // Repeated components are valid reads, but cannot form a writable swizzle.
            member.writable = false;
        }
    }
    member.memberType = text.size() == 1 ? *member.object->type.pointee
                                         : makeVector(*member.object->type.pointee, text.size());
    member.type = member.memberType;
    member.isConst = member.object->isConst;
}

void Analyzer::analyzeStatement(Stmt& stmt, const Function& function) {
    switch (stmt.kind) {
        case StmtKind::VarDecl: {
            auto& decl = static_cast<VarDeclStmt&>(stmt);
            if (lookup(decl.name)) {
                throw XiHashError(decl.pos, "Variable '" + decl.name + "' is already declared");
            }
            if (!decl.init) {
                if (decl.declType.kind != Type::Struct) {
                    throw XiHashError(decl.pos, "Only struct variables may be declared without an initializer");
                }
                if (decl.isConst || decl.isReadonly) {
                    throw XiHashError(decl.pos, "A const struct must have an initializer");
                }
                scopes.back()[decl.name] = VariableInfo{decl.declType, decl.isConst, decl.isReadonly, false, {}};
                break;
            }
            analyzeInitializer(decl.init, decl.declType, "declaration");
            if (decl.isConst && !decl.init->isConst) {
                throw XiHashError(decl.init->pos, "A const initializer must be a compile-time constant expression");
            }
            VariableInfo variable{decl.declType, decl.isConst, decl.isReadonly};
            markStructInitialized(variable);
            scopes.back()[decl.name] = std::move(variable);
            break;
        }
        case StmtKind::Assign: {
            auto& assign = static_cast<AssignStmt&>(stmt);
            if (assign.target->kind == ExprKind::Variable) {
                auto& variableExpr = static_cast<VariableExpr&>(*assign.target);
                VariableInfo* variable = findVariable(variableExpr.name);
                if (!variable) throw XiHashError(assign.target->pos, "Unknown variable '" + variableExpr.name + "'");
                if (variable->isConst || variable->isReadonly) {
                    throw XiHashError(assign.target->pos, "Cannot assign to readonly variable '" + variableExpr.name + "'");
                }
                assign.target->type = variable->type;
                assign.target->isConst = false;
            } else if (assign.target->kind == ExprKind::Member) {
                auto& member = static_cast<MemberExpr&>(*assign.target);
                analyzeMember(member, true);
                if (!member.writable) {
                    throw XiHashError(assign.target->pos, "A swizzle with repeated components cannot be assigned");
                }
                Expr* object = member.object.get();
                while (object->kind == ExprKind::Member || object->kind == ExprKind::Index) {
                    object = object->kind == ExprKind::Member ? static_cast<MemberExpr&>(*object).object.get()
                                                              : static_cast<IndexExpr&>(*object).array.get();
                }
                if (object->kind == ExprKind::Variable) {
                    const auto& name = static_cast<VariableExpr&>(*object).name;
                    if (VariableInfo* variable = findVariable(name); variable &&
                        (variable->isConst || variable->isReadonly) && variable->type.kind != Type::Pointer) {
                        throw XiHashError(assign.target->pos, "Cannot modify a member of readonly variable '" + name + "'");
                    }
                }
            } else {
                analyzeExpr(*assign.target);
                Expr* base = assign.target.get();
                while (base->kind == ExprKind::Index || base->kind == ExprKind::Member) {
                    base = base->kind == ExprKind::Index ? static_cast<IndexExpr&>(*base).array.get()
                                                        : static_cast<MemberExpr&>(*base).object.get();
                }
                if (base->kind == ExprKind::Variable) {
                    const auto& name = static_cast<VariableExpr&>(*base).name;
                    if (VariableInfo* variable = findVariable(name); variable &&
                        (variable->isConst || variable->isReadonly) &&
                        (variable->type.kind == Type::Array || variable->type.kind == Type::Vector ||
                         variable->type.kind == Type::Struct)) {
                        throw XiHashError(assign.target->pos, "Cannot modify a member or element of readonly variable '" + name + "'");
                    }
                }
            }
            analyzeInitializer(assign.value, assign.target->type, "assignment");
            if (assign.target->kind == ExprKind::Variable) {
                VariableInfo* variable = findVariable(static_cast<VariableExpr&>(*assign.target).name);
                if (variable && variable->type.kind == Type::Struct) markStructInitialized(*variable);
            } else if (assign.target->kind == ExprKind::Member) {
                auto& member = static_cast<MemberExpr&>(*assign.target);
                if (member.object->kind == ExprKind::Variable && member.object->type.kind == Type::Struct) {
                    VariableInfo* variable = findVariable(static_cast<VariableExpr&>(*member.object).name);
                    if (variable) {
                        variable->assignedFields.insert(member.member);
                        const auto declaration = structDeclarations.find(variable->type.structName);
                        if (declaration != structDeclarations.end() &&
                            variable->assignedFields.size() == declaration->second->fields.size()) {
                            variable->initialized = true;
                        }
                    }
                }
            }
            break;
        }
        case StmtKind::Delete: {
            auto& del = static_cast<DeleteStmt&>(stmt);
            if (del.operand->kind == ExprKind::Variable) {
                const auto& name = static_cast<VariableExpr&>(*del.operand).name;
                if (VariableInfo* variable = findVariable(name); variable && (variable->isConst || variable->isReadonly)) {
                    throw XiHashError(del.pos, "Cannot delete through readonly variable '" + name + "'");
                }
            }
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
            const auto before = scopes;
            analyzeBlock(ifStmt.thenBody, function);
            const auto thenState = scopes;
            scopes = before;
            analyzeBlock(ifStmt.elseBody, function);
            const auto elseState = scopes;
            scopes = mergeStates(thenState, elseState);
            break;
        }
        case StmtKind::While: {
            auto& whileStmt = static_cast<WhileStmt&>(stmt);
            analyzeExpr(*whileStmt.condition);
            if (whileStmt.condition->type != Type::Bool) {
                throw XiHashError(whileStmt.condition->pos, std::string("Condition must be bool, got ") +
                                                               typeName(whileStmt.condition->type));
            }
            const auto before = scopes;
            analyzeBlock(whileStmt.body, function);
            scopes = before; // A loop may execute zero times.
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
            Expr* base = addressOf.operand.get();
            bool indexedThroughPointer = false;
            while (base->kind == ExprKind::Member || base->kind == ExprKind::Index) {
                if (base->kind == ExprKind::Index) indexedThroughPointer = true;
                base = base->kind == ExprKind::Member ? static_cast<MemberExpr&>(*base).object.get()
                                                      : static_cast<IndexExpr&>(*base).array.get();
            }
            if (base->kind == ExprKind::Variable) {
                const auto& name = static_cast<VariableExpr&>(*base).name;
                if (VariableInfo* variable = findVariable(name); variable && (variable->isConst || variable->isReadonly) &&
                    !(indexedThroughPointer && variable->type.kind == Type::Pointer)) {
                    throw XiHashError(expr.pos, "Cannot take a writable address of readonly variable '" + name + "'");
                }
            }
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
                    if (!variable->second.initialized) {
                        throw XiHashError(expr.pos, "Struct variable '" + name + "' is read before all fields are assigned");
                    }
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
        case ExprKind::ArrayLiteral:
            analyzeArrayLiteral(static_cast<ArrayLiteralExpr&>(expr));
            break;
        case ExprKind::Index: {
            auto& subscript = static_cast<IndexExpr&>(expr);
            analyzeExpr(*subscript.array);
            analyzeExpr(*subscript.index);
            const Type* arrayType = &subscript.array->type;
            if (arrayType->kind == Type::Pointer && (arrayType->pointee->kind == Type::Array ||
                                                     arrayType->pointee->kind == Type::Vector)) {
                arrayType = arrayType->pointee.get();
            }
            if (arrayType->kind != Type::Array && arrayType->kind != Type::Vector) {
                throw XiHashError(subscript.array->pos, "Indexing requires an Array or Vector value");
            }
            if (subscript.index->type != Type::Int && subscript.index->type != Type::UInt) {
                throw XiHashError(subscript.index->pos, "Array index must be int or uint");
            }
            expr.type = *arrayType->pointee;
            expr.isConst = subscript.array->isConst;
            break;
        }
        case ExprKind::Member: {
            analyzeMember(static_cast<MemberExpr&>(expr), false);
            break;
        }
        case ExprKind::SizeOf: {
            auto& sizeOf = static_cast<SizeOfExpr&>(expr);
            if (sizeOf.value->kind != ExprKind::Variable) {
                throw XiHashError(sizeOf.value->pos, "sizeOf() requires a variable name");
            }
            auto& variable = static_cast<VariableExpr&>(*sizeOf.value);
            VariableInfo* info = findVariable(variable.name);
            if (!info) {
                throw XiHashError(variable.pos, "Unknown variable '" + variable.name + "'");
            }
            // sizeof inspects a variable's type and does not read its value. This intentionally
            // works before a struct's fields have been initialized.
            sizeOf.value->type = info->type;
            expr.type = Type::Int;
            expr.isConst = true;
            break;
        }
        case ExprKind::MethodCall: {
            auto& call = static_cast<MethodCallExpr&>(expr);
            analyzeExpr(*call.object);
            if (call.object->type.kind != Type::Vector) {
                throw XiHashError(call.pos, "Vector methods require a vector receiver");
            }
            const Type vectorType = call.object->type;
            const std::size_t dimension = vectorType.vectorSize;
            const std::string suffix = vectorType.pointee->kind == Type::Float ? "" :
                vectorType.pointee->kind == Type::Int ? "i" : vectorType.pointee->kind == Type::UInt ? "u" : "d";
            const std::string dim = std::to_string(dimension);
            const auto sameVectorArg = [&]() {
                if (call.args.size() != 1) throw XiHashError(call.pos, "Vector method expects one vector argument");
                analyzeExpr(*call.args[0]);
                if (call.args[0]->type != vectorType) throw XiHashError(call.args[0]->pos, "Vector method argument must have the same vector type");
            };
            if (call.method == "length" || call.method == "Length") {
                if (!call.args.empty()) throw XiHashError(call.pos, "length() takes no arguments");
                call.cppFunction = "length" + dim + suffix;
                expr.type = *vectorType.pointee;
            } else if (call.method == "normalize" || call.method == "Normalize") {
                if (!call.args.empty()) throw XiHashError(call.pos, "normalize() takes no arguments");
                call.cppFunction = "normalize" + dim + suffix;
                expr.type = vectorType;
            } else if (call.method == "Dot" || call.method == "dot") {
                sameVectorArg();
                call.cppFunction = "dot" + dim + suffix;
                expr.type = *vectorType.pointee;
            } else if (call.method == "Add" || call.method == "add" ||
                       call.method == "Subtract" || call.method == "subtract") {
                sameVectorArg();
                call.cppFunction = (call.method == "Add" || call.method == "add" ? "add" : "subtract") + dim + suffix;
                expr.type = vectorType;
            } else if (call.method == "Scale" || call.method == "scale") {
                if (call.args.size() != 1) throw XiHashError(call.pos, "scale() expects one scalar argument");
                analyzeExpr(*call.args[0]);
                requireConvertible(*vectorType.pointee, *call.args[0], "vector scale");
                call.cppFunction = "scale" + dim + suffix;
                expr.type = vectorType;
            } else if (call.method == "Cross" || call.method == "cross") {
                if (dimension != 3) throw XiHashError(call.pos, "cross() is only available on Vector3");
                sameVectorArg();
                call.cppFunction = "cross3" + suffix;
                expr.type = vectorType;
            } else {
                throw XiHashError(call.pos, "Unknown vector method '" + call.method + "'");
            }
            break;
        }
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

        case BinaryOp::In: {
            const Type* container = &rhs.type;
            if (container->kind == Type::Pointer &&
                (container->pointee->kind == Type::Array || container->pointee->kind == Type::Vector)) {
                container = container->pointee.get();
            }
            if (container->kind != Type::Array && container->kind != Type::Vector) {
                fail("Operator 'in' requires an Array or Vector on the right, got " + typeName(rhs.type));
            }
            const Type& elementType = *container->pointee;
            const bool comparable = isNumeric(elementType) || elementType == Type::String ||
                                    elementType == Type::Bool || elementType == Type::Char ||
                                    elementType.kind == Type::Pointer;
            if (!comparable) {
                fail("Operator 'in' cannot search an Array of " + typeName(elementType));
            }
            if (!isConvertible(elementType, lhs)) {
                fail("Cannot search an Array of " + typeName(elementType) + " for " + typeName(lhs.type));
            }
            expr.operandType = *container;
            expr.type = Type::Bool;
            expr.isConst = false;
            return;
        }

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
