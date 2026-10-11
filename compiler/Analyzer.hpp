#ifndef XIHASH_ANALYZER_H
#define XIHASH_ANALYZER_H

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Ast.hpp"

// Static type checking. Annotates every Expr with its type and validates declarations,
// assignments, operators, conditions and the required `int main()` entry point.
class Analyzer {
public:
    // Throws XiHashError on the first semantic error.
    void analyze(Program& program);

private:
    void registerExterns(Program& program);
    void registerFunctions(Program& program);
    void analyzeFunction(Function& function);
    void analyzeBlock(std::vector<StmtPtr>& body, const Function& function);
    void analyzeStatement(Stmt& stmt, const Function& function);
    void analyzeExpr(Expr& expr);
    void analyzeCall(CallExpr& expr);
    void analyzeBinary(BinaryExpr& expr);
    Type unifyNumeric(const BinaryExpr& expr);
    void requireConvertible(const Type& to, const Expr& expr, const char* context);
    // Analyzes `expr` as the value stored into something of type `target`. `new` and `move(...)` are
    // only valid here, which guarantees every owning pointer ends up in a variable or heap cell.
    void analyzeInitializer(ExprPtr& expr, const Type& target, const char* context);
    void analyzeNew(NewExpr& expr, const Type& target, const char* context);
    void analyzeMove(MoveExpr& expr);
    static bool isLvalue(const Expr& expr);
    std::optional<Type> lookup(const std::string& name) const;

    struct VariableInfo {
        Type type;
        bool isConst;
    };
    std::vector<std::unordered_map<std::string, VariableInfo>> scopes;
    std::unordered_map<std::string, const ExternBlock*> externBlocks;
    std::unordered_map<std::string, const Function*> xiFunctions;
    std::vector<std::string> openNamespaces;
};

// Whether `expr` can be implicitly converted to `to`.
bool isConvertible(const Type& to, const Expr& expr);

#endif
