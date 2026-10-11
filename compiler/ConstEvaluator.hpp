#ifndef XIHASH_CONSTEVALUATOR_H
#define XIHASH_CONSTEVALUATOR_H

#include <string>

#include "Ast.hpp"

// Folded-away representation of a compile-time constant value.
struct Value {
    Type type = Type::Void;
    long long integer = 0; // Int, UInt, Bool (0 or 1), Char
    double real = 0;       // Float, UFloat, Double, UDouble (floats are rounded to 32-bit)
    std::string text;      // String
};

// Evaluates constant-folded expressions for the Optimizer. The Optimizer only ever asks this to
// evaluate subtrees made entirely of literals (`IntLiteral`/`FloatLiteral`/`StringLiteral`/
// `BoolLiteral`/`CharLiteral`) combined with `Unary`/`Binary` operators, so this does not need to
// know about variables, pointers, or `new` the way the old tree-walking Interpreter did. Throws
// XiHashError on a compile-time-detectable runtime error (overflow, division by zero, etc.) so
// the Optimizer can leave the expression alone and let it fail when the generated program runs.
class ConstEvaluator {
public:
    Value evaluate(const Expr& expr);

private:
    Value evaluateBinary(const BinaryExpr& expr);
};

#endif
