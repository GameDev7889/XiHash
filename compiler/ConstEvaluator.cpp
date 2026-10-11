#include "ConstEvaluator.hpp"

#include <cmath>
#include <cstdint>
#include <format>

namespace {
Value makeInteger(const Type& type, long long value, SourcePos pos) {
    if (type == Type::Int && (value < INT32_MIN || value > INT32_MAX)) {
        throw XiHashError(pos, "Value out of range for int");
    }
    if (type == Type::UInt) {
        if (value < 0) {
            throw XiHashError(pos, "Negative value cannot be stored in uint");
        }
        if (value > UINT32_MAX) {
            throw XiHashError(pos, "Value out of range for uint");
        }
    }
    Value result;
    result.type = type;
    result.integer = value;
    return result;
}

Value makeReal(const Type& type, double value, SourcePos pos) {
    if (isUnsigned(type) && value < 0) {
        throw XiHashError(pos, std::string("Negative value cannot be stored in ") + typeName(type));
    }
    if (type == Type::Float || type == Type::UFloat) {
        const float narrowed = static_cast<float>(value);
        if (std::isinf(narrowed) && !std::isinf(value)) {
            throw XiHashError(pos, std::string("Value out of range for ") + typeName(type));
        }
        value = narrowed;
    }
    Value result;
    result.type = type;
    result.real = value;
    return result;
}

Value makeBool(bool value) {
    Value result;
    result.type = Type::Bool;
    result.integer = value ? 1 : 0;
    return result;
}

template <typename T>
bool compare(BinaryOp op, const T& a, const T& b) {
    switch (op) {
        case BinaryOp::Eq: return a == b;
        case BinaryOp::Ne: return a != b;
        case BinaryOp::In: return false;
        case BinaryOp::Lt: return a < b;
        case BinaryOp::Le: return a <= b;
        case BinaryOp::Gt: return a > b;
        default: return a >= b;
    }
}

std::string toString(const Value& value) {
    switch (value.type.kind) {
        case Type::Char:
            return std::string(1, static_cast<char>(value.integer));
        case Type::Bool:
            return value.integer ? "true" : "false";
        case Type::Int:
        case Type::UInt:
            return std::to_string(value.integer);
        case Type::Float:
        case Type::UFloat:
            return std::format("{}", static_cast<float>(value.real));
        case Type::Double:
        case Type::UDouble:
            return std::format("{}", value.real);
        default:
            return value.text;
    }
}

Value convert(const Value& value, const Type& target, SourcePos pos) {
    if (target == Type::String || target == Type::Bool || target == Type::Char) {
        return value;
    }
    if (target == Type::Int || target == Type::UInt) {
        return makeInteger(target, value.integer, pos);
    }
    const double number = (value.type == Type::Int || value.type == Type::UInt)
                              ? static_cast<double>(value.integer)
                              : value.real;
    return makeReal(target, number, pos);
}
}

Value ConstEvaluator::evaluate(const Expr& expr) {
    switch (expr.kind) {
        case ExprKind::IntLiteral: {
            Value value;
            value.type = Type::Int;
            value.integer = static_cast<const IntLiteralExpr&>(expr).value;
            return value;
        }
        case ExprKind::FloatLiteral: {
            Value value;
            value.type = Type::Double;
            value.real = static_cast<const FloatLiteralExpr&>(expr).value;
            return value;
        }
        case ExprKind::StringLiteral: {
            Value value;
            value.type = Type::String;
            value.text = static_cast<const StringLiteralExpr&>(expr).value;
            return value;
        }
        case ExprKind::BoolLiteral:
            return makeBool(static_cast<const BoolLiteralExpr&>(expr).value);
        case ExprKind::CharLiteral: {
            Value value;
            value.type = Type::Char;
            value.integer = static_cast<unsigned char>(static_cast<const CharLiteralExpr&>(expr).value);
            return value;
        }
        case ExprKind::Unary: {
            const auto& unary = static_cast<const UnaryExpr&>(expr);
            const Value operand = evaluate(*unary.operand);
            if (unary.op == '!') {
                return makeBool(!operand.integer);
            }
            if (operand.type == Type::Int) {
                return makeInteger(Type::Int, -operand.integer, expr.pos);
            }
            return makeReal(operand.type, -operand.real, expr.pos);
        }
        case ExprKind::Binary:
            return evaluateBinary(static_cast<const BinaryExpr&>(expr));
        default:
            // Only literal-only subtrees are ever passed in by the Optimizer.
            throw XiHashError(expr.pos, "Internal error: non-constant expression cannot be folded");
    }
}

Value ConstEvaluator::evaluateBinary(const BinaryExpr& expr) {
    if (expr.op == BinaryOp::In) {
        throw XiHashError(expr.pos, "Membership expressions are not constant-folded");
    }
    if (expr.op == BinaryOp::And || expr.op == BinaryOp::Or) {
        const bool left = evaluate(*expr.lhs).integer != 0;
        if (expr.op == BinaryOp::And ? !left : left) {
            return makeBool(left);
        }
        return makeBool(evaluate(*expr.rhs).integer != 0);
    }

    const Value lhs = evaluate(*expr.lhs);
    const Value rhs = evaluate(*expr.rhs);

    if (expr.type == Type::Bool) {
        switch (expr.operandType.kind) {
            case Type::Bool:
            case Type::Char: return makeBool(compare(expr.op, lhs.integer, rhs.integer));
            case Type::String: return makeBool(compare(expr.op, lhs.text, rhs.text));
            case Type::Int:
            case Type::UInt:
                return makeBool(compare(expr.op, convert(lhs, expr.operandType, expr.pos).integer,
                                        convert(rhs, expr.operandType, expr.pos).integer));
            default:
                return makeBool(compare(expr.op, convert(lhs, expr.operandType, expr.pos).real,
                                        convert(rhs, expr.operandType, expr.pos).real));
        }
    }

    if (expr.type == Type::String) {
        Value result;
        result.type = Type::String;
        result.text = toString(lhs) + toString(rhs);
        return result;
    }

    const Value a = convert(lhs, expr.type, expr.pos);
    const Value b = convert(rhs, expr.type, expr.pos);

    if (expr.type == Type::Int || expr.type == Type::UInt) {
        long long result = 0;
        switch (expr.op) {
            case BinaryOp::Add: result = a.integer + b.integer; break;
            case BinaryOp::Sub: result = a.integer - b.integer; break;
            case BinaryOp::Mul: result = a.integer * b.integer; break;
            default:
                if (b.integer == 0) {
                    throw XiHashError(expr.pos, "Division by zero");
                }
                result = a.integer / b.integer;
        }
        return makeInteger(expr.type, result, expr.pos);
    }

    double result = 0;
    switch (expr.op) {
        case BinaryOp::Add: result = a.real + b.real; break;
        case BinaryOp::Sub: result = a.real - b.real; break;
        case BinaryOp::Mul: result = a.real * b.real; break;
        default:
            if (b.real == 0) {
                throw XiHashError(expr.pos, "Division by zero");
            }
            result = a.real / b.real;
    }
    return makeReal(expr.type, result, expr.pos);
}
