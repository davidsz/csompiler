#include "tac_builder.h"
#include "common/labeling.h"
#include <format>

namespace tac {

#if 0
// This can be useful later, but it's a layering violation
Type GetExpressionType(parser::Expression &expr)
{
    return std::visit([](const auto &node) -> Type {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            assert(false);
            return Type{};
        } else {
            return node.type;
        }
    }, expr);
}
#endif

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

TACBuilder::LHSInfo TACBuilder::AnalyzeLHS(const parser::Expression &expr)
{
    if (const auto *var = std::get_if<parser::VariableExpression>(&expr)) {
        Variant v{ var->identifier };
        return {
            .kind = LHSInfo::Kind::Plain,
            .address = v,
            .original_type = GetType(v)
        };
    }

    if (const auto *cast = std::get_if<parser::CastExpression>(&expr))
        return AnalyzeLHS(*cast->expr);

    if (const auto *deref = std::get_if<parser::DereferenceExpression>(&expr)) {
        Value ptr = VisitAndConvert(*deref->expr);
        PointerType *pt = GetType(ptr).getAs<PointerType>();
        assert(pt);
        return {
            .kind = LHSInfo::Kind::Deref,
            .address = ptr,
            .original_type = *pt->referenced
        };
    }

    if (const auto *sub = std::get_if<parser::SubscriptExpression>(&expr)) {
        Value lhs = VisitAndConvert(*sub->pointer);
        Value rhs = VisitAndConvert(*sub->index);
        Type lhs_type = GetType(lhs);

        Value pointer_operand = lhs_type.isPointer() ? lhs : rhs;
        Value integer_operand = lhs_type.isPointer() ? rhs : lhs;

        const PointerType *pt = GetType(pointer_operand).getAs<PointerType>();
        assert(pt);

        Type element_type = pt->referenced->storedType();

        Variant addr = CreateTemporaryVariable(
            Type{ PointerType{ .referenced = std::make_shared<Type>(element_type) } }
        );

        m_instructions.push_back(AddPtr{
            .ptr   = pointer_operand,
            .index = integer_operand,
            .scale = element_type.size(),
            .dst   = addr
        });

        return {
            .kind = LHSInfo::Kind::Deref,
            .address = addr,
            .original_type = element_type
        };
    }

    assert(false);
}

void TACBuilder::EmitRuntimeCompoundInit(
    const parser::Initializer &init,
    const std::string &base,
    int type_size,
    int &offset)
{
    if (auto single = std::get_if<parser::SingleInit>(&init)) {
        Value v = VisitAndConvert(*single->expr);
        m_instructions.push_back(CopyToOffset{ v, base, offset });
        offset += type_size;
    } else if (auto compound = std::get_if<parser::CompoundInit>(&init)) {
        for (auto &elem : compound->list)
            EmitRuntimeCompoundInit(*elem, base, type_size, offset);
    }
}

Type TACBuilder::GetType(const Value &value)
{
    if (auto c = std::get_if<Constant>(&value))
        return getType(c->value);
    else if (auto v = std::get_if<Variant>(&value)) {
        const SymbolEntry *entry = m_symbolTable->get(v->name);
        assert(entry);
        return entry->type;
    } else {
        assert(false);
        return Type{ BasicType::Int };
    }
}

ExpResult TACBuilder::operator()(const parser::ConstantExpression &n)
{
    return PlainOperand{ Constant{ n.value } };
}

ExpResult TACBuilder::operator()(const parser::StringExpression &)
{
    // TODO
    return PlainOperand{ };
}

ExpResult TACBuilder::operator()(const parser::VariableExpression &v)
{
    return PlainOperand{ Variant{ v.identifier } };
}

ExpResult TACBuilder::operator()(const parser::CastExpression &c)
{
    Value result = VisitAndConvert(*c.expr);
    if (c.type == c.inner_type)
        return PlainOperand{ result };
    return PlainOperand{
        CastValue(m_instructions, result, c.inner_type, c.type)
    };
}

ExpResult TACBuilder::operator()(const parser::UnaryExpression &u)
{
    // Implement mutating unary operators as binaries ( a++ -> a = a + 1 )
    if (u.op == UnaryOperator::Increment || u.op == UnaryOperator::Decrement) {
        // We visit the lhs only once
        LHSInfo lhs = AnalyzeLHS(*u.expr);

        // Load current value if it's a pointer dereference
        Variant old_val = CreateTemporaryVariable(lhs.original_type.storedType());
        if (lhs.kind == LHSInfo::Kind::Plain)
            m_instructions.push_back(Copy{ lhs.address, old_val });
        else
            m_instructions.push_back(Load{ lhs.address, old_val });

        // Cast if needed
        Value typed_val = old_val;
        if (lhs.original_type != u.type)
            typed_val = CastValue(m_instructions, old_val, lhs.original_type, u.type);

        // Increment/Decrement
        Variant new_value = CreateTemporaryVariable(u.type.storedType());
        if (u.type.isPointer()) {
            auto ptr_type = u.type.getAs<PointerType>();
            assert(ptr_type);
            int offset = (u.op == UnaryOperator::Increment) ? 1 : -1;
            m_instructions.push_back(AddPtr{
                .ptr = typed_val,
                .index = Constant{ MakeConstantValue(offset, Type{ BasicType::Long }) },
                .scale = ptr_type->referenced->size(),
                .dst = new_value
            });
        } else {
            m_instructions.push_back(Binary{
                unaryToBinary(u.op),
                typed_val,
                Constant{ MakeConstantValue(1, u.type) },
                new_value
            });
        }

        // Cast back if needed
        Value result_to_store = new_value;
        if (u.type != lhs.original_type)
            result_to_store = CastValue(m_instructions, new_value, u.type, lhs.original_type.storedType());

        // Store the result back
        if (lhs.kind == LHSInfo::Kind::Plain)
            m_instructions.push_back(Copy{ result_to_store, lhs.address });
        else
            m_instructions.push_back(Store{ result_to_store, lhs.address });

        // The return value depends on the operator type
        return PlainOperand{ u.postfix ? typed_val : new_value };
    }

    auto unary = Unary{};
    unary.op = u.op;
    unary.src = VisitAndConvert(*u.expr);
    unary.dst = CreateTemporaryVariable(u.type);
    m_instructions.push_back(unary);
    return PlainOperand{ unary.dst };
}

ExpResult TACBuilder::operator()(const parser::BinaryExpression &b)
{
    // Short-circuiting operators
    if (b.op == BinaryOperator::And || b.op == BinaryOperator::Or) {
        Variant result = CreateTemporaryVariable(Type{ BasicType::Int });
        auto lhs_val = VisitAndConvert(*b.lhs);
        auto label_true = MakeNameUnique("true_label");
        auto label_false = MakeNameUnique("false_label");
        auto label_end = MakeNameUnique("end_label");
        if (b.op == BinaryOperator::And) {
            m_instructions.push_back(JumpIfZero{lhs_val, label_false});
            auto rhs = VisitAndConvert(*b.rhs);
            m_instructions.push_back(JumpIfZero{rhs, label_false});
            m_instructions.push_back(Copy{Constant(1), result});
            m_instructions.push_back(Jump{label_end});
            m_instructions.push_back(Label{label_false});
            m_instructions.push_back(Copy{Constant(0), result});
            m_instructions.push_back(Label{label_end});
        } else {
            m_instructions.push_back(JumpIfNotZero{lhs_val, label_true});
            auto rhs = VisitAndConvert(*b.rhs);
            m_instructions.push_back(JumpIfNotZero{rhs, label_true});
            m_instructions.push_back(Copy{Constant(0), result});
            m_instructions.push_back(Jump{label_end});
            m_instructions.push_back(Label{label_true});
            m_instructions.push_back(Copy{Constant(1), result});
            m_instructions.push_back(Label{label_end});
        }
        return PlainOperand{ result };
    }

    Value lhs = VisitAndConvert(*b.lhs);
    Value rhs = VisitAndConvert(*b.rhs);
    Type lhs_type = GetType(lhs);
    Type rhs_type = GetType(rhs);

    // Pointer arithmetics
    if (lhs_type.isPointer() || rhs_type.isPointer()) {
        if (b.op == BinaryOperator::Add) {
            // We determine the pointer and the integer operands for addition
            Value pointer_operand = lhs_type.isPointer() ? lhs : rhs;
            Value integer_operand = lhs_type.isPointer() ? rhs : lhs;
            auto ptr_type = b.type.getAs<PointerType>();
            assert(ptr_type);
            auto add_ptr = AddPtr{
                .ptr = pointer_operand,
                .index = integer_operand,
                .scale = ptr_type->referenced->size(),
                .dst = CreateTemporaryVariable(b.type)
            };
            m_instructions.push_back(add_ptr);
            return PlainOperand{ add_ptr.dst };
        }
        if (b.op == BinaryOperator::Subtract) {
            if (rhs_type.isInteger()) {
                // ptr - int; the order of the operands is already correct
                // The integer operand was converted to long
                auto negate = Unary{
                    .op = UnaryOperator::Negate,
                    .src = rhs,
                    .dst = CreateTemporaryVariable(Type{ BasicType::Long })
                };
                m_instructions.push_back(negate);
                auto ptr_type = b.type.getAs<PointerType>();
                assert(ptr_type);
                auto add_ptr = AddPtr{
                    .ptr = lhs,
                    .index = negate.dst,
                    .scale = ptr_type->referenced->size(),
                    .dst = CreateTemporaryVariable(lhs_type)
                };
                m_instructions.push_back(add_ptr);
                return PlainOperand{ add_ptr.dst };
            } else if (rhs_type.isPointer()) {
                // Subtracting one pointer from another: First, we calculate the difference
                // in bytes, using an ordinary Subtract instruction. Then, we divide this
                // result by the number of bytes in one array element, to calculate the
                // difference between the two pointers in terms of array indices.
                auto diff = Binary{
                    .op = BinaryOperator::Subtract,
                    .src1 = lhs,
                    .src2 = rhs,
                    .dst = CreateTemporaryVariable(Type{ BasicType::Long })
                };
                m_instructions.push_back(diff);

                auto ptr_type = lhs_type.getAs<PointerType>();
                assert(ptr_type);
                int elem_size = ptr_type->referenced->size();
                auto result = Binary{
                    .op = BinaryOperator::Divide,
                    .src1 = diff.dst,
                    .src2 = Constant{ MakeConstantValue(elem_size, Type{ BasicType::Long }) },
                    .dst = CreateTemporaryVariable(Type{ BasicType::Long })
                };
                m_instructions.push_back(result);
                return PlainOperand{ result.dst };
            } else
                assert(false);
        }
    }

    auto binary = Binary{};
    binary.op = b.op;
    binary.src1 = lhs;
    binary.src2 = rhs;
    binary.dst = CreateTemporaryVariable(b.type);
    m_instructions.push_back(binary);
    return PlainOperand{ binary.dst };
}

ExpResult TACBuilder::operator()(const parser::AssignmentExpression &a)
{
    ExpResult left = std::visit(*this, *a.lhs);
    Value right = VisitAndConvert(*a.rhs);
    if (PlainOperand *plain = std::get_if<PlainOperand>(&left)) {
        m_instructions.push_back(Copy{ right, plain->val });
        return left;
    } else if (DereferencedPointer *deref = std::get_if<DereferencedPointer>(&left)) {
        m_instructions.push_back(Store{ right, deref->ptr });
        return PlainOperand{ right };
    }
    assert(false);
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::CompoundAssignmentExpression &c)
{
    // Handling compound assignments becomes a complex task when their lvalue
    // is nested in cast expressions and/or pointer dereferences. To avoid multiplied
    // side effects and get the correct assignment address we visit it only in case of
    // pointer dereference, otherwise we do all the cast operations manually.

    // We visit the lhs only once
    LHSInfo lhs = AnalyzeLHS(*c.lhs);

    // Load the value if lhs is a pointer dereference
    Variant old_val = CreateTemporaryVariable(lhs.original_type.storedType());
    if (lhs.kind == LHSInfo::Kind::Plain)
        m_instructions.push_back(Copy{ lhs.address, old_val });
    else
        m_instructions.push_back(Load{ lhs.address, old_val });

    // Cast the left side if needed
    Value typed_left = old_val;
    if (lhs.original_type != c.inner_type)
        typed_left = CastValue(m_instructions, typed_left, lhs.original_type, c.inner_type);

    Value rhs = VisitAndConvert(*c.rhs);

    // Perform the actual operation (binary or pointer arithmetic)
    Variant tmp = CreateTemporaryVariable(c.inner_type.storedType());
    if (const PointerType *pointer_type = c.inner_type.getAs<PointerType>()) {
        Value index = rhs;
        if (c.op == BinaryOperator::AssignSub) {
            Variant negated = CreateTemporaryVariable(GetType(rhs));
            m_instructions.push_back(Unary{
                UnaryOperator::Negate,
                rhs,
                negated
            });
            index = negated;
        }
        m_instructions.push_back(AddPtr{
            .ptr = typed_left,
            .index = index,
            .scale = pointer_type->referenced->size(),
            .dst = tmp
        });
    } else
        m_instructions.push_back(Binary{ compoundToBinary(c.op), typed_left, rhs, tmp });

    // Cast back if needed
    Value result = tmp;
    if (c.inner_type != c.type)
        result = CastValue(m_instructions, tmp, c.inner_type, c.type.storedType());

    // Store the result if it was modified through a pointer
    if (lhs.kind == LHSInfo::Kind::Plain)
        m_instructions.push_back(Copy{ result, lhs.address });
    else
        m_instructions.push_back(Store{ result, lhs.address });

    return PlainOperand{ result };
}

ExpResult TACBuilder::operator()(const parser::ConditionalExpression &c)
{
    auto label_end = MakeNameUnique("end");
    auto label_false_branch = MakeNameUnique("false_branch");
    Value result = CreateTemporaryVariable(c.type);

    Value condition = VisitAndConvert(*c.condition);
    m_instructions.push_back(JumpIfZero{ condition, label_false_branch });
    Value true_branch_value = VisitAndConvert(*c.trueBranch);
    m_instructions.push_back(Copy{ true_branch_value, result });
    m_instructions.push_back(Jump{ label_end });

    m_instructions.push_back(Label{ label_false_branch });
    Value false_branch_value = VisitAndConvert(*c.falseBranch);
    m_instructions.push_back(Copy{ false_branch_value, result });
    m_instructions.push_back(Label{ label_end });

    return PlainOperand{ result };
}

ExpResult TACBuilder::operator()(const parser::FunctionCallExpression &f)
{
    auto ret = FunctionCall{};
    ret.identifier = f.identifier;
    for (auto &a : f.args)
        ret.args.push_back(VisitAndConvert(*a));
    ret.dst = CreateTemporaryVariable(f.type);
    m_instructions.push_back(ret);
    return PlainOperand{ ret.dst };
}

ExpResult TACBuilder::operator()(const parser::DereferenceExpression &d)
{
    Value result = VisitAndConvert(*d.expr);
    return DereferencedPointer{ result };
}

ExpResult TACBuilder::operator()(const parser::AddressOfExpression &a)
{
    ExpResult inner = std::visit(*this, *a.expr);
    if (PlainOperand *plain = std::get_if<PlainOperand>(&inner)) {
        Variant dst = CreateTemporaryVariable(Type{
            PointerType{ .referenced = std::make_shared<Type>(GetType(plain->val)) }
        });
        m_instructions.push_back(GetAddress{ plain->val, dst });
        return PlainOperand{ dst };
    } else if (DereferencedPointer *deref = std::get_if<DereferencedPointer>(&inner))
        return PlainOperand{ deref->ptr };
    assert(false);
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::SubscriptExpression &s)
{
    Value lhs = VisitAndConvert(*s.pointer);
    Value rhs = VisitAndConvert(*s.index);
    Type lhs_type = GetType(lhs);

    // Support both ptr[i] and i[ptr]
    Value pointer_operand = lhs_type.isPointer() ? lhs : rhs;
    Value integer_operand = lhs_type.isPointer() ? rhs : lhs;
    std::shared_ptr<Type> element_type = std::make_shared<Type>(s.type);

    auto add_ptr = AddPtr{
        .ptr = pointer_operand,
        .index = integer_operand,
        .scale = element_type->size(),
        .dst = CreateTemporaryVariable(Type{ PointerType{ .referenced = element_type } })
    };
    m_instructions.push_back(add_ptr);
    return DereferencedPointer{ add_ptr.dst };
}

ExpResult TACBuilder::operator()(const parser::ReturnStatement &r)
{
    auto ret = Return{};
    ret.val = VisitAndConvert(*r.expr);
    m_instructions.push_back(ret);
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::IfStatement &i)
{
    Value condition = VisitAndConvert(*i.condition);
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

ExpResult TACBuilder::operator()(const parser::GotoStatement &g)
{
    m_instructions.push_back(Jump{ g.label });
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::LabeledStatement &l)
{
    m_instructions.push_back(Label{ l.label });
    std::visit(*this, *l.statement);
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::BlockStatement &b)
{
    for (auto &s : b.items)
        std::visit(*this, s);
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::ExpressionStatement &e)
{
    std::visit(*this, *e.expr);
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::NullStatement &)
{
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::BreakStatement &b)
{
    m_instructions.push_back(Jump{ std::format("break_{}", b.label) });
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::ContinueStatement &c)
{
    m_instructions.push_back(Jump{ std::format("continue_{}", c.label) });
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::WhileStatement &w)
{
    auto label_continue = std::format("continue_{}", w.label);
    auto label_break = std::format("break_{}", w.label);

    m_instructions.push_back(Label{ label_continue });
    Value condition = VisitAndConvert(*w.condition);
    m_instructions.push_back(JumpIfZero{ condition, label_break });
    std::visit(*this, *w.body);
    m_instructions.push_back(Jump{ label_continue });
    m_instructions.push_back(Label{ label_break });
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::DoWhileStatement &d)
{
    auto label_start = std::format("start_{}", d.label);

    m_instructions.push_back(Label{ label_start });
    std::visit(*this, *d.body);
    m_instructions.push_back(Label{ std::format("continue_{}", d.label) });
    Value condition = VisitAndConvert(*d.condition);
    m_instructions.push_back(JumpIfNotZero{ condition, label_start });
    m_instructions.push_back(Label{ std::format("break_{}", d.label) });
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::ForStatement &f)
{
    auto label_start = std::format("start_{}", f.label);
    auto label_break = std::format("break_{}", f.label);

    if (f.init)
        std::visit(*this, *f.init);
    m_instructions.push_back(Label{ label_start });
    Value condition;
    if (f.condition)
        condition = VisitAndConvert(*f.condition);
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

ExpResult TACBuilder::operator()(const parser::SwitchStatement &s)
{
    auto label_break = std::format("break_{}", s.label);

    Value condition = VisitAndConvert(*s.condition);
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

ExpResult TACBuilder::operator()(const parser::CaseStatement &c)
{
    m_instructions.push_back(Label{ c.label });
    std::visit(*this, *c.statement);
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::DefaultStatement &d)
{
    m_instructions.push_back(Label{ d.label });
    std::visit(*this, *d.statement);
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::FunctionDeclaration &f)
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

ExpResult TACBuilder::operator()(const parser::VariableDeclaration &v)
{
    const SymbolEntry *entry = m_symbolTable->get(v.identifier);
    assert(entry);

    // We will move static variable declarations to the top level in a later step
    if (entry->attrs.type == IdentifierAttributes::Static)
        return std::monostate();

    // We discard declarations, but we handle their init expressions
    if (!v.init)
        return std::monostate();

    if (auto single_init = std::get_if<parser::SingleInit>(v.init.get())) {
        Value result = VisitAndConvert(*single_init->expr);
        m_instructions.push_back(Copy{ result, Variant{ v.identifier } });
        return std::monostate();
    }

    if (std::holds_alternative<parser::CompoundInit>(*v.init)) {
        auto initial = std::get_if<Initial>(&entry->attrs.init);
        // Having no initializer in the entry means it should be computed in runtime.
        // e.g.: initializers with automatic storage duration (in block scopes)
        if (!initial) {
            const ArrayType *arr_type = entry->type.getAs<ArrayType>();
            assert(arr_type);
            int type_size = arr_type->element->storedType().size();
            int offset = 0;
            EmitRuntimeCompoundInit(*v.init, v.identifier, type_size, offset);
            return std::monostate();
        }

        // Having initializers in the entry means the type checker already converted
        // it into a list of constant values, because it was possible,
        // e.g.: file scope and static initializers
        int offset = 0;
        int incr = entry->type.storedType().size();
        for (const ConstantValue &cv : initial->list) {
            std::visit([&](auto &val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, ZeroBytes>) {
                    // Only static variables will have ZeroBytes values,
                    // and we already early-returned when processing those.
                    assert(false);
                } else {
                    auto copy = CopyToOffset{
                        .src = Constant{ val },
                        .dst_identifier = v.identifier,
                        .offset = offset
                    };
                    m_instructions.push_back(copy);
                    offset += incr;
                }
            }, cv);
        }
        return std::monostate();
    }
    assert(false);
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::SingleInit &)
{
    assert(false);
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::CompoundInit &)
{
    assert(false);
    return std::monostate();
}

ExpResult TACBuilder::operator()(std::monostate)
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
                std::vector<ConstantValue> initializer;
                if (entry.type.isArray())
                    initializer.push_back(ZeroBytes{ static_cast<size_t>(entry.type.size()) });
                else
                    initializer.push_back(MakeConstantValue(0, entry.type));
                m_topLevel.push_back(StaticVariable{
                    .name = name,
                    .type = entry.type,
                    .global = entry.attrs.global,
                    .list = initializer
                });
            } else if (auto initial = std::get_if<Initial>(&entry.attrs.init)) {
                m_topLevel.push_back(StaticVariable{
                    .name = name,
                    .type = entry.type,
                    .global = entry.attrs.global,
                    .list = initial->list
                });
            }
        }
    }
}

}; // tac
