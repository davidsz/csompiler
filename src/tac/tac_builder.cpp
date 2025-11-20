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

Value TACBuilder::operator()(const parser::VariableExpression &v)
{
    return Variant{ v.identifier };
}

Value TACBuilder::operator()(const parser::UnaryExpression &u)
{
    // Implement mutating unary operators as binaries ( a++ -> a = a + 1 )
    if (u.op == UnaryOperator::Increment || u.op == UnaryOperator::Decrement) {
        Value target = std::visit(*this, *u.expr);
        Binary mutation = Binary{
            unaryToBinary(u.op),
            target,
            Constant{ 1 },
            target
        };
        if (u.postfix) {
            Variant temp = Variant{ generateTempVariableName() };
            m_instructions.push_back(Copy{ target, temp });
            m_instructions.push_back(mutation);
            return temp;
        } else {
            m_instructions.push_back(mutation);
            return target;
        }
    }

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

    // Compound assignments
    if (isCompoundAssignment(b.op)) {
        auto binary = Binary{};
        BinaryOperator op = compoundToBinary(b.op);
        binary.op = op;
        Value left = std::visit(*this, *b.lhs);
        binary.src1 = left;
        binary.src2 = std::visit(*this, *b.rhs);
        binary.dst = left;
        m_instructions.push_back(binary);
        return binary.dst;
    }

    auto binary = Binary{};
    binary.op = b.op;
    binary.src1 = std::visit(*this, *b.lhs);
    binary.src2 = std::visit(*this, *b.rhs);
    binary.dst = Variant{ generateTempVariableName() };
    m_instructions.push_back(binary);
    return binary.dst;
}

Value TACBuilder::operator()(const parser::AssignmentExpression &a)
{
    Value result = std::visit(*this, *a.rhs);
    // After semantic analysis, a.lhs is guaranteed to be a variable
    Value var = std::visit(*this, *a.lhs);
    m_instructions.push_back(Copy{ result, var });
    return var;
}

Value TACBuilder::operator()(const parser::ConditionalExpression &c)
{
    auto label_end = generateLabelName("end");
    auto label_false_branch = generateLabelName("false_branch");
    Value result = Variant{ generateTempVariableName() };

    Value condition = std::visit(*this, *c.condition);
    m_instructions.push_back(JumpIfZero{ condition, label_false_branch });
    Value true_branch_value = std::visit(*this, *c.trueBranch);
    m_instructions.push_back(Copy{ true_branch_value, result });
    m_instructions.push_back(Jump{ label_end });

    m_instructions.push_back(Label{ label_false_branch });
    Value false_branch_value = std::visit(*this, *c.falseBranch);
    m_instructions.push_back(Copy{ false_branch_value, result });
    m_instructions.push_back(Label{ label_end });

    return result;
}

Value TACBuilder::operator()(const parser::FuncDeclStatement &f)
{
    auto func = FunctionDefinition{};
    func.name = f.name;
    if (auto body = std::get_if<parser::BlockStatement>(f.body.get())) {
        TACBuilder builder;
        func.inst = builder.Convert(body->items);
        // Avoid undefined behavior in functions where there is no return.
        // If it already had a return, this extra one won't be executed
        // and will be optimised out in later stages.
        func.inst.push_back(Return{ Constant{ 0 } });
    }
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

Value TACBuilder::operator()(const parser::IfStatement &i)
{
    Value condition = std::visit(*this, *i.condition);
    auto label_end = generateLabelName("end");
    if (i.falseBranch) {
        auto label_else = generateLabelName("else");
        m_instructions.push_back(JumpIfZero{ condition, label_else });
        std::visit(*this, *i.trueBranch);
        m_instructions.push_back(Jump{ label_end });
        m_instructions.push_back(Label{ label_else });
        std::visit(*this, *i.falseBranch);
    } else {
        m_instructions.push_back(JumpIfZero{ condition, label_end });
        std::visit(*this, *i.trueBranch);
    }
    m_instructions.push_back(Label{ label_end });
    return std::monostate();
}

Value TACBuilder::operator()(const parser::GotoStatement &g)
{
    m_instructions.push_back(Jump{ g.label });
    return std::monostate();
}

Value TACBuilder::operator()(const parser::LabeledStatement &l)
{
    m_instructions.push_back(Label{ l.label });
    std::visit(*this, *l.statement);
    return std::monostate();
}

Value TACBuilder::operator()(const parser::BlockStatement &b)
{
    for (auto &s : b.items)
        std::visit(*this, s);
    return std::monostate();
}

Value TACBuilder::operator()(const parser::ExpressionStatement &e)
{
    std::visit(*this, *e.expr);
    return std::monostate();
}

Value TACBuilder::operator()(const parser::NullStatement &)
{
    return std::monostate();
}

Value TACBuilder::operator()(const parser::BreakStatement &)
{
    return std::monostate();
}

Value TACBuilder::operator()(const parser::ContinueStatement &)
{
    return std::monostate();
}

Value TACBuilder::operator()(const parser::WhileStatement &)
{
    return std::monostate();
}

Value TACBuilder::operator()(const parser::DoWhileStatement &)
{
    return std::monostate();
}

Value TACBuilder::operator()(const parser::ForStatement &)
{
    return std::monostate();
}

Value TACBuilder::operator()(const parser::Declaration &d)
{
    // We discard declarations, but we handle their init expressions
    if (d.init) {
        Value result = std::visit(*this, *d.init);
        m_instructions.push_back(Copy{ result, Variant{ d.identifier } });
    }
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
