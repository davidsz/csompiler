#include "asm_builder.h"

namespace assembly {

Operand ASMBuilder::operator()(const tac::FunctionDefinition &f)
{
    auto func = Function{};
    func.name = f.name;
    ASMBuilder builder;
    func.instructions = builder.Convert(f.inst);
    m_instructions.push_back(std::move(func));
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Return &r)
{
    m_instructions.push_back(Mov{
        std::visit(*this, r.val),
        Reg{"eax"}
    });
    m_instructions.push_back(Ret{});
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Unary &u)
{
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

Operand ASMBuilder::operator()(const tac::Constant &c)
{
    return Imm{ c.value };
}

Operand ASMBuilder::operator()(const tac::Variant &v)
{
    return Pseudo{ v.name };
}

std::vector<Instruction> ASMBuilder::Convert(const std::vector<tac::Instruction> instructions)
{
    for (auto &inst : instructions)
        std::visit(*this, inst);
    return std::move(m_instructions);
}

}; // assembly
