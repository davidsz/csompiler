#include "asm_builder.h"
#include <format>

namespace assembly {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
static bool isRelationOperator(BinaryOperator op)
{
    switch (op) {
        case BinaryOperator::Equal:
        case BinaryOperator::NotEqual:
        case BinaryOperator::LessThan:
        case BinaryOperator::LessOrEqual:
        case BinaryOperator::GreaterThan:
        case BinaryOperator::GreaterOrEqual:
            return true;
        default:
            return false;
    }
}

static std::string toConditionCode(BinaryOperator op)
{
    switch (op) {
        case BinaryOperator::Equal:
            return "e";
        case BinaryOperator::NotEqual:
            return "ne";
        case BinaryOperator::LessThan:
            return "l";
        case BinaryOperator::LessOrEqual:
            return "le";
        case BinaryOperator::GreaterThan:
            return "g";
        case BinaryOperator::GreaterOrEqual:
            return "ge";
        default:
            return "UNKNOWN_COND";
    }
}
#pragma clang diagnostic pop

Operand ASMBuilder::operator()(const tac::FunctionDefinition &f)
{
    auto func = Function{};
    func.name = std::format("_{}", f.name);
    ASMBuilder builder;
    func.instructions = builder.Convert(f.inst);
    m_instructions.push_back(std::move(func));
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Return &r)
{
    m_instructions.push_back(Mov{
        std::visit(*this, r.val),
        Reg{Register::AX}
    });
    m_instructions.push_back(Ret{});
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Unary &u)
{
    if (u.op == UnaryOperator::Not) {
        // !x is equivalent to x==0, so we implement it as a binary comparison
        m_instructions.push_back(Cmp{
            Imm{0},
            std::visit(*this, u.src)
        });
        m_instructions.push_back(Mov{
            Imm{0},
            std::visit(*this, u.dst)
        });
        m_instructions.push_back(SetCC{
            "e",
            std::visit(*this, u.dst)
        });
        return std::monostate();
    }

    m_instructions.push_back(Mov{
        std::visit(*this, u.src),
        std::visit(*this, u.dst)
    });
    m_instructions.push_back(Unary{
        toASMUnaryOperator(u.op),
        std::visit(*this, u.dst)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Binary &b)
{
    ASMBinaryOperator op = toASMBinaryOperator(b.op);
    // Easier binary operators with common format:
    // Add, Subtract, Multiply...
    if (op != ASMBinaryOperator::Unknown_AB) {
        m_instructions.push_back(Mov{
            std::visit(*this, b.src1),
            std::visit(*this, b.dst)
        });
        m_instructions.push_back(Binary{
            op,
            std::visit(*this, b.src2),
            std::visit(*this, b.dst)
        });
        return std::monostate();
    }

    // Division, Remainder
    if (b.op == BinaryOperator::Divide || b.op == BinaryOperator::Remainder) {
        m_instructions.push_back(Mov{
            std::visit(*this, b.src1),
            Reg{Register::AX}
        });
        m_instructions.push_back(Cdq{});
        m_instructions.push_back(Idiv{std::visit(*this, b.src2)});
        m_instructions.push_back(Mov{
            (b.op == BinaryOperator::Divide ? Reg{Register::AX} : Reg{Register::DX}),
            std::visit(*this, b.dst),
        });
        return std::monostate();
    }

    // Equal, NotEqual, LessThan, LessOrEqual, GreaterThan, GreaterOrEqual
    if (isRelationOperator(b.op)) {
        // Comparing arguments; result stored in RFLAGS
        m_instructions.push_back(Cmp{
            std::visit(*this, b.src2),
            std::visit(*this, b.src1)
        });
        // Null out the destination before placing the condition result in
        m_instructions.push_back(Mov{
            Imm{0},
            std::visit(*this, b.dst)
        });
        // Conditionally set the lowest byte of the destination according to RFLAGS
        m_instructions.push_back(SetCC{
            toConditionCode(b.op),
            std::visit(*this, b.dst)
        });
        return std::monostate();
    }

    assert(false);
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Copy &c)
{
    m_instructions.push_back(Mov{
        std::visit(*this, c.src),
        std::visit(*this, c.dst),
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Jump &j)
{
    m_instructions.push_back(Jmp{ j.target });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::JumpIfZero &j)
{
    m_instructions.push_back(Cmp{
        Imm{0},
        std::visit(*this, j.condition)
    });
    m_instructions.push_back(JmpCC{"e", j.target});
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::JumpIfNotZero &j)
{
    m_instructions.push_back(Cmp{
        Imm{0},
        std::visit(*this, j.condition)
    });
    m_instructions.push_back(JmpCC{"ne", j.target});
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Label &l)
{
    m_instructions.push_back(Label{ l.identifier });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::FunctionCall &)
{
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Constant &c)
{
    return Imm{ c.value };
}

Operand ASMBuilder::operator()(const tac::Variant &v)
{
    return Pseudo{ v.name };
}

std::list<Instruction> ASMBuilder::Convert(const std::vector<tac::TopLevel> instructions)
{
    for (auto &inst : instructions)
        std::visit(*this, inst);
    return std::move(m_instructions);
}

std::list<Instruction> ASMBuilder::Convert(const std::vector<tac::Instruction> instructions)
{
    for (auto &inst : instructions)
        std::visit(*this, inst);
    return std::move(m_instructions);
}

}; // assembly
