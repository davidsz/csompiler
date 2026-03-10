#include "optimization.h"
#include "common/system.h"

namespace tac {

DIAG_PUSH
DIAG_IGNORE("-Wsign-conversion")
DIAG_IGNORE("-Wswitch")
DIAG_IGNORE("-Wswitch-enum")
static std::optional<ConstantValue> evaluate(
    BinaryOperator op,
    const ConstantValue &a,
    const ConstantValue &b)
{
    return std::visit([&](auto lhs, auto rhs) -> std::optional<ConstantValue> {
        using L = std::decay_t<decltype(lhs)>;
        using R = std::decay_t<decltype(rhs)>;
        if constexpr (!is_custom_constant_type<L>::value && !is_custom_constant_type<R>::value) {
            using T = std::common_type_t<L,R>;
            T l = lhs;
            T r = rhs;

            switch (op) {
            case BinaryOperator::Multiply:       return ConstantValue(T(l * r));
            case BinaryOperator::Divide: {
                if (r == 0)
                    return std::nullopt;
                return ConstantValue(T(l / r));
            }
            case BinaryOperator::Add:            return ConstantValue(T(l + r));
            case BinaryOperator::Subtract:       return ConstantValue(T(l - r));
            case BinaryOperator::LessThan:       return ConstantValue(bool(l < r));
            case BinaryOperator::LessOrEqual:    return ConstantValue(bool(l <= r));
            case BinaryOperator::GreaterThan:    return ConstantValue(bool(l > r));
            case BinaryOperator::GreaterOrEqual: return ConstantValue(bool(l >= r));
            case BinaryOperator::Equal:          return ConstantValue(bool(l == r));
            case BinaryOperator::NotEqual:       return ConstantValue(bool(l != r));
            }

            if constexpr (!std::is_floating_point_v<T>) {
                switch (op) {
                case BinaryOperator::Remainder:  return ConstantValue(T(l % r));
                case BinaryOperator::LeftShift:  return ConstantValue(T(l << r));
                case BinaryOperator::RightShift: return ConstantValue(T(l >> r));
                case BinaryOperator::BitwiseAnd: return ConstantValue(T(l & r));
                case BinaryOperator::BitwiseXor: return ConstantValue(T(l ^ r));
                case BinaryOperator::BitwiseOr:  return ConstantValue(T(l | r));
                case BinaryOperator::And:        return ConstantValue(bool(l && r));
                case BinaryOperator::Or:         return ConstantValue(bool(l || r));
                }
            }
        }
        return std::nullopt;
    }, a, b);
}
DIAG_POP

static std::list<Instruction>::iterator foldUnary(
    std::list<Instruction> &,
    std::list<Instruction>::iterator it,
    bool &)
{
    // TODO
    return std::next(it);
}

static std::list<Instruction>::iterator foldBinary(
    std::list<Instruction>::iterator it,
    bool &changed)
{
    auto &obj = std::get<Binary>(*it);
    const Constant *lhs = std::get_if<Constant>(&obj.src1);
    const Constant *rhs = std::get_if<Constant>(&obj.src2);
    if (!lhs || !rhs)
        return std::next(it);
    if (auto result = evaluate(obj.op, lhs->value, rhs->value)) {
        *it = Copy{ Constant{ *result }, obj.dst };
        changed = true;
    }
    return std::next(it);
}

static std::list<Instruction>::iterator foldJumpIfZero(
    std::list<Instruction> &i,
    std::list<Instruction>::iterator it,
    bool &changed)
{
    auto &obj = std::get<JumpIfZero>(*it);
    const Constant *condition = std::get_if<Constant>(&obj.condition);
    if (!condition)
        return std::next(it);
    if (castTo<int>(condition->value) == 0)
        *it = Jump{ obj.target };
    else
        it = i.erase(it);
    changed = true;
    return std::next(it);
}

static std::list<Instruction>::iterator foldJumpIfNotZero(
    std::list<Instruction> &i,
    std::list<Instruction>::iterator it,
    bool &changed)
{
    auto &obj = std::get<JumpIfNotZero>(*it);
    const Constant *condition = std::get_if<Constant>(&obj.condition);
    if (!condition)
        return std::next(it);
    if (castTo<int>(condition->value) != 0)
        *it = Jump{ obj.target };
    else
        it = i.erase(it);
    changed = true;
    return std::next(it);
}

void constantFolding(std::list<Instruction> &instructions, bool &changed)
{
    for (auto it = instructions.begin(); it != instructions.end();) {
        it = std::visit([&](auto &obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, Unary>) {
                return foldUnary(instructions, it, changed);
            } else if constexpr (std::is_same_v<T, Binary>) {
                return foldBinary(it, changed);
            } else if constexpr (std::is_same_v<T, JumpIfZero>) {
                return foldJumpIfZero(instructions, it, changed);
            } else if constexpr (std::is_same_v<T, JumpIfNotZero>) {
                return foldJumpIfNotZero(instructions, it, changed);
            } else
                return std::next(it);
        }, *it);
    }
}

}; // tac
