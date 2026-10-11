#ifndef XIHASH_OPTIMIZER_H
#define XIHASH_OPTIMIZER_H

#include <vector>

#include "Ast.hpp"
#include "ConstEvaluator.hpp"

struct OptimizerStats {
    int foldedExpressions = 0;
    int removedBranches = 0;
    int removedStatements = 0;
};

// AST-level optimizations that run after the Analyzer, so they never hide semantic errors:
//  - constant folding (expressions made only of literals are evaluated at compile time),
//  - short-circuit simplification (`true && x` -> `x`, `false && x` -> `false`, ...),
//  - dead branch elimination (`if` with a constant condition keeps only the taken branch),
//  - unreachable code removal (statements after a `return`).
// Expressions that would fail at runtime (division by zero, overflow) are left untouched.
class Optimizer {
public:
    OptimizerStats optimize(Program& program);

private:
    void optimizeBlock(std::vector<StmtPtr>& body);
    void optimizeExpr(ExprPtr& expr);
    void tryFold(ExprPtr& expr);
    void simplifyLogical(ExprPtr& expr);

    ConstEvaluator evaluator;
    OptimizerStats stats;
};

#endif
