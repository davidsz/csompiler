#include "tac_builder.h"
#include "common/labeling.h"
#include <format>

namespace tac {

Variant TACBuilder::CreateTemporaryVariable(const Type &type)
{
    Variant var = Variant{ GenerateTempVariableName() };
    m_symbolTable->insert(var.name, type,
        IdentifierAttributes{ .type = IdentifierAttributes::Local }
    );
    return var;
}

Variant TACBuilder::CastValue(
    std::vector<Instruction> &i,
    const Value &value,
    const Type &from_type,
    const Type &to_type)
{
    Variant dst = CreateTemporaryVariable(to_type);
    // In case of doubles, we don't differentiate between int and long
    if (to_type.isBasic(BasicType::Double)) {
        if (from_type.isSigned())
            i.push_back(IntToDouble{ value, dst });
        else
            i.push_back(UIntToDouble{ value, dst });
        return dst;
    } else if (from_type.isBasic(BasicType::Double)) {
        if (to_type.isSigned())
            i.push_back(DoubleToInt{ value, dst });
        else
            i.push_back(DoubleToUInt{ value, dst });
        return dst;
    }
    // We are preserving type information for assembly generation
    // by using the seemingly redundant Copy
    if (to_type.size() == from_type.size())
        i.push_back(Copy{ value, dst });
    else if (to_type.size() < from_type.size())
        i.push_back(Truncate{ value, dst });
    else if (from_type.isSigned())
        i.push_back(SignExtend{ value, dst });
    else
        i.push_back(ZeroExtend{ value, dst });
    return dst;
}

std::optional<Variant> TACBuilder::GetTargetLvalue(const parser::Expression &expr)
{
    if (const auto *var_expr = std::get_if<parser::VariableExpression>(&expr))
        return Variant{ var_expr->identifier };
    else if (const auto *cast_expr = std::get_if<parser::CastExpression>(&expr))
        return GetTargetLvalue(*cast_expr->expr);
    return std::nullopt;
}

Value TACBuilder::operator()(const parser::ConstantExpression &n)
{
    return Constant{ n.value };
}

Value TACBuilder::operator()(const parser::VariableExpression &v)
{
    return Variant{ v.identifier };
}

Value TACBuilder::operator()(const parser::CastExpression &c)
{
    Value result = std::visit(*this, *c.expr);
    if (c.target_type == c.inner_type)
        return result;
    return CastValue(m_instructions, result, c.inner_type, c.target_type);
}

Value TACBuilder::operator()(const parser::UnaryExpression &u)
{
    // Implement mutating unary operators as binaries ( a++ -> a = a + 1 )
    if (u.op == UnaryOperator::Increment || u.op == UnaryOperator::Decrement) {
        Value target = std::visit(*this, *u.expr);
        Binary mutation = Binary{
            unaryToBinary(u.op),
            target,
            Constant{ MakeConstantValue(1, u.type) },
            target
        };
        if (u.postfix) {
            Variant temp = CreateTemporaryVariable(u.type);
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
    unary.dst = CreateTemporaryVariable(u.type);
    m_instructions.push_back(unary);
    return unary.dst;
}

Value TACBuilder::operator()(const parser::BinaryExpression &b)
{
    // Short-circuiting operators
    if (b.op == BinaryOperator::And || b.op == BinaryOperator::Or) {
        Variant result = CreateTemporaryVariable(Type{ BasicType::Int });
        auto lhs_val = std::visit(*this, *b.lhs);
        auto label_true = MakeNameUnique("true_label");
        auto label_false = MakeNameUnique("false_label");
        auto label_end = MakeNameUnique("end_label");
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
    binary.dst = CreateTemporaryVariable(b.type);
    m_instructions.push_back(binary);
    return binary.dst;
}

Value TACBuilder::operator()(const parser::AssignmentExpression &a)
{
    Value result = std::visit(*this, *a.rhs);
    Value dst = std::visit(*this, *a.lhs);
    m_instructions.push_back(Copy{ result, dst });
    return dst;
}

Value TACBuilder::operator()(const parser::CompoundAssignmentExpression &c)
{
    std::optional<Variant> target = GetTargetLvalue(*c.lhs);
    if (!target)
        assert(false);

    Value lhs = std::visit(*this, *c.lhs);
    Value rhs = std::visit(*this, *c.rhs);
    Variant tmp = CreateTemporaryVariable(c.inner_type);
    m_instructions.push_back(Binary{ compoundToBinary(c.op), lhs, rhs, tmp });

    if (c.inner_type != c.result_type) {
        Value casted = CastValue(m_instructions, tmp, c.inner_type, c.result_type);
        m_instructions.push_back(Copy{ casted, *target });
    } else
        m_instructions.push_back(Copy{ tmp, *target });
    return *target;
}

Value TACBuilder::operator()(const parser::ConditionalExpression &c)
{
    auto label_end = MakeNameUnique("end");
    auto label_false_branch = MakeNameUnique("false_branch");
    Value result = CreateTemporaryVariable(c.type);

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

Value TACBuilder::operator()(const parser::FunctionCallExpression &f)
{
    auto ret = FunctionCall{};
    ret.identifier = f.identifier;
    for (auto &a : f.args)
        ret.args.push_back(std::visit(*this, *a));
    ret.dst = CreateTemporaryVariable(*f.type);
    m_instructions.push_back(ret);
    return ret.dst;
}

Value TACBuilder::operator()(const parser::DereferenceExpression &)
{
    return Constant{ 0 };
}

Value TACBuilder::operator()(const parser::AddressOfExpression &)
{
    return Constant{ 0 };
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
    auto label_end = MakeNameUnique("end");
    if (i.falseBranch) {
        auto label_else = MakeNameUnique("else");
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

Value TACBuilder::operator()(const parser::BreakStatement &b)
{
    m_instructions.push_back(Jump{ std::format("break_{}", b.label) });
    return std::monostate();
}

Value TACBuilder::operator()(const parser::ContinueStatement &c)
{
    m_instructions.push_back(Jump{ std::format("continue_{}", c.label) });
    return std::monostate();
}

Value TACBuilder::operator()(const parser::WhileStatement &w)
{
    auto label_continue = std::format("continue_{}", w.label);
    auto label_break = std::format("break_{}", w.label);

    m_instructions.push_back(Label{ label_continue });
    Value condition = std::visit(*this, *w.condition);
    m_instructions.push_back(JumpIfZero{ condition, label_break });
    std::visit(*this, *w.body);
    m_instructions.push_back(Jump{ label_continue });
    m_instructions.push_back(Label{ label_break });
    return std::monostate();
}

Value TACBuilder::operator()(const parser::DoWhileStatement &d)
{
    auto label_start = std::format("start_{}", d.label);

    m_instructions.push_back(Label{ label_start });
    std::visit(*this, *d.body);
    m_instructions.push_back(Label{ std::format("continue_{}", d.label) });
    Value condition = std::visit(*this, *d.condition);
    m_instructions.push_back(JumpIfNotZero{ condition, label_start });
    m_instructions.push_back(Label{ std::format("break_{}", d.label) });
    return std::monostate();
}

Value TACBuilder::operator()(const parser::ForStatement &f)
{
    auto label_start = std::format("start_{}", f.label);
    auto label_break = std::format("break_{}", f.label);

    if (f.init)
        std::visit(*this, *f.init);
    m_instructions.push_back(Label{ label_start });
    Value condition;
    if (f.condition)
        condition = std::visit(*this, *f.condition);
    else
        condition = Constant { 1 };
    m_instructions.push_back(JumpIfZero{ condition, label_break });
    std::visit(*this, *f.body);
    m_instructions.push_back(Label{ std::format("continue_{}", f.label) });
    if (f.update)
        std::visit(*this, *f.update);
    m_instructions.push_back(Jump{ label_start });
    m_instructions.push_back(Label{ label_break });
    return std::monostate();
}

Value TACBuilder::operator()(const parser::SwitchStatement &s)
{
    auto label_break = std::format("break_{}", s.label);

    Value condition = std::visit(*this, *s.condition);
    for (auto &c : s.cases) {
        auto binary = Binary{};
        binary.op = BinaryOperator::Subtract;
        binary.src1 = condition;
        binary.src2 = Constant { c };
        binary.dst = CreateTemporaryVariable(s.type);
        m_instructions.push_back(binary);
        m_instructions.push_back(JumpIfZero{
            binary.dst,
            std::format("case_{}_{}", s.label, toLabel(c))
        });
    }
    if (s.hasDefault)
        m_instructions.push_back(Jump{ std::format("default_{}", s.label) });
    else
        m_instructions.push_back(Jump{ label_break });
    std::visit(*this, *s.body);
    m_instructions.push_back(Label{ label_break });
    return std::monostate();
}

Value TACBuilder::operator()(const parser::CaseStatement &c)
{
    m_instructions.push_back(Label{ c.label });
    std::visit(*this, *c.statement);
    return std::monostate();
}

Value TACBuilder::operator()(const parser::DefaultStatement &d)
{
    m_instructions.push_back(Label{ d.label });
    std::visit(*this, *d.statement);
    return std::monostate();
}

Value TACBuilder::operator()(const parser::FunctionDeclaration &f)
{
    // We only handle definitions, not declarations
    if (!f.body)
        return std::monostate();

    auto func = FunctionDefinition{};
    func.name = f.name;
    // Nothing to do here with parameters. They already have unique names
    // after semantic analysis and they will be pseudo-registers in ASM.
    func.params = f.params;
    if (auto body = std::get_if<parser::BlockStatement>(f.body.get())) {
        TACBuilder builder(m_symbolTable);
        func.inst = builder.ConvertBlock(body->items);
        // Avoid undefined behavior in functions where there is no return.
        // If it already had a return, this extra one won't be executed
        // and will be optimised out in later stages.
        const FunctionType *type = f.type.getAs<FunctionType>();
        assert(type);
        func.inst.push_back(Return{ Constant{ MakeConstantValue(0, *(type->ret)) } });
    }

    if (const SymbolEntry *entry = m_symbolTable->get(f.name))
        func.global = entry->attrs.global;

    m_topLevel.push_back(func);
    return std::monostate();
}

Value TACBuilder::operator()(const parser::VariableDeclaration &v)
{
    // We will move static variable declarations to the top level in a later step
    if (const SymbolEntry *entry = m_symbolTable->get(v.identifier)) {
        if (entry->attrs.type == IdentifierAttributes::Static)
            return std::monostate();
    }
    // We discard declarations, but we handle their init expressions
    if (v.init) {
        Value result = std::visit(*this, *v.init);
        m_instructions.push_back(Copy{ result, Variant{ v.identifier } });
    }
    return std::monostate();
}

Value TACBuilder::operator()(std::monostate)
{
    assert(false);
    return std::monostate();
}

TACBuilder::TACBuilder(std::shared_ptr<SymbolTable> symbolTable)
    : m_symbolTable(symbolTable)
{
}

std::vector<TopLevel> TACBuilder::ConvertTopLevel(const std::vector<parser::Declaration> &list)
{
    m_topLevel.clear();
    for (auto &i : list)
        std::visit(*this, i);

    ProcessStaticSymbols();

    return std::move(m_topLevel);
}

std::vector<Instruction> TACBuilder::ConvertBlock(const std::vector<parser::BlockItem> &list)
{
    m_instructions.clear();
    for (auto &i : list)
        std::visit(*this, i);
    return std::move(m_instructions);
}

void TACBuilder::ProcessStaticSymbols()
{
    for (const auto &[name, entry] : m_symbolTable->m_table) {
        if (entry.attrs.type == IdentifierAttributes::Static) {
            if (std::holds_alternative<Tentative>(entry.attrs.init)) {
                m_topLevel.push_back(StaticVariable{
                    .name = name,
                    .type = entry.type,
                    .global = entry.attrs.global,
                    .init = MakeConstantValue(0, entry.type)
                });
            } else if (auto n = std::get_if<Initial>(&entry.attrs.init)) {
                m_topLevel.push_back(StaticVariable{
                    .name = name,
                    .type = entry.type,
                    .global = entry.attrs.global,
                    .init = n->i
                });
            }
        }
    }
}

}; // tac
