#include "asm_builder.h"
#include <format>
#include <ranges>

namespace assembly {

static const std::array<Register, 6> s_argRegisters = {
    DI, SI, DX, CX, R8, R9
};

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

Operand ASMBuilder::operator()(const tac::FunctionCall &f)
{
    // The first six arguments are in registers, other on the stack
    std::vector<tac::Value> register_args = {};
    std::vector<tac::Value> stack_args = {};
    for (size_t i = 0; i < f.args.size(); i++) {
        if (i < 6)
            register_args.push_back(f.args[i]);
        else
            stack_args.push_back(f.args[i]);
    }

    // Adjust stack alignment to 16 bytes
    size_t stack_padding = (stack_args.size() % 2 == 0) ? 0 : 8;
    if (stack_padding != 0)
        m_instructions.push_back(AllocateStack{ stack_padding });

    // Move arguments into registers
    size_t reg_index = 0;
    for (auto &arg : register_args) {
        Register r = s_argRegisters[reg_index++];
        m_instructions.push_back(Mov{
            std::visit(*this, arg),
            Reg { r }
        });
    }

    // Push remaining arguments to the stack (in reverse order)
    for (auto &arg : std::views::reverse(stack_args)) {
        auto asm_arg = std::visit(*this, arg);
        if (std::holds_alternative<Reg>(asm_arg) || std::holds_alternative<Imm>(asm_arg))
            m_instructions.push_back(Push{ asm_arg });
        else {
            m_instructions.push_back(Mov{ asm_arg, Reg{ AX } });
            m_instructions.push_back(Push{ Reg{ AX, 8 } });
        }
    }

    // Call the function
    m_instructions.push_back(Call{ f.identifier });

    // The callee clears the stack
    size_t bytes_to_remove = 8 * stack_args.size() + stack_padding;
    if (bytes_to_remove != 0)
        m_instructions.push_back(DeallocateStack{ bytes_to_remove });

    // Retrieve the return value from AX
    m_instructions.push_back(Mov{
        Reg{ AX },
        std::visit(*this, f.dst)
    });

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

Operand ASMBuilder::operator()(const tac::FunctionDefinition &f)
{
    auto func = Function{};
    func.name = f.name;

    // We copy each of the parameters into the current stack frame
    // to make life easier. This can be optimised out later.
    // First six parameters are in registers
    for (size_t i = 0; i < f.params.size() && i < 6; i++) {
        func.instructions.push_back(Mov{
            Reg{ s_argRegisters[i] },
            Pseudo{ f.params[i] }
        });
    }
    // The others are on the stack
    // The 8 byte RBP are already on the stack (prologue)
    size_t stack_offset = 16;
    for (size_t i = 6; i < f.params.size(); i++) {
        func.instructions.push_back(Mov{
            Stack{ (int)stack_offset },
            Pseudo{ f.params[(size_t)i] }
        });
        stack_offset += 8;
    }

    // Body
    ASMBuilder builder;
    auto body_instructions = builder.ConvertInstructions(f.inst);
    std::copy(body_instructions.begin(),
                body_instructions.end(),
                std::back_inserter(func.instructions));
    m_topLevel.push_back(std::move(func));
    return std::monostate();
}

std::list<TopLevel> ASMBuilder::ConvertTopLevel(const std::vector<tac::TopLevel> instructions)
{
    m_topLevel.clear();
    for (auto &inst : instructions)
        std::visit(*this, inst);
    return std::move(m_topLevel);
}

std::list<Instruction> ASMBuilder::ConvertInstructions(const std::vector<tac::Instruction> instructions)
{
    m_instructions.clear();
    for (auto &inst : instructions)
        std::visit(*this, inst);
    return std::move(m_instructions);
}

}; // assembly
