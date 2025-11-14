#include "tac_builder.h"
#include <format>

namespace tac {

static std::string generateTempVariableName()
{
    static size_t counter = 0;
    return std::format("tmp.{}", counter++);
}

static std::string generateLabelName(std::string_view label)
{
    static size_t counter = 0;
    return std::format("{}_{}", label, counter++);
}

Value TACBuilder::operator()(const parser::NumberExpression &n)
{
    return Constant{ static_cast<int>(n.value) };
}

Value TACBuilder::operator()(const parser::VariableExpression &)
{
    return std::monostate();
}

Value TACBuilder::operator()(const parser::UnaryExpression &u)
{
    auto unary = Unary{};
    unary.op = u.op;
    unary.src = std::visit(*this, *u.expr);
    unary.dst = Variant{ generateTempVariableName() };
    m_instructions.push_back(unary);
    return unary.dst;
}

Value TACBuilder::operator()(const parser::BinaryExpression &b)
{
    // Short-circuiting operators
    if (b.op == BinaryOperator::And || b.op == BinaryOperator::Or) {
        auto result = Variant{ generateTempVariableName() };
        auto lhs_val = std::visit(*this, *b.lhs);
        auto label_true = generateLabelName("true_label");
        auto label_false = generateLabelName("false_label");
        auto label_end = generateLabelName("end_label");
        if (b.op == BinaryOperator::And) {
            m_instructions.push_back(JumpIfZero{lhs_val, label_false});
            auto rhs = std::visit(*this, *b.rhs);
            m_instructions.push_back(JumpIfZero{rhs, label_false});
            m_instructions.push_back(Copy{Constant(1), result});
            m_instructions.push_back(Jump{label_end});
            m_instructions.push_back(Label{label_false});
            m_instructions.push_back(Copy{Constant(0), result});
            m_instructions.push_back(Label{label_end});
        } else {
            m_instructions.push_back(JumpIfNotZero{lhs_val, label_true});
            auto rhs = std::visit(*this, *b.rhs);
            m_instructions.push_back(JumpIfNotZero{rhs, label_true});
            m_instructions.push_back(Copy{Constant(0), result});
            m_instructions.push_back(Jump{label_end});
            m_instructions.push_back(Label{label_true});
            m_instructions.push_back(Copy{Constant(1), result});
            m_instructions.push_back(Label{label_end});
        }
        return result;
    }

    auto binary = Binary{};
    binary.op = b.op;
    binary.src1 = std::visit(*this, *b.lhs);
    binary.src2 = std::visit(*this, *b.rhs);
    binary.dst = Variant{ generateTempVariableName() };
    m_instructions.push_back(binary);
    return binary.dst;
}

Value TACBuilder::operator()(const parser::AssignmentExpression &)
{
    return std::monostate();
}

Value TACBuilder::operator()(const parser::FuncDeclStatement &f)
{
    auto func = FunctionDefinition{};
    func.name = f.name;
    TACBuilder builder;
    func.inst = builder.Convert(f.body);
    m_instructions.push_back(func);
    return std::monostate();
}

Value TACBuilder::operator()(const parser::ReturnStatement &r)
{
    auto ret = Return{};
    ret.val = std::visit(*this, *r.expr);
    m_instructions.push_back(ret);
    return std::monostate();
}

Value TACBuilder::operator()(const parser::BlockStatement &b)
{
    for (auto &s : b.statements)
        std::visit(*this, *s);
    return std::monostate();
}

Value TACBuilder::operator()(const parser::ExpressionStatement &)
{
    return std::monostate();
}

Value TACBuilder::operator()(const parser::NullStatement &)
{
    return std::monostate();
}

Value TACBuilder::operator()(const parser::Declaration &)
{
    return std::monostate();
}

Value TACBuilder::operator()(std::monostate)
{
    assert(false);
    return std::monostate();
}

std::vector<Instruction> TACBuilder::Convert(const std::vector<parser::BlockItem> &list) {
    for (auto &i : list)
        std::visit(*this, i);
    return m_instructions;
}

}; // tac
