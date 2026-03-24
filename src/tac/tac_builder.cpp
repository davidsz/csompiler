#include "tac_builder.h"
#include "common/context.h"
#include "common/labeling.h"
#include <format>

namespace tac {

Variant TACBuilder::CreateTemporaryVariable(const Type &type)
{
    Variant var = Variant{ GenerateTempVariableName() };
    m_context->symbolTable->insert(var.name, type,
        IdentifierAttributes{ .type = IdentifierAttributes::Local }
    );
    return var;
}

Variant TACBuilder::CastValue(
    std::list<Instruction> &i,
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
    if (to_type.size(m_typeTable) == from_type.size(m_typeTable))
        i.push_back(Copy{ value, dst });
    else if (to_type.size(m_typeTable) < from_type.size(m_typeTable))
        i.push_back(Truncate{ value, dst });
    else if (from_type.isSigned())
        i.push_back(SignExtend{ value, dst });
    else
        i.push_back(ZeroExtend{ value, dst });
    return dst;
}

std::pair<ExpResult, Type> TACBuilder::VisitLHS(const parser::Expression &expr)
{
    if (const auto *var = std::get_if<parser::VariableExpression>(&expr))
        return std::make_pair(std::visit(*this, expr), var->type);

    if (const auto *cast = std::get_if<parser::CastExpression>(&expr))
        return VisitLHS(*cast->expr);

    if (const auto *deref = std::get_if<parser::DereferenceExpression>(&expr))
        return std::make_pair(std::visit(*this, expr), deref->type);

    if (const auto *sub = std::get_if<parser::SubscriptExpression>(&expr))
        return std::make_pair(std::visit(*this, expr), sub->type);

    if (const auto *dot = std::get_if<parser::DotExpression>(&expr))
        return std::make_pair(std::visit(*this, expr), dot->type);

    if (const auto *arr = std::get_if<parser::ArrowExpression>(&expr))
        return std::make_pair(std::visit(*this, expr), arr->type);

    assert(false);
}

void TACBuilder::EmitZeroInit(const Type &type, const std::string &base, size_t &offset)
{
    if (const ArrayType *array = type.getAs<ArrayType>()) {
        for (size_t i = 0; i < array->count; i++)
            EmitZeroInit(*array->element, base, offset);
        return;
    }

    if (const AggregateType *aggr_type = type.getAs<AggregateType>()) {
        auto entry = m_typeTable->get(aggr_type->tag);
        for (auto &member : entry->members) {
            size_t member_offset = offset + member.offset;
            EmitZeroInit(member.type, base, member_offset);
        }
        offset += entry->size;
        return;
    }

    AddInstruction(CopyToOffset{
        Constant{ MakeConstantValue(0, type) },
        base,
        offset
    });
    offset += type.storedType().size(m_typeTable);
}

void TACBuilder::EmitRuntimeInitForNonScalar(
    const parser::Initializer *init,
    const std::string &base,
    const Type &type,
    size_t &offset)
{
    assert(init);

    if (const ArrayType *array_type = type.getAs<ArrayType>()) {
        const Type &element_type = *array_type->element;
        // Scalar initializer for an array type
        if (auto single = std::get_if<parser::SingleInit>(init)) {
            // Initializing an array by a string literal
            // (the only case where allowed to initialize an array with a single init)
            assert(element_type.isCharacter());
            auto str = std::get_if<parser::StringExpression>(single->expr.get());
            assert(str);
            const std::string &text = str->value;
            size_t i = 0;
            // Optimization: pack the characters into 4-byte chunks if possible
            while (i + 4 <= text.size() && offset % 4 == 0) {
                const char *p = &text[i];
                // std::string uses signed chars; we have to cast to uint8_t first
                // to avoid sign extension
                uint32_t v = uint32_t(uint8_t(p[0])) |
                             uint32_t(uint8_t(p[1]) <<  8) |
                             uint32_t(uint8_t(p[2]) << 16) |
                             uint32_t(uint8_t(p[3]) << 24);
                AddInstruction(
                    CopyToOffset{
                        Constant{ MakeConstantValue(v, Type{ BasicType::UInt }) },
                        base,
                        offset
                    });
                i += 4;
                offset += 4;
            }

            // Copy the rest of the string one by one
            for (; i < text.size(); i++) {
                AddInstruction(
                    CopyToOffset{
                        Constant{ MakeConstantValue(text[i], Type{ BasicType::Char }) },
                        base,
                        offset
                    });
                offset++;
            }

            // Null terminator and padding with zeros (if possible)
            for (; i < array_type->count; i++) {
                AddInstruction(CopyToOffset{
                    Constant{ MakeConstantValue(0, Type{ BasicType::Char }) },
                    base,
                    offset
                });
                offset++;
            }
            return;
        }

        // Compound initializer for an array type
        auto compound = std::get_if<parser::CompoundInit>(init);
        assert(compound);

        size_t initialized_count = 0;
        for (auto &elem : compound->list) {
            EmitRuntimeInitForNonScalar(elem.get(), base, element_type, offset);
            initialized_count++;
        }

        // Pad remaining elements
        for (; initialized_count < array_type->count; initialized_count++)
            EmitZeroInit(element_type, base, offset);
        return;
    }

    if (const AggregateType *aggr_type = type.getAs<AggregateType>()) {
        auto entry = m_typeTable->get(aggr_type->tag);

        // Single init for an aggregate type
        if (auto single = std::get_if<parser::SingleInit>(init)) {
            assert(single->expr);
            Value v = VisitAndConvert(*single->expr);
            AddInstruction(CopyToOffset{ v, base, offset });
            offset += type.storedType().size(m_typeTable);
            return;
        }

        // Compound initializer for an aggregate type
        auto compound = std::get_if<parser::CompoundInit>(init);
        assert(compound);
        size_t i = 0;
        for (; i < compound->list.size() && i < entry->members.size(); i++) {
            const TypeTable::AggregateMemberEntry &member = entry->members[i];
            size_t member_offset = aggr_type->is_union ? offset : offset + member.offset;
            EmitRuntimeInitForNonScalar(
                compound->list[i].get(),
                base,
                member.type,
                member_offset
            );
        }

        if (!aggr_type->is_union) {
            for (; i < entry->members.size(); ++i) {
                const TypeTable::AggregateMemberEntry &member = entry->members[i];
                size_t member_offset = offset + member.offset;
                EmitZeroInit(member.type, base, member_offset);
            }
        }
        offset += entry->size;
        return;
    }

    // Single initializer for a scalar member of a compound type
    if (auto single = std::get_if<parser::SingleInit>(init)) {
        if (!single->expr)
            EmitZeroInit(type, base, offset);
        else {
            Value v = VisitAndConvert(*single->expr);
            AddInstruction(CopyToOffset{ v, base, offset });
            offset += type.storedType().size(m_typeTable);
        }
        return;
    }

    assert(false);
}

void TACBuilder::EmitRuntimeInitForScalar(
    const parser::Initializer *init,
    const std::string &base,
    const Type &type)
{
    // Use only Copy instructions to be able to differentiate later
    // from CopyToOffsets of arrays and aggregate types
    assert(init);
    auto single = std::get_if<parser::SingleInit>(init);
    assert(single);
    if (!single->expr) {
        AddInstruction(Copy{
            Constant{ MakeConstantValue(0, type) },
            Variant{ base }
        });
    } else {
        AddInstruction(Copy{
            VisitAndConvert(*single->expr),
            Variant{ base }
        });
    }
}

Type TACBuilder::GetType(const Value &value)
{
    if (auto c = std::get_if<Constant>(&value))
        return getType(c->value);
    else if (auto v = std::get_if<Variant>(&value)) {
        const SymbolEntry *entry = m_context->symbolTable->get(v->name);
        assert(entry);
        return entry->type;
    } else {
        assert(false);
        return Type{ BasicType::Int };
    }
}

const Type &TACBuilder::GetExpressionType(const parser::Expression &expr)
{
    return std::visit([](const auto &node) -> const Type & {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            assert(false);
        } else {
            return node.type;
        }
    }, expr);
}

ExpResult TACBuilder::operator()(const parser::ConstantExpression &n)
{
    return PlainOperand{ Constant{ n.value } };
}

ExpResult TACBuilder::operator()(const parser::StringExpression &s)
{
    std::string name = MakeNameUnique("string");
    m_context->symbolTable->insert(
        name,
        Type { ArrayType{
            .element = std::make_shared<Type>(BasicType::Char),
            .count = s.value.size()
        } },
        IdentifierAttributes{
            .type = IdentifierAttributes::Constant,
            .static_init = StringInit{ s.value, true }
        }
    );
    return PlainOperand{ Variant{ name } };
}

ExpResult TACBuilder::operator()(const parser::VariableExpression &v)
{
    return PlainOperand{ Variant{ v.identifier } };
}

ExpResult TACBuilder::operator()(const parser::CastExpression &c)
{
    Value result = VisitAndConvert(*c.expr);
    if (c.type.isVoid())
        return PlainOperand{ Variant{ "DUMMY" } };
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
        // We gather all the information and perform all side effects in one visit of the LHS
        const auto &[lhs, lhs_type] = VisitLHS(*u.expr);

        // Load current value if it's a pointer dereference
        Variant old_val = CreateTemporaryVariable(lhs_type.storedType());
        if (const PlainOperand *plain = std::get_if<PlainOperand>(&lhs))
            AddInstruction(Copy{ plain->val, old_val });
        else if (const DereferencedPointer *deref = std::get_if<DereferencedPointer>(&lhs))
            AddInstruction(Load{ deref->ptr, old_val });
        else if (const SubObject *sub = std::get_if<SubObject>(&lhs)) {
            AddInstruction(CopyFromOffset{
                .src_identifier = sub->base_identifier,
                .offset = sub->offset,
                .dst = old_val
            });
        }

        // Cast if needed
        Value typed_val = old_val;
        if (lhs_type != u.type)
            typed_val = CastValue(m_instructions, old_val, lhs_type, u.type);

        // Increment/Decrement
        Variant new_value = CreateTemporaryVariable(u.type.storedType());
        if (u.type.isPointer()) {
            auto ptr_type = u.type.getAs<PointerType>();
            assert(ptr_type);
            int offset = (u.op == UnaryOperator::Increment) ? 1 : -1;
            AddInstruction(AddPtr{
                .ptr = typed_val,
                .index = Constant{ MakeConstantValue(offset, Type{ BasicType::Long }) },
                .scale = ptr_type->referenced->size(m_typeTable),
                .dst = new_value
            });
        } else {
            AddInstruction(Binary{
                unaryToBinary(u.op),
                typed_val,
                Constant{ MakeConstantValue(1, u.type) },
                new_value
            });
        }

        // Cast back if needed
        Value result_to_store = new_value;
        if (u.type != lhs_type)
            result_to_store = CastValue(m_instructions, new_value, u.type, lhs_type.storedType());

        // Store the result back
        if (const PlainOperand *plain = std::get_if<PlainOperand>(&lhs))
            AddInstruction(Copy{ result_to_store, plain->val });
        else if (const DereferencedPointer *deref = std::get_if<DereferencedPointer>(&lhs))
            AddInstruction(Store{ result_to_store, deref->ptr });
        else if (const SubObject *sub = std::get_if<SubObject>(&lhs)) {
            AddInstruction(CopyToOffset{
                .src = result_to_store,
                .dst_identifier = sub->base_identifier,
                .offset = sub->offset
            });
        }

        // The return value depends on the operator type
        return PlainOperand{ u.postfix ? typed_val : new_value };
    }

    auto unary = Unary{};
    unary.op = u.op;
    unary.src = VisitAndConvert(*u.expr);
    unary.dst = CreateTemporaryVariable(u.type);
    AddInstruction(unary);
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
            AddInstruction(JumpIfZero{lhs_val, label_false});
            auto rhs = VisitAndConvert(*b.rhs);
            AddInstruction(JumpIfZero{rhs, label_false});
            AddInstruction(Copy{Constant(1), result});
            AddInstruction(Jump{label_end});
            AddInstruction(Label{label_false});
            AddInstruction(Copy{Constant(0), result});
            AddInstruction(Label{label_end});
        } else {
            AddInstruction(JumpIfNotZero{lhs_val, label_true});
            auto rhs = VisitAndConvert(*b.rhs);
            AddInstruction(JumpIfNotZero{rhs, label_true});
            AddInstruction(Copy{Constant(0), result});
            AddInstruction(Jump{label_end});
            AddInstruction(Label{label_true});
            AddInstruction(Copy{Constant(1), result});
            AddInstruction(Label{label_end});
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
                .scale = ptr_type->referenced->size(m_typeTable),
                .dst = CreateTemporaryVariable(b.type)
            };
            AddInstruction(add_ptr);
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
                AddInstruction(negate);
                auto ptr_type = b.type.getAs<PointerType>();
                assert(ptr_type);
                auto add_ptr = AddPtr{
                    .ptr = lhs,
                    .index = negate.dst,
                    .scale = ptr_type->referenced->size(m_typeTable),
                    .dst = CreateTemporaryVariable(lhs_type)
                };
                AddInstruction(add_ptr);
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
                AddInstruction(diff);

                auto ptr_type = lhs_type.getAs<PointerType>();
                assert(ptr_type);
                size_t elem_size = ptr_type->referenced->size(m_typeTable);
                auto result = Binary{
                    .op = BinaryOperator::Divide,
                    .src1 = diff.dst,
                    .src2 = Constant{
                        MakeConstantValue(static_cast<long>(elem_size), Type{ BasicType::Long })
                    },
                    .dst = CreateTemporaryVariable(Type{ BasicType::Long })
                };
                AddInstruction(result);
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
    AddInstruction(binary);
    return PlainOperand{ binary.dst };
}

ExpResult TACBuilder::operator()(const parser::AssignmentExpression &a)
{
    ExpResult left = std::visit(*this, *a.lhs);
    Value right = VisitAndConvert(*a.rhs);
    if (PlainOperand *plain = std::get_if<PlainOperand>(&left)) {
        AddInstruction(Copy{ right, plain->val });
        return left;
    } else if (DereferencedPointer *deref = std::get_if<DereferencedPointer>(&left)) {
        AddInstruction(Store{ right, deref->ptr });
        return PlainOperand{ right };
    } else if (SubObject *sub = std::get_if<SubObject>(&left)) {
        AddInstruction(CopyToOffset{
            .src = right,
            .dst_identifier = sub->base_identifier,
            .offset = sub->offset
        });
        return PlainOperand{ right };
    }
    assert(false);
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::CompoundAssignmentExpression &c)
{
    // We gather all the information and perform all side effects in one visit of the LHS
    const auto &[lhs, lhs_type] = VisitLHS(*c.lhs);

    // Load the value if lhs is a pointer dereference
    Variant old_val = CreateTemporaryVariable(lhs_type.storedType());
    if (const PlainOperand *plain = std::get_if<PlainOperand>(&lhs))
        AddInstruction(Copy{ plain->val, old_val });
    else if (const DereferencedPointer *deref = std::get_if<DereferencedPointer>(&lhs))
        AddInstruction(Load{ deref->ptr, old_val });
    else if (const SubObject *sub = std::get_if<SubObject>(&lhs)) {
        AddInstruction(CopyFromOffset{
            .src_identifier = sub->base_identifier,
            .offset =sub->offset,
            .dst = old_val
        });
    }

    // Cast the left side if needed
    Value typed_left = old_val;
    if (lhs_type != c.inner_type)
        typed_left = CastValue(m_instructions, typed_left, lhs_type, c.inner_type);

    Value rhs = VisitAndConvert(*c.rhs);

    // Perform the actual operation (binary or pointer arithmetic)
    Variant tmp = CreateTemporaryVariable(c.inner_type.storedType());
    if (const PointerType *pointer_type = c.inner_type.getAs<PointerType>()) {
        Value index = rhs;
        if (c.op == BinaryOperator::AssignSub) {
            Variant negated = CreateTemporaryVariable(GetType(rhs));
            AddInstruction(Unary{
                UnaryOperator::Negate,
                rhs,
                negated
            });
            index = negated;
        }
        AddInstruction(AddPtr{
            .ptr = typed_left,
            .index = index,
            .scale = pointer_type->referenced->size(m_typeTable),
            .dst = tmp
        });
    } else
        AddInstruction(Binary{ compoundToBinary(c.op), typed_left, rhs, tmp });

    // Cast back if needed
    Value result = tmp;
    if (c.inner_type != c.type)
        result = CastValue(m_instructions, tmp, c.inner_type, c.type.storedType());

    // Store the result if it was modified through a pointer
    if (const PlainOperand *plain = std::get_if<PlainOperand>(&lhs))
        AddInstruction(Copy{ result, plain->val });
    else if (const DereferencedPointer *deref = std::get_if<DereferencedPointer>(&lhs))
        AddInstruction(Store{ result, deref->ptr });
    else if (const SubObject *sub = std::get_if<SubObject>(&lhs)) {
        AddInstruction(CopyToOffset{
            .src = result,
            .dst_identifier = sub->base_identifier,
            .offset = sub->offset
        });
    }

    return PlainOperand{ result };
}

ExpResult TACBuilder::operator()(const parser::ConditionalExpression &c)
{
    auto label_end = MakeNameUnique("end");
    auto label_false_branch = MakeNameUnique("false_branch");
    Value result = c.type.isVoid() ? Variant{ "DUMMY" } : CreateTemporaryVariable(c.type);

    Value condition = VisitAndConvert(*c.condition);
    AddInstruction(JumpIfZero{ condition, label_false_branch });

    Value true_branch_value = VisitAndConvert(*c.trueBranch);
    if (!c.type.isVoid())
        AddInstruction(Copy{ true_branch_value, result });
    AddInstruction(Jump{ label_end });

    AddInstruction(Label{ label_false_branch });
    Value false_branch_value = VisitAndConvert(*c.falseBranch);
    if (!c.type.isVoid())
        AddInstruction(Copy{ false_branch_value, result });
    AddInstruction(Label{ label_end });

    return PlainOperand{ result };
}

ExpResult TACBuilder::operator()(const parser::FunctionCallExpression &f)
{
    auto ret = FunctionCall{};
    ret.identifier = f.identifier;
    for (auto &a : f.args)
        ret.args.push_back(VisitAndConvert(*a));
    if (!f.type.isVoid())
        ret.dst = CreateTemporaryVariable(f.type);
    AddInstruction(ret);
    return PlainOperand{ ret.dst ? *ret.dst : Variant{ "DUMMY" } };
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
        AddInstruction(GetAddress{ plain->val, dst });
        return PlainOperand{ dst };
    } else if (DereferencedPointer *deref = std::get_if<DereferencedPointer>(&inner))
        return PlainOperand{ deref->ptr };
    else if (SubObject *sub = std::get_if<SubObject>(&inner)) {
        Variant base_ptr = CreateTemporaryVariable(a.type);
        AddInstruction(GetAddress{ Variant{ sub->base_identifier }, base_ptr });
        if (sub->offset != 0) {
            AddInstruction(AddPtr{
                .ptr = base_ptr,
                .index = Constant{ MakeConstantValue(static_cast<long>(sub->offset), Type{ BasicType::Long }) },
                .scale = 1,
                .dst = base_ptr
            });
        }
        return PlainOperand{ base_ptr };
    }
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
        .scale = element_type->size(m_typeTable),
        .dst = CreateTemporaryVariable(Type{ PointerType{ .referenced = element_type } })
    };
    AddInstruction(add_ptr);
    return DereferencedPointer{ add_ptr.dst };
}

ExpResult TACBuilder::operator()(const parser::SizeOfExpression &s)
{
    return PlainOperand{
        Constant{
            MakeConstantValue(
                static_cast<long>(s.inner_type.size(m_typeTable)),
                Type{ BasicType::ULong })
        }
    };
}

ExpResult TACBuilder::operator()(const parser::SizeOfTypeExpression &s)
{
    return PlainOperand{
        Constant{ MakeConstantValue(static_cast<long>(s.operand.size(m_typeTable)), Type{ BasicType::ULong }) }
    };
}

ExpResult TACBuilder::operator()(const parser::DotExpression &d)
{
    Type type = GetExpressionType(*d.expr);
    const AggregateType *aggr_type = type.getAs<AggregateType>();
    assert(aggr_type);
    auto aggr_entry = m_typeTable->get(aggr_type->tag);
    size_t member_offset = aggr_entry->find(d.identifier)->offset;
    ExpResult inner_object = std::visit(*this, *d.expr);
    if (PlainOperand *plain = std::get_if<PlainOperand>(&inner_object)) {
        Variant *var = std::get_if<Variant>(&plain->val);
        assert(var);
        return SubObject{ var->name, member_offset };
    } else if (DereferencedPointer *deref = std::get_if<DereferencedPointer>(&inner_object)) {
        // TODO: If member_offset is 0, we don't need the AddPtr
        Variant dst_ptr = CreateTemporaryVariable(
            Type{ PointerType{ .referenced = std::make_shared<Type>(d.type) } }
        );
        AddInstruction(AddPtr{
            .ptr = deref->ptr,
            .index = Constant{ MakeConstantValue(static_cast<long>(member_offset), Type{ BasicType::Long }) },
            .scale = 1,
            .dst = dst_ptr
        });
        return DereferencedPointer{ dst_ptr };
    } else if (SubObject *sub = std::get_if<SubObject>(&inner_object))
        return SubObject{ sub->base_identifier, sub->offset + member_offset };
    assert(false);
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::ArrowExpression &a)
{
    const PointerType *pointer_type = GetExpressionType(*a.expr).getAs<PointerType>();
    assert(pointer_type);
    const AggregateType *aggr_type = pointer_type->referenced->getAs<AggregateType>();
    assert(aggr_type);
    // TODO: If member_offset is 0, we don't need the AddPtr
    auto aggr_entry = m_typeTable->get(aggr_type->tag);
    size_t member_offset = aggr_entry->find(a.identifier)->offset;
    Value base_ptr = VisitAndConvert(*a.expr);
    Variant dst_ptr = CreateTemporaryVariable(
        Type{ PointerType{ .referenced = std::make_shared<Type>(a.type) } }
    );
    AddInstruction(AddPtr{
        .ptr = base_ptr,
        .index = Constant{ MakeConstantValue(static_cast<long>(member_offset), Type{ BasicType::Long }) },
        .scale = 1,
        .dst = dst_ptr
    });
    return DereferencedPointer{ dst_ptr };
}

ExpResult TACBuilder::operator()(const parser::ReturnStatement &r)
{
    AddInstruction(Return{
        .val = r.expr ? std::optional{ VisitAndConvert(*r.expr) } : std::nullopt
    });
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::IfStatement &i)
{
    Value condition = VisitAndConvert(*i.condition);
    auto label_end = MakeNameUnique("end");
    if (i.falseBranch) {
        auto label_else = MakeNameUnique("else");
        AddInstruction(JumpIfZero{ condition, label_else });
        std::visit(*this, *i.trueBranch);
        AddInstruction(Jump{ label_end });
        AddInstruction(Label{ label_else });
        std::visit(*this, *i.falseBranch);
    } else {
        AddInstruction(JumpIfZero{ condition, label_end });
        std::visit(*this, *i.trueBranch);
    }
    AddInstruction(Label{ label_end });
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::GotoStatement &g)
{
    AddInstruction(Jump{ g.label });
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::LabeledStatement &l)
{
    AddInstruction(Label{ l.label });
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
    AddInstruction(Jump{ std::format("break_{}", b.label) });
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::ContinueStatement &c)
{
    AddInstruction(Jump{ std::format("continue_{}", c.label) });
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::WhileStatement &w)
{
    auto label_continue = std::format("continue_{}", w.label);
    auto label_break = std::format("break_{}", w.label);

    AddInstruction(Label{ label_continue });
    Value condition = VisitAndConvert(*w.condition);
    AddInstruction(JumpIfZero{ condition, label_break });
    std::visit(*this, *w.body);
    AddInstruction(Jump{ label_continue });
    AddInstruction(Label{ label_break });
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::DoWhileStatement &d)
{
    auto label_start = std::format("start_{}", d.label);

    AddInstruction(Label{ label_start });
    std::visit(*this, *d.body);
    AddInstruction(Label{ std::format("continue_{}", d.label) });
    Value condition = VisitAndConvert(*d.condition);
    AddInstruction(JumpIfNotZero{ condition, label_start });
    AddInstruction(Label{ std::format("break_{}", d.label) });
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::ForStatement &f)
{
    auto label_start = std::format("start_{}", f.label);
    auto label_break = std::format("break_{}", f.label);

    if (f.init)
        std::visit(*this, *f.init);
    AddInstruction(Label{ label_start });
    Value condition;
    if (f.condition)
        condition = VisitAndConvert(*f.condition);
    else
        condition = Constant { 1 };
    AddInstruction(JumpIfZero{ condition, label_break });
    std::visit(*this, *f.body);
    AddInstruction(Label{ std::format("continue_{}", f.label) });
    if (f.update)
        std::visit(*this, *f.update);
    AddInstruction(Jump{ label_start });
    AddInstruction(Label{ label_break });
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
        AddInstruction(binary);
        AddInstruction(JumpIfZero{
            binary.dst,
            std::format("case_{}_{}", s.label, toLabel(c))
        });
    }
    if (s.hasDefault)
        AddInstruction(Jump{ std::format("default_{}", s.label) });
    else
        AddInstruction(Jump{ label_break });
    std::visit(*this, *s.body);
    AddInstruction(Label{ label_break });
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::CaseStatement &c)
{
    AddInstruction(Label{ c.label });
    std::visit(*this, *c.statement);
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::DefaultStatement &d)
{
    AddInstruction(Label{ d.label });
    std::visit(*this, *d.statement);
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::FunctionDeclaration &f)
{
    // We only handle definitions, not declarations
    if (!f.body)
        return std::monostate();

    // Insert it first to its final place, then use that object to avoid all copy/move
    m_topLevel->emplace_back(FunctionDefinition{});
    FunctionDefinition &func = std::get<FunctionDefinition>(m_topLevel->back());

    func.name = f.name;
    // Nothing to do here with parameters. They already have unique names
    // after semantic analysis and they will be pseudo-registers in ASM.
    func.params = f.params;

    if (auto body = std::get_if<parser::BlockStatement>(f.body.get())) {
        TACBuilder builder(m_context);
        builder.ConvertFunctionBlock(body->items, func.blocks);
        // Avoid undefined behavior in functions where there is no return.
        // If it already had a return, this extra one won't be executed
        // and will be optimised out in later stages.
        // After ConvertFunctionBlock(), the last CFGBlock is an empty exit block;
        // we need the block before that one.
        auto last_block = std::prev(func.blocks.end(), 2);
        const FunctionType *type = f.type.getAs<FunctionType>();
        assert(type);
        if (!type->ret->isScalar())
            last_block->instructions.push_back(Return{ });
        else
            last_block->instructions.push_back(Return{
                Constant{ MakeConstantValue(0, *(type->ret)) }
            });
    }
    if (const SymbolEntry *entry = m_context->symbolTable->get(f.name))
        func.global = entry->attrs.global;
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::VariableDeclaration &v)
{
    const SymbolEntry *entry = m_context->symbolTable->get(v.identifier);
    assert(entry);

    // We will move static variable declarations to the top level in a later step
    if (entry->attrs.type == IdentifierAttributes::Static)
        return std::monostate();

    // We discard declarations, but we handle initializations
    if (!v.init)
        return std::monostate();

    size_t offset = 0;
    if (entry->type.isScalar())
        EmitRuntimeInitForScalar(v.init.get(), v.identifier, entry->type);
    else
        EmitRuntimeInitForNonScalar(v.init.get(), v.identifier, entry->type, offset);
    return std::monostate();
}

ExpResult TACBuilder::operator()(const parser::AggregateTypeDeclaration &)
{
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

TACBuilder::TACBuilder(Context *context)
    : m_context(context)
    , m_typeTable(context->typeTable.get())
{
    assert(m_context);
}

void TACBuilder::ConvertTopLevel(
    const std::vector<parser::Declaration> &list,
    std::list<tac::TopLevel> &top_level_out)
{
    m_topLevel = &top_level_out;
    for (auto &i : list)
        std::visit(*this, i);
    ProcessStaticSymbols();
}

void TACBuilder::ConvertFunctionBlock(
    const std::vector<parser::BlockItem> &list,
    std::list<CFGBlock> &block_list_out)
{
    m_blocks = &block_list_out;
    for (auto &i : list)
        std::visit(*this, i);
    FinalizeControlFlowBlocks();
}

void TACBuilder::ProcessStaticSymbols()
{
    for (const auto &[name, entry] : m_context->symbolTable->m_table) {
        if (entry.attrs.type == IdentifierAttributes::Static) {
            if (std::holds_alternative<Tentative>(entry.attrs.init)) {
                std::vector<ConstantValue> initializer;
                if (entry.type.isArray() || entry.type.isAggregate())
                    initializer.push_back(ZeroBytes{ entry.type.size(m_typeTable) });
                else
                    initializer.push_back(MakeConstantValue(0, entry.type));
                m_topLevel->push_back(StaticVariable{
                    .name = name,
                    .type = entry.type,
                    .global = entry.attrs.global,
                    .list = initializer
                });
            } else if (auto initial = std::get_if<Initial>(&entry.attrs.init)) {
                m_topLevel->push_back(StaticVariable{
                    .name = name,
                    .type = entry.type,
                    .global = entry.attrs.global,
                    .list = initial->list
                });
            }
        } else if (entry.attrs.type == IdentifierAttributes::Constant) {
            m_topLevel->push_back(StaticConstant{
                .name = name,
                .type = entry.type,
                .static_init = entry.attrs.static_init
            });
        }
    }
}

void TACBuilder::CommitBlock()
{
    m_blocks->emplace_back(CFGBlock{
        .instructions = std::move(m_instructions),
        .id = m_nextBlockId++
    });
}

void TACBuilder::FinalizeControlFlowBlocks()
{
    // Small functions don't even have one complete block; so we create one
    if (m_blocks->empty() || !m_instructions.empty())
        CommitBlock();

    // Add entry block
    m_blocks->emplace_front(CFGBlock{
        .id = 0
    });

    // Extra block for the extra return instruction at the end of functions
    m_blocks->emplace_back(CFGBlock{
        .id = m_blocks->size()
    });

    // Add exit block
    m_blocks->emplace_back(CFGBlock{
        .id = m_blocks->size()
    });
}

}; // tac
