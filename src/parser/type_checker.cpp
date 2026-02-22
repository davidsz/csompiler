#include "type_checker.h"
#include "common/context.h"
#include "common/labeling.h"
#include <cassert>
#include <format>
#include <unordered_set>

namespace parser {

struct TypeError : public std::runtime_error
{
    explicit TypeError(std::string_view msg)
    : std::runtime_error(std::string(msg))
    {
    }
};

static size_t roundUp(size_t number, size_t alignment)
{
    return (number + alignment - 1) & ~(alignment - 1);
}

// It must be a constant literal, it must be
// an integer, and its value must be 0.
static bool isNullPointerExpression(const Expression &expr)
{
    if (const ConstantExpression *c = std::get_if<ConstantExpression>(&expr)) {
        return std::visit([](const auto &x) -> bool {
            using T = std::decay_t<decltype(x)>;
            if constexpr(std::is_floating_point_v<T>)
                return false;
            if constexpr (std::is_integral_v<T>)
                return x == 0;
            else
                return false;
        }, c->value);
    }
    return false;
}

static std::unique_ptr<Expression> explicitCast(
    std::unique_ptr<Expression> expr,
    const Type &from_type,
    const Type &to_type)
{
    assert(from_type.isInitialized() && to_type.isInitialized());
    if (from_type == to_type)
        return expr;
    return std::make_unique<Expression>(CastExpression{
        .expr = std::move(expr),
        .inner_type = from_type,
        .type = to_type
    });
}

static std::unique_ptr<Expression> convertByAssignment(
    std::unique_ptr<Expression> expr,
    const Type &from_type,
    const Type &to_type)
{
    assert(from_type.isInitialized() && to_type.isInitialized());
    if (from_type == to_type)
        return expr;
    if (from_type.isArithmetic() && to_type.isArithmetic())
        return explicitCast(std::move(expr), from_type, to_type);
    if (isNullPointerExpression(*expr) && to_type.isPointer())
        return explicitCast(std::move(expr), from_type, to_type);
    if (from_type.isPointer() && to_type.isVoidPointer())
        return explicitCast(std::move(expr), from_type, to_type);
    if (from_type.isVoidPointer() && to_type.isPointer())
        return explicitCast(std::move(expr), from_type, to_type);
    return nullptr;
}

// This is a terrible hack. When a double value stands as a conditional
// expression, we double-negate it just to wrap it into an expression, which
// can be processed during the assembly generation. There are logics implemented
// in ASM to handle that case where the double is a NaN value.
// TODO: Maybe wrap it into a "pseudo-expression" to optimize it out in assembly?
static std::unique_ptr<Expression> notNot(std::unique_ptr<Expression> expr)
{
    auto ret = std::make_unique<Expression>(UnaryExpression{
        .op = UnaryOperator::Not,
        .expr = std::make_unique<Expression>(UnaryExpression{
            .op = UnaryOperator::Not,
            .expr = std::move(expr),
            .type = Type{ BasicType::Int }
        }),
        .type = Type{ BasicType::Int }
    });
    return ret;
}

static bool isLvalue(const Expression &expr, const Type &type)
{
    if (const PointerType *pointer_type = type.getAs<PointerType>()) {
        if (pointer_type->decayed)
            return false;
    }
    if (const DotExpression *dot = std::get_if<DotExpression>(&expr))
        return isLvalue(expr, dot->type);
    return std::holds_alternative<VariableExpression>(expr)
        || std::holds_alternative<DereferenceExpression>(expr)
        || std::holds_alternative<SubscriptExpression>(expr)
        || std::holds_alternative<StringExpression>(expr)
        || std::holds_alternative<ArrowExpression>(expr);
}

static std::optional<Type> getCommonPointerType(
    const Expression &first_expr,
    const Type &first_type,
    const Expression &second_expr,
    const Type &second_type)
{
    assert(first_type.isInitialized() && second_type.isInitialized());
    if (first_type == second_type)
        return first_type;
    if (isNullPointerExpression(first_expr))
        return second_type;
    if (isNullPointerExpression(second_expr))
        return first_type;
    if (first_type.isVoidPointer() && second_type.isPointer())
        return first_type;
    if (first_type.isPointer() && second_type.isVoidPointer())
        return second_type;
    return std::nullopt;
}

static size_t byteSizeOf(const std::vector<ConstantValue> &v)
{
    size_t sum = 0;
    for (auto &c : v)
        sum += byteSizeOf(c);
    return sum;
}

TypeChecker::TypeChecker(Context *context)
    : m_context(context)
    , m_typeTable(context->typeTable.get())
{
    assert(m_context);
}

Type TypeChecker::GetCommonType(const Type &first, const Type &second)
{
    assert(first.isInitialized() && second.isInitialized());

    Type lhs = first.isCharacter() ? Type{ BasicType::Int } : first;
    Type rhs = second.isCharacter() ? Type{ BasicType::Int } : second;

    if (lhs == rhs)
        return lhs;

    if (lhs.isBasic(BasicType::Double) || rhs.isBasic(BasicType::Double))
        return Type{ BasicType::Double };

    if (lhs.size(m_typeTable) == rhs.size(m_typeTable))
        return lhs.isSigned() ? rhs : lhs;

    return lhs.size(m_typeTable) > rhs.size(m_typeTable) ? lhs : rhs;
}

std::vector<ConstantValue>
TypeChecker::ToConstantValueList(const Initializer *init, const Type &type)
{
    std::vector<ConstantValue> ret;

    // Zero initialization
    size_t final_size = type.size(m_typeTable);
    if (!init) {
        ret.push_back(ZeroBytes{ final_size });
        return ret;
    }

    // Array
    if (const ArrayType *array_type = type.getAs<ArrayType>()) {
        size_t element_count = array_type->count;
        const Type &element_type = *array_type->element;
        if (const SingleInit *single = std::get_if<SingleInit>(init)) {
            // Character array initialized by a single string literal
            if (element_type.isCharacter()) {
                if (!single->expr) {
                    ret.push_back(ZeroBytes{ element_count });
                    return ret;
                }

                auto string_expr = std::get_if<StringExpression>(single->expr.get());
                if (!string_expr)
                    Abort("Invalid string initializer.");
                size_t string_length = string_expr->value.size();
                if (string_length > element_count)
                    Abort("Too many characters in string literal.");

                ret.push_back(StringInit{ string_expr->value, string_length < element_count });
                if (element_count > string_length + 1)
                    ret.push_back(ZeroBytes{ element_count - string_length - 1 });
                return ret;
            }

            // It's not allowed in other cases
            Abort("Array type can't be initialized by a scalar value");
            return ret;
        }

        // Initializing an array by a compound initializer
        const CompoundInit *compound = std::get_if<CompoundInit>(init);
        assert(compound);

        if (array_type->count < compound->list.size())
            Abort("Too long compound initializer for the given type.");

        for (auto &element : compound->list) {
            auto values = ToConstantValueList(element.get(), element_type);
            ret.insert(ret.end(),
                std::make_move_iterator(values.begin()),
                std::make_move_iterator(values.end()));
        }

        // Pad the missing bytes
        size_t current_size = byteSizeOf(ret);
        if (current_size < final_size)
            ret.push_back(ZeroBytes{ final_size - current_size });

        return ret;
    }

    // Struct
    if (const StructType *struct_type = type.getAs<StructType>()) {
        if (std::holds_alternative<SingleInit>(*init))
            Abort("Can't initialize a struct with a single initializer");
        const CompoundInit *compound = std::get_if<CompoundInit>(init);
        assert(compound);

        auto entry = m_typeTable->get(struct_type->tag);
        if (entry->members.size() < compound->list.size())
            Abort("Too many initializers for the struct.");

        size_t i = 0;
        size_t current_offset = 0;
        for (auto &member_init : compound->list) {
            const TypeTable::StructMemberEntry &member = entry->members[i];
            // Insert member padding if necessary
            if (member.offset != current_offset)
                ret.push_back(ZeroBytes{ member.offset - current_offset });
            // Recursively convert the initializer
            auto values = ToConstantValueList(member_init.get(), member.type);
            ret.insert(ret.end(),
                std::make_move_iterator(values.begin()),
                std::make_move_iterator(values.end()));
            current_offset = member.offset + member.type.size(m_typeTable);
            i++;
        }
        // Pad the whole struct
        if (current_offset != entry->size)
            ret.push_back(ZeroBytes{ entry->size - current_offset });
        return ret;
    }

    // Scalar type
    if (const SingleInit *single = std::get_if<SingleInit>(init)) {
        assert(single->expr);
        if (auto c = std::get_if<ConstantExpression>(single->expr.get()))
            ret.push_back(ConvertValue(c->value, type));
        else
            Abort("Initializer is not a constant expression.");
        return ret;
    }
    Abort("Scalar types can't have compound initializers.");
    return ret;
}

Type TypeChecker::VisitAndConvert(std::unique_ptr<Expression> &expr)
{
    Type type = std::visit(*this, *expr);
    if (type.isArray()) {
        auto old_expr = std::move(expr);
        Type new_type = Type{ PointerType{
            .referenced = type.getAs<ArrayType>()->element,
            .decayed = true
        } };
        AddressOfExpression addr{
            .expr = std::move(old_expr),
            .type = new_type
        };
        expr = std::make_unique<Expression>(std::move(addr));
        return new_type;
    }
    if (const StructType *struct_type = type.getAs<StructType>()) {
        const std::string &tag = struct_type->tag;
        if (!m_typeTable->contains(tag))
            Abort(std::format("Invalid use of incomplete struct type '{}'", tag));
        return type;
    }
    return type;
}

void TypeChecker::ValidateTypeSpecifier(const Type &type)
{
    if (auto array_type = type.getAs<ArrayType>()) {
        if (!array_type->element->isComplete(m_typeTable))
            Abort("Illegal array of incomplete type");
        ValidateTypeSpecifier(*array_type->element);
    } else if (auto pointer_type = type.getAs<PointerType>())
        ValidateTypeSpecifier(*pointer_type->referenced);
    else if (auto function_type = type.getAs<FunctionType>()) {
        for (auto &p : function_type->params)
            ValidateTypeSpecifier(*p);
        ValidateTypeSpecifier(*function_type->ret);
    }
}

Type TypeChecker::operator()(ConstantExpression &c)
{
    return c.type;
}

Type TypeChecker::operator()(StringExpression &s)
{
    s.type = Type{ ArrayType{
        .element = std::make_shared<Type>(BasicType::Char),
        .count = s.value.size() + 1
    } };
    return s.type;
}

Type TypeChecker::operator()(VariableExpression &v)
{
    if (const SymbolEntry *entry = m_context->symbolTable->get(v.identifier)) {
        if (entry->type.getAs<FunctionType>())
            Abort(std::format("Function name '{}' is used as variable", v.identifier));
        v.type = entry->type;
    } else
        Abort(std::format("Undeclared variable '{}'", v.identifier));
    return v.type;
}

Type TypeChecker::operator()(CastExpression &c)
{
    // c.type was already determined in the AST builder
    ValidateTypeSpecifier(c.type);
    c.inner_type = Type{ VisitAndConvert(c.expr) };
    if (c.type.isVoid())
        return c.type;
    if (!c.type.isScalar())
        Abort("Not allowed to cast an expression to a non-scalar type");
    if (!c.inner_type.isScalar())
        Abort("Cannot cast non-scalar expression to scalar type");
    if ((c.inner_type.isPointer() && c.type.isBasic(Double))
        || (c.inner_type.isBasic(Double) && c.type.isPointer()))
        Abort("Not allowed to cast pointer from/to a floating point type");
    return c.type;
}

Type TypeChecker::operator()(UnaryExpression &u)
{
    Type type = VisitAndConvert(u.expr);
    if (!type.isScalar())
        Abort("Unary operators only apply to scalar expressions");
    if (isMutating(u.op)) {
        if (!isLvalue(*u.expr, type))
            Abort(std::format("Invalid lvalue in {} unary expression", toString(u.op)));
        if (PointerType *pointer_type = type.getAs<PointerType>()) {
            if (!pointer_type->referenced->isComplete(m_typeTable))
                Abort("Incomplete pointer type in unary expression");
        }
    }

    if (type.isBasic(Double) && u.op == UnaryOperator::BitwiseComplement)
        Abort("The type of a unary bitwise complement operation can't be double.");
    if (type.isPointer() && u.op == UnaryOperator::BitwiseComplement)
        Abort("Can't apply complement on a pointer type.");
    if (type.isPointer() && u.op == UnaryOperator::Negate)
        Abort("Can't negate a pointer type.");

    // This is not necessarily correct; ++ and -- also should be integer promoted,
    // but we avoid that in order to keep the one byte representation.
    if (type.isCharacter() && !isMutating(u.op)) {
        Type promotedType = type.promotedType();
        u.expr = explicitCast(std::move(u.expr), type, promotedType);
        u.type = promotedType;
        return u.type;
    }

    if (u.op == UnaryOperator::Not)
        u.type = Type{ BasicType::Int };
    else
        u.type = type;
    return u.type;
}

Type TypeChecker::operator()(BinaryExpression &b)
{
    Type left_type = VisitAndConvert(b.lhs);
    Type right_type = VisitAndConvert(b.rhs);

    if (b.op == BinaryOperator::And || b.op == BinaryOperator::Or) {
        if (!left_type.isScalar() || !right_type.isScalar())
            Abort("Logical operators only apply to scalar expressions");
        // Return value of logical operators can be represented as an integer
        b.type = Type{ BasicType::Int };
        return b.type;
    }

    if (left_type.isBasic(Double) || right_type.isBasic(Double)) {
        if (b.op == BinaryOperator::Remainder
            || b.op == BinaryOperator::LeftShift
            || b.op == BinaryOperator::RightShift
            || b.op == BinaryOperator::BitwiseAnd
            || b.op == BinaryOperator::BitwiseXor
            || b.op == BinaryOperator::BitwiseOr
            || b.op == BinaryOperator::AssignLShift
            || b.op == BinaryOperator::AssignMod
            || b.op == BinaryOperator::AssignRShift
            || b.op == BinaryOperator::AssignBitwiseAnd
            || b.op == BinaryOperator::AssignBitwiseXor
            || b.op == BinaryOperator::AssignBitwiseOr)
            Abort("The type of the binary operation can't be double.");
    }

    if ((left_type.isBasic(Double) && right_type.isPointer())
        || (left_type.isPointer() && right_type.isBasic(Double)))
        Abort("Not allowed to operate between pointers and doubles.");

    if (left_type.isPointer() || right_type.isPointer()) {
        if (b.op == BinaryOperator::Multiply
            || b.op == BinaryOperator::Divide
            || b.op == BinaryOperator::Remainder
            || b.op == BinaryOperator::BitwiseAnd
            || b.op == BinaryOperator::BitwiseXor
            || b.op == BinaryOperator::BitwiseOr)
        Abort("The type of the binary operation can't be a pointer.");
    }

    Type common_type;

    // Pointer arithmetics
    if (b.op == Add) {
        if (left_type.isCompletePointer(m_typeTable) && right_type.isInteger()) {
            b.rhs = explicitCast(std::move(b.rhs), right_type, Type{ BasicType::Long });
            b.type = left_type;
            return b.type;
        } else if (left_type.isInteger() && right_type.isCompletePointer(m_typeTable)) {
            b.lhs = explicitCast(std::move(b.lhs), left_type, Type{ BasicType::Long });
            b.type = right_type;
            return b.type;
        } else if (!left_type.isArithmetic() && !right_type.isArithmetic())
            Abort("Invalid operands for addition.");
    }
    if (b.op == Subtract) {
        if (left_type.isCompletePointer(m_typeTable) && right_type.isInteger()) {
            b.rhs = explicitCast(std::move(b.rhs), right_type, Type{ BasicType::Long });
            b.type = left_type;
            return b.type;
        } else if (left_type.isCompletePointer(m_typeTable) && left_type == right_type) {
            // Difference of two pointers: the number of array elements between them
            b.type = Type{ BasicType::Long };  // This would be ptrdiff_t officially
            return b.type;
        } else if (!left_type.isArithmetic() && !right_type.isArithmetic())
            Abort("Invalid operands for subtraction.");
    }

    if (b.op == LessThan || b.op == LessOrEqual || b.op == GreaterThan || b.op == GreaterOrEqual) {
        if (left_type.isPointer() && right_type.isPointer()) {
            if (left_type == right_type) {
                b.type = Type{ BasicType::Int };
                return b.type;
            } else
                Abort("Not allowed operation between different pointer types.");
        }
        if (!left_type.isArithmetic() || !right_type.isArithmetic())
            Abort(std::format("Invalid operand types for {} expression", toString(b.op)));
    }
    if (b.op == BinaryOperator::Equal || b.op == BinaryOperator::NotEqual) {
        if (left_type.isPointer() || right_type.isPointer()) {
            if (auto cpt = getCommonPointerType(*b.lhs, left_type, *b.rhs, right_type)) {
                common_type = *cpt;
                b.lhs = explicitCast(std::move(b.lhs), left_type, common_type);
                b.rhs = explicitCast(std::move(b.rhs), right_type, common_type);
                b.type = Type{ BasicType::Int };
                return b.type;
            } else
                Abort("Expressions have incompatible pointer types");
        }
        if (!left_type.isArithmetic() || !right_type.isArithmetic())
            Abort(std::format("Invalid operand types for {} expression", toString(b.op)));
    }
    // We already handled all valid pointer arithmetic operations
    if ((left_type.isPointer() && right_type.isArithmetic())
        || (left_type.isArithmetic() && right_type.isPointer()))
        Abort("Not allowed operation between pointer and arithmetic value.");

    if (b.op == LeftShift || b.op == RightShift) {
        if (left_type.isPointer() || right_type.isPointer())
            Abort("Operand of bitshifts can't be pointers.");
        Type promoted_left = left_type.promotedType();
        b.lhs = explicitCast(std::move(b.lhs), left_type, promoted_left);
        b.rhs = explicitCast(std::move(b.rhs), right_type, right_type.promotedType());
        b.type = promoted_left;
        return b.type;
    }

    common_type = GetCommonType(left_type, right_type);
    b.lhs = explicitCast(std::move(b.lhs), left_type, common_type);
    b.rhs = explicitCast(std::move(b.rhs), right_type, common_type);
    if (isRelationOperator(b.op)) {
        // Represented as integer, but they needed a common type before
        b.type = Type{ BasicType::Int };
        return b.type;
    } else if (isAssignment(b.op))
        b.type = left_type;
    else
        b.type = common_type;
    return b.type;
}

Type TypeChecker::operator()(AssignmentExpression &a)
{
    Type left_type = VisitAndConvert(a.lhs);
    if (!isLvalue(*a.lhs, left_type))
        Abort("The left side of an assignment should be an lvalue.");
    Type right_type = VisitAndConvert(a.rhs);
    if (!(a.rhs = convertByAssignment(std::move(a.rhs), right_type, left_type)))
        Abort("Can't convert type for assignment");
    a.type = left_type;
    return a.type;
}

Type TypeChecker::operator()(CompoundAssignmentExpression &c)
{
    Type left_type = VisitAndConvert(c.lhs);
    Type right_type = VisitAndConvert(c.rhs);
    if (!isLvalue(*c.lhs, left_type))
        Abort("The left side of a compound assignment should be an lvalue.");

    if (left_type.isBasic(Double) || right_type.isBasic(Double)) {
        if (c.op == BinaryOperator::AssignLShift
            || c.op == BinaryOperator::AssignMod
            || c.op == BinaryOperator::AssignRShift
            || c.op == BinaryOperator::AssignBitwiseAnd
            || c.op == BinaryOperator::AssignBitwiseXor
            || c.op == BinaryOperator::AssignBitwiseOr)
            Abort("The type of the compound operation can't be double.");
    }

    // Pointer arithmetics
    if (left_type.isPointer()) {
        if (c.op == BinaryOperator::AssignMult
            || c.op == BinaryOperator::AssignDiv
            || c.op == BinaryOperator::AssignMod
            || c.op == BinaryOperator::AssignBitwiseAnd
            || c.op == BinaryOperator::AssignBitwiseXor
            || c.op == BinaryOperator::AssignBitwiseOr
            || c.op == BinaryOperator::AssignLShift
            || c.op == BinaryOperator::AssignRShift)
            Abort("The type of the given compound operation can't be a pointer.");

        if (!left_type.isCompletePointer(m_typeTable))
            Abort("The left side of += and -= must be a complete pointer.");

        if (!right_type.isInteger())
            Abort("The right side of += and -= must be integer if left is a pointer.");

        // += and -=
        c.rhs = explicitCast(std::move(c.rhs), right_type, Type{ BasicType::ULong });
        c.inner_type = left_type;
        c.type = left_type;
        return c.type;
    }
    if (right_type.isPointer())
        Abort("The right side of compound operations can't be a pointer.");

    if (c.op == BinaryOperator::AssignLShift || c.op == BinaryOperator::AssignRShift) {
        // The right operand of shift operators needs an integer promotion
        c.rhs = explicitCast(std::move(c.rhs), right_type, right_type.promotedType());
        c.inner_type = left_type;
        c.type = left_type;
        return c.type;
    }

    Type common_type = GetCommonType(left_type, right_type);
    c.lhs = explicitCast(std::move(c.lhs), left_type, common_type);
    c.rhs = explicitCast(std::move(c.rhs), right_type, common_type);
    c.inner_type = common_type;
    c.type = left_type;
    return c.type;
}

Type TypeChecker::operator()(ConditionalExpression &c)
{
    Type condition_type = VisitAndConvert(c.condition);
    if (!condition_type.isScalar())
        Abort("Conditional expression must have a scalar type condition");

    if (condition_type.isBasic(Double))
        c.condition = notNot(std::move(c.condition));
    Type true_type = VisitAndConvert(c.trueBranch);
    Type false_type = VisitAndConvert(c.falseBranch);

    Type common_type;
    if (true_type.isVoid() || false_type.isVoid()) {
        if (true_type != false_type)
            Abort("Expressions have incompatible void types");
        c.type = Type { VoidType{ } };
        return c.type;
    } else if (true_type.isPointer() || false_type.isPointer()) {
        if (auto cpt = getCommonPointerType(*c.trueBranch, true_type, *c.falseBranch, false_type))
            common_type = *cpt;
        else
            Abort("Expressions have incompatible pointer types");
    } else if (true_type.isStruct() || false_type.isStruct()) {
        if (true_type != false_type)
            Abort("Expressions have incompatible struct types");
        c.type = true_type;
        return c.type;
    } else
        common_type = GetCommonType(true_type, false_type);

    c.trueBranch = explicitCast(std::move(c.trueBranch), true_type, common_type);
    c.falseBranch = explicitCast(std::move(c.falseBranch), false_type, common_type);
    c.type = common_type;
    return c.type;
}

Type TypeChecker::operator()(FunctionCallExpression &f)
{
    if (const FunctionType *type = m_context->symbolTable->getTypeAs<FunctionType>(f.identifier)) {
        if (type->params.size() != f.args.size())
            Abort(std::format("Function '{}' is called with wrong number of arguments", f.identifier));

        for (size_t i = 0; i < f.args.size(); i++) {
            Type arg_type = VisitAndConvert(f.args[i]);
            if (!(f.args[i] = convertByAssignment(std::move(f.args[i]), arg_type, *type->params[i])))
                Abort("Can't convert argument type for function call");
        }
        f.type = *type->ret;
        return f.type;
    } else {
        // The symbol name exists, we verified it during the semantic analysis.
        Abort(std::format("'{}' is not a function name", f.identifier));
    }
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(DereferenceExpression &d)
{
    Type type = VisitAndConvert(d.expr);
    if (PointerType *pointer_type = type.getAs<PointerType>()) {
        if (pointer_type->referenced->isVoid())
            Abort("Can't dereference a void pointer");
        d.type = *pointer_type->referenced;
    } else
        Abort("Can't dereference a non-pointer");
    return d.type;
}

Type TypeChecker::operator()(AddressOfExpression &a)
{
    Type type = std::visit(*this, *a.expr);
    if (!isLvalue(*a.expr, type))
        Abort("Can't take the address of a non-lvalue");
    a.type = Type{ PointerType{ .referenced = std::make_shared<Type>(type) } };
    return a.type;
}

Type TypeChecker::operator()(SubscriptExpression &s)
{
    Type base_type = VisitAndConvert(s.pointer);
    Type index_type = VisitAndConvert(s.index);
    Type result_type;
    if (base_type.isCompletePointer(m_typeTable) && index_type.isInteger()) {
        // array_name[index]
        result_type = base_type;
        s.index = explicitCast(std::move(s.index), index_type, Type{ BasicType::Long });
    } else if (base_type.isInteger() && index_type.isCompletePointer(m_typeTable)) {
        // index[array_name]
        result_type = index_type;
        s.pointer = explicitCast(std::move(s.pointer), base_type, Type{ BasicType::Long });
    } else
        Abort("Subscript expressions must have a (complete) pointer and integer operands.");
    s.type = *result_type.getAs<PointerType>()->referenced;
    return s.type;
}

Type TypeChecker::operator()(SizeOfExpression &s)
{
    s.inner_type = std::visit(*this, *s.expr);
    if (!s.inner_type.isComplete(m_typeTable))
        Abort("Can't take the size of an incomplete type");
    s.type = Type{ BasicType::ULong };
    return s.type;
}

Type TypeChecker::operator()(SizeOfTypeExpression &s)
{
    ValidateTypeSpecifier(s.operand);
    if (!s.operand.isComplete(m_typeTable))
        Abort("Can't take the size of an incomplete type");
    s.type = Type{ BasicType::ULong };
    return s.type;
}

Type TypeChecker::operator()(DotExpression &d)
{
    Type type = VisitAndConvert(d.expr);
    const StructType *struct_type = type.getAs<StructType>();
    if (!struct_type)
        Abort("Can't access member of a non-struct type");
    auto entry = m_typeTable->get(struct_type->tag);
    if (!entry->find(d.identifier))
        Abort(std::format("Member '{}' not found in struct '{}'", d.identifier, struct_type->tag));
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(ArrowExpression &a)
{
    Type type = VisitAndConvert(a.expr);
    const PointerType *pointer_type = type.getAs<PointerType>();
    if (!pointer_type)
        Abort("Can't access member of a non-pointer type");
    const StructType *struct_type = pointer_type->referenced->getAs<StructType>();
    if (!struct_type)
        Abort("Can't access member of a non-struct type");
    auto entry = m_typeTable->get(struct_type->tag);
    if (!entry->find(a.identifier))
        Abort(std::format("Member '{}' not found in struct '{}'", a.identifier, struct_type->tag));
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(ReturnStatement &r)
{
    Type function_return_type = *std::get<FunctionType>(m_functionTypeStack.back().t).ret;
    if (r.expr) {
        if (function_return_type.isVoid())
            Abort("Void function can't return a value");
        Type ret_type = VisitAndConvert(r.expr);
        if (!(r.expr = convertByAssignment(std::move(r.expr), ret_type, function_return_type)))
            Abort("Can't convert return type");
    } else {
        if (!function_return_type.isVoid())
            Abort("Function must return a value");
    }
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(IfStatement &i)
{
    Type condition_type = VisitAndConvert(i.condition);
    if (!condition_type.isScalar())
        Abort("The if statement should have a scalar condition type");
    if (condition_type.isBasic(Double))
        i.condition = notNot(std::move(i.condition));
    std::visit(*this, *i.trueBranch);
    if (i.falseBranch)
        std::visit(*this, *i.falseBranch);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(GotoStatement &)
{
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(LabeledStatement &l)
{
    std::visit(*this, *l.statement);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(BlockStatement &b)
{
    for (auto &i : b.items)
        std::visit(*this, i);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(ExpressionStatement &e)
{
    VisitAndConvert(e.expr);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(NullStatement &)
{
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(BreakStatement &)
{
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(ContinueStatement &)
{
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(WhileStatement &w)
{
    Type condition_type = VisitAndConvert(w.condition);
    if (!condition_type.isScalar())
        Abort("While loop should have a scalar condition type");
    if (condition_type.isBasic(Double))
        w.condition = notNot(std::move(w.condition));
    std::visit(*this, *w.body);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(DoWhileStatement &d)
{
    std::visit(*this, *d.body);
    Type condition_type = VisitAndConvert(d.condition);
    if (!condition_type.isScalar())
        Abort("Do-while loop should have a scalar condition type");
    if (condition_type.isBasic(Double))
        d.condition = notNot(std::move(d.condition));
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(ForStatement &f)
{
    if (f.init) {
        m_forLoopInitializer = true;
        std::visit(*this, *f.init);
        // TODO
        /*
        std::visit([this](auto &init) {
            using T = std::decay_t<decltype(init)>;
            if constexpr (is_expression_v<T>)
                VisitAndConvert(init);
            else
                (*this)(init);
        }, *f.init);
        */
        m_forLoopInitializer = false;
    }
    if (f.condition) {
        Type condition_type = VisitAndConvert(f.condition);
        if (!condition_type.isScalar())
            Abort("For loop should have a scalar condition type");
        if (condition_type.isBasic(Double))
            f.condition = notNot(std::move(f.condition));
    }
    if (f.update)
        VisitAndConvert(f.update);
    std::visit(*this, *f.body);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(SwitchStatement &s)
{
    Type type = VisitAndConvert(s.condition);
    if (type.isBasic(Double) || type.isPointer())
        Abort("The type of a switch statement has to be az integer.");

    // Integer promotion of the controlling expression
    Type promotedType = type.promotedType();
    s.condition = explicitCast(std::move(s.condition), type, promotedType);
    s.type = promotedType;

    m_switches.push_back(&s);
    std::visit(*this, *s.body);
    m_switches.pop_back();
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(CaseStatement &c)
{
    Type type = VisitAndConvert(c.condition);

    if (type.isBasic(Double))
        Abort("The type of a case statement can't be double.");

    if (auto expr = std::get_if<ConstantExpression>(c.condition.get())) {
        SwitchStatement *s = m_switches.back();
        expr->value = ConvertValue(expr->value, s->type);
        c.label = std::format("case_{}_{}", s->label, toLabel(expr->value));
        auto [it, inserted] = s->cases.insert(expr->value);
        if (!inserted)
            Abort("Duplicate case in switch");
    }
    std::visit(*this, *c.statement);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(DefaultStatement &d)
{
    std::visit(*this, *d.statement);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(FunctionDeclaration &f)
{
    // f.type was already determined during the AST build
    ValidateTypeSpecifier(f.type);

    FunctionType *function_type = f.type.getAs<FunctionType>();
    if (!function_type->ret->isComplete(m_typeTable) && f.body)
        Abort(std::format("Defined function '{}' can't return an incomplete type", f.name));
    if (function_type->ret->isArray())
        Abort(std::format("Function '{}' can't return an array", f.name));
    if (!m_fileScope && f.storage == StorageStatic)
        Abort(std::format("Function '{}' can't be declared as static in block scope", f.name));

    bool already_defined = false;
    bool is_global = f.storage != StorageStatic;

    // Adjust the parameter list to accept arrays as pointers
    auto param_types = function_type->params;
    std::vector<std::shared_ptr<Type>> adjusted_param_types;
    for (size_t i = 0; i < f.params.size(); i++) {
        Type adjusted_type;
        if (param_types[i]->isVoid())
            Abort(std::format("Can't declare a parameter of type void in function '{}'", f.name));
        else if (ArrayType *arr = param_types[i]->getAs<ArrayType>())
            adjusted_type = Type{ PointerType{ .referenced = arr->element, .decayed = true } };
        else
            adjusted_type = *param_types[i];
        adjusted_param_types.push_back(std::make_shared<Type>(adjusted_type));
    }
    function_type->params = std::move(adjusted_param_types);

    if (const SymbolEntry *entry = m_context->symbolTable->get(f.name)) {
        if (entry->type != f.type)
            Abort(std::format("Incompatible function declarations of '{}'", f.name));
        if (entry->attrs.defined && f.body)
            Abort(std::format("Function '{}' is defined more than once", f.name));
        already_defined = entry->attrs.defined;
        if (entry->attrs.global && f.storage == StorageStatic)
            Abort(std::format("Static function declaration '{}' follows non-static", f.name));
        is_global = entry->attrs.global;
    }

    // Insert before checking the body to support recursive calls
    m_context->symbolTable->insert(f.name, f.type, IdentifierAttributes{
        .type = IdentifierAttributes::Function,
        .defined = already_defined || (bool)f.body,
        .global = is_global
    });

    if (f.body) {
        for (size_t i = 0; i < f.params.size(); i++) {
            // Arguments must be complete types when defining a function
            Type param_type = *function_type->params[i];
            if (!param_type.isComplete(m_typeTable))
                Abort("Argument of a function can't be an incomplete type");
            // Store the arguments in the symbol table only if the body is present
            m_context->symbolTable->insert(f.params[i], param_type, IdentifierAttributes{
                .type = IdentifierAttributes::Local,
                .defined = false
            });
        }
        m_fileScope = false;
        m_functionTypeStack.push_back(f.type);
        std::visit(*this, *f.body);
        m_functionTypeStack.pop_back();
    }

    return Type{ std::monostate() };
}

Type TypeChecker::operator()(VariableDeclaration &v)
{
    // v.type was already determined during the AST build
    if (v.type.isVoid())
        Abort("Can't declare a variable of type void");
    // Accept a variable declaration with an incomplete structure type
    // only if it has the extern storage class and no initializer.
    if (!v.type.isComplete(m_typeTable) && (v.init || v.storage != StorageExtern))
        Abort("Can't initialize an incomplete variable");
    ValidateTypeSpecifier(v.type);

    if (m_forLoopInitializer && v.storage != StorageDefault)
        Abort("Initializer of a for loop can't have storage specifier");

    // This member helps to verify the correct types in initializer visitors
    m_targetTypeForInitializer = v.type;

    if (m_fileScope) {
        // File-scope variable
        InitialValue init = NoInitializer{};
        if (!v.init) {
            if (v.storage == StorageExtern)
                init = NoInitializer{};
            else
                init = Tentative{};
        } else if (v.type.isPointer())
            init = InitializeStaticPointer(v.init.get(), v.type);
        else
            init = Initial{ ToConstantValueList(v.init.get(), v.type) };

        // std::visit() v.init only after adjusting and compile-time processing
        // to avoid CastExpression wrappers on ConstantExpressions
        if (v.init)
            std::visit(*this, *v.init);

        bool is_global = (v.storage != StorageStatic);

        if (const SymbolEntry *entry = m_context->symbolTable->get(v.identifier)) {
            if (entry->type != v.type)
                Abort(std::format("'{}' redeclared with different type", v.identifier));

            if (v.storage == StorageExtern)
                is_global = entry->attrs.global;
            else if (entry->attrs.global != is_global)
                Abort(std::format("Conflicting variable linkage ('{}')", v.identifier));

            if (std::get_if<Initial>(&entry->attrs.init)) {
                if (std::get_if<Initial>(&init))
                    Abort(std::format("Conflicting file scope variable definition ('{}')", v.identifier));
                else
                    init = entry->attrs.init;
            } else if (!std::holds_alternative<Initial>(init) && std::holds_alternative<Tentative>(entry->attrs.init))
                init = Tentative{};
        }

        m_context->symbolTable->insert(v.identifier, v.type, IdentifierAttributes{
            .type = IdentifierAttributes::Static,
            .global = is_global,
            .init = init
        });
    } else {
        // Block-level variable
        if (v.storage == StorageExtern) {
            if (v.init)
                Abort(std::format("Initializer on local extern variable '{}'", v.identifier));

            if (const SymbolEntry *entry = m_context->symbolTable->get(v.identifier)) {
                if (entry->type != v.type)
                    Abort(std::format("'{}' redeclared with different type", v.identifier));
            } else {
                m_context->symbolTable->insert(v.identifier, v.type, IdentifierAttributes{
                    .type = IdentifierAttributes::Static,
                    .global = true,
                    .init = NoInitializer{}
                });
            }
        } else if (v.storage == StorageStatic) {
            InitialValue init = NoInitializer{};
            if (v.type.isPointer())
                init = InitializeStaticPointer(v.init.get(), v.type);
            else {
                // It doesn't matter if we have an initializer or not; we adjust it
                // if any part is missing and calculate the values in build time,
                // because it has static storage.
                init = Initial{ ToConstantValueList(v.init.get(), v.type) };
            }

            // std::visit() v.init only after adjusting and compile-time processing
            // to avoid CastExpression wrappers on ConstantExpressions
            if (v.init)
                std::visit(*this, *v.init);

            m_context->symbolTable->insert(v.identifier, v.type, IdentifierAttributes{
                .type = IdentifierAttributes::Static,
                .global = false,
                .init = init
            });
        } else {
            // Automatic storage duration; will handle the initializer in TAC.
            // Insert into the table first, because a variable can be used
            // in its own initializer. (int a = a = 5;)
            m_context->symbolTable->insert(v.identifier, v.type, IdentifierAttributes{
                .type = IdentifierAttributes::Local
            });
            if (v.init) {
                m_fileScope = false;
                std::visit(*this, *v.init);
            }
        }
    }
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(StructDeclaration &s)
{
    // Don't process incomplete structs
    if (s.members.empty())
        return Type{ std::monostate() };

    if (m_typeTable->get(s.tag))
        Abort(std::format("Redeclaration of struct '{}'", s.tag));

    // Validation; calculation of sizes, alignments, offsets
    TypeTable::StructEntry entry;
    size_t struct_size = 0;
    size_t struct_alignment = 0;
    std::unordered_set<std::string> member_names;
    for (auto &m : s.members) {
        if (member_names.contains(m.name))
            Abort(std::format("Member names should be unique in struct '{}'", s.tag));
        if (m.type.isArray() && !m.type.getAs<ArrayType>()->element->isComplete(m_typeTable))
            Abort(std::format("Array member '{}' of struct '{}' is incomplete", m.name, s.tag));
        member_names.insert(m.name);

        size_t member_alignment = m.type.alignment(m_typeTable);
        size_t member_offset = roundUp(struct_size, member_alignment);
        entry.members.emplace_back(TypeTable::StructMemberEntry{
            .name = m.name,
            .type = m.type,
            .offset = member_offset
        });

        struct_alignment = std::max(struct_alignment, member_alignment);
        struct_size = member_offset + m.type.size(m_typeTable);
    }
    entry.size = roundUp(struct_size, struct_alignment);
    entry.alignment = struct_alignment;

    // Insert it into the type table
    m_typeTable->insert(s.tag, entry);
    return Type{ std::monostate() };
}

Type TypeChecker::operator()(SingleInit &s)
{
    assert(m_targetTypeForInitializer.isInitialized());

    // When a string literal is used to initialize an array, we’ll type check it differently.
    if (m_targetTypeForInitializer.isArray() && std::holds_alternative<StringExpression>(*s.expr)) {
        std::visit(*this, *s.expr);
        const ArrayType *target_array = m_targetTypeForInitializer.getAs<ArrayType>();
        if (!target_array->element->isCharacter())
            Abort("Can't initialize a non-character type with a string literal");
        const StringExpression *string_expr = std::get_if<parser::StringExpression>(s.expr.get());
        if (string_expr->value.length() > target_array->count) {
            Abort(std::format("Too many characters in string literal ({} vs {})",
                target_array->count, string_expr->value.length()));
        }
        s.type = m_targetTypeForInitializer;
        return s.type;
    }

    Type targetType = m_targetTypeForInitializer;
    Type type = VisitAndConvert(s.expr);
    m_targetTypeForInitializer = targetType;

    if (!(s.expr = convertByAssignment(std::move(s.expr), type, m_targetTypeForInitializer)))
        Abort(std::format("Can't convert initializer from {} to {}.",
            type.toString(), m_targetTypeForInitializer.toString()));
    s.type = m_targetTypeForInitializer;
    return s.type;
}

Type TypeChecker::operator()(CompoundInit &c)
{
    assert(m_targetTypeForInitializer.isInitialized());
    if (const ArrayType *array_type = m_targetTypeForInitializer.getAs<ArrayType>()) {
        if (c.list.size() > array_type->count)
            Abort("Too many initializers for the array.");

        Type outer_target = m_targetTypeForInitializer;
        // Make a copy of the pointed type, because setting m_targetTypeForInitializer
        // in a nested initializer would modify the memory area of array_type->element
        Type inner_target = *array_type->element;
        for (auto &i: c.list) {
            m_targetTypeForInitializer = inner_target;
            std::visit(*this, *i);
        }
        m_targetTypeForInitializer = outer_target;
        c.type = outer_target;
    } else if (const StructType *struct_type = m_targetTypeForInitializer.getAs<StructType>()) {
        auto entry = m_typeTable->get(struct_type->tag);
        if (c.list.size() > entry->members.size())
            Abort("Too many initializers for the struct.");

        Type outer_target = m_targetTypeForInitializer;
        size_t i = 0;
        for (auto &member_init : c.list) {
            m_targetTypeForInitializer = entry->members[i++].type;
            std::visit(*this, *member_init);
        }
        m_targetTypeForInitializer = outer_target;
        c.type = outer_target;
    } else
        Abort("Can't initialize a scalar object with a compound initializer.");
    return c.type;
}

Type TypeChecker::operator()(std::monostate)
{
    return Type{ std::monostate() };
}

Error TypeChecker::CheckAndMutate(std::vector<parser::Declaration> &astVector)
{
    try {
        for (auto &i : astVector) {
            m_fileScope = true;
            std::visit(*this, i);
        }
        return Error::ALL_OK;
    } catch (const TypeError &e) {
        std::cerr << e.what() << std::endl;
        return Error::TYPE_ERROR;
    }
}

void TypeChecker::Abort(std::string_view message)
{
    throw TypeError(
        std::format("[Type error] {}", message));
}

InitialValue TypeChecker::InitializeStaticPointer(
    const Initializer *init,
    const Type &variable_type)
{
    if (!init)
        return Initial{ { MakeConstantValue(0, ULong) } };

    const PointerType *pointer_type = variable_type.getAs<PointerType>();
    assert(pointer_type);
    const SingleInit *single_init = std::get_if<SingleInit>(init);
    if (!single_init)
        Abort("Can't initialize a pointer with a compound initializer");

    // Null-pointer initializer
    // static int *numbers = 0;
    if (std::holds_alternative<ConstantExpression>(*single_init->expr)) {
        if (!isNullPointerExpression(*single_init->expr))
            Abort("Can't initialize a pointer with a non-null integer");
        return Initial{ { MakeConstantValue(0, ULong) } };
    }

    // String literal initializer
    // char *message = "Hello!";
    if (auto string_expr = std::get_if<StringExpression>(single_init->expr.get())) {
        if (!pointer_type->referenced->isCharacter())
            Abort("String literal initialization expect char pointer type");
        // Put the string constant into the symbol table and later initialize
        // the pointer from the address of this.
        std::string constant_name = MakeNameUnique("string");
        Type expr_type = std::visit(*this, *single_init->expr);
        m_context->symbolTable->insert(constant_name, expr_type, IdentifierAttributes{
            .type = IdentifierAttributes::Static,
            .static_init = StringInit{ string_expr->value, true }
        });
        // Return the initializer of the pointer
        return Initial{ { PointerInit{ constant_name } } };
    }

    Abort("Unsupported pointer initialization");
    return NoInitializer{};
}

}; // namespace parser
