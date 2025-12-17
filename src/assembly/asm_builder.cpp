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

static uint8_t getBytesOfWordType(WordType type)
{
    switch (type) {
    case WordType::Longword:
        return 4;
    case WordType::Quadword:
        return 8;
    default:
        return 4;
    }
}

ASMBuilder::ASMBuilder(std::shared_ptr<SymbolTable> symbolTable)
    : m_symbolTable(symbolTable)
{
}

WordType ASMBuilder::GetWordType(const tac::Value &value)
{
    if (auto c = std::get_if<tac::Constant>(&value)) {
        if (std::holds_alternative<long>(c->value))
            return WordType::Quadword;
        else
            return WordType::Longword;
    } else if (auto v = std::get_if<tac::Variant>(&value)) {
        const SymbolEntry &entry = (*m_symbolTable)[v->name];
        if (entry.type.isBasic(BasicType::Long))
            return WordType::Quadword;
        else
            return WordType::Longword;
    } else {
        assert(false);
        return Longword;
    }
}

void ASMBuilder::Comment(std::list<Instruction> &i, const std::string &text)
{
    if (m_commentsEnabled)
        i.push_back(assembly::Comment{ text });
}

Operand ASMBuilder::operator()(const tac::Return &r)
{
    WordType type = GetWordType(r.val);
    m_instructions.push_back(Mov{
        std::visit(*this, r.val),
        Reg{Register::AX, getBytesOfWordType(type)},
        type
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
            std::visit(*this, u.src),
            GetWordType(u.src)
        });
        m_instructions.push_back(Mov{
            Imm{0},
            std::visit(*this, u.dst),
            GetWordType(u.dst)
        });
        m_instructions.push_back(SetCC{
            "e",
            std::visit(*this, u.dst)
        });
        return std::monostate();
    }

    m_instructions.push_back(Mov{
        std::visit(*this, u.src),
        std::visit(*this, u.dst),
        GetWordType(u.src)
    });
    m_instructions.push_back(Unary{
        toASMUnaryOperator(u.op),
        std::visit(*this, u.dst),
        GetWordType(u.dst)
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
            std::visit(*this, b.dst),
            GetWordType(b.dst)
        });
        m_instructions.push_back(Binary{
            op,
            std::visit(*this, b.src2),
            std::visit(*this, b.dst),
            GetWordType(b.dst)
        });
        return std::monostate();
    }

    // Division, Remainder
    if (b.op == BinaryOperator::Divide || b.op == BinaryOperator::Remainder) {
        WordType wordType = GetWordType(b.dst);
        uint8_t bytes = getBytesOfWordType(wordType);
        m_instructions.push_back(Mov{
            std::visit(*this, b.src1),
            Reg{Register::AX, bytes},
            wordType
        });
        m_instructions.push_back(Cdq{
            wordType
        });
        m_instructions.push_back(Idiv{
            std::visit(*this, b.src2),
            wordType
        });
        m_instructions.push_back(Mov{
            (b.op == BinaryOperator::Divide
                ? Reg{Register::AX, bytes} : Reg{Register::DX, bytes}),
            std::visit(*this, b.dst),
            wordType
        });
        return std::monostate();
    }

    // Equal, NotEqual, LessThan, LessOrEqual, GreaterThan, GreaterOrEqual
    if (isRelationOperator(b.op)) {
        WordType wordType = GetWordType(b.dst);
        // Comparing arguments; result stored in RFLAGS
        m_instructions.push_back(Cmp{
            std::visit(*this, b.src2),
            std::visit(*this, b.src1),
            wordType
        });
        // Null out the destination before placing the condition result in
        m_instructions.push_back(Mov{
            Imm{0},
            std::visit(*this, b.dst),
            wordType
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
        GetWordType(c.dst)
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
        std::visit(*this, j.condition),
        GetWordType(j.condition)
    });
    m_instructions.push_back(JmpCC{"e", j.target});
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::JumpIfNotZero &j)
{
    m_instructions.push_back(Cmp{
        Imm{0},
        std::visit(*this, j.condition),
        GetWordType(j.condition)
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

    // Adjust stack alignment to 16 bytes and allocate stack
    size_t stack_padding = (stack_args.size() % 2 == 0) ? 0 : 8;
    if (stack_padding != 0) {
        Comment(m_instructions, "Allocating stack");
        m_instructions.push_back(Binary{
            ASMBinaryOperator::Sub_AB,
            Imm{ static_cast<int>(stack_padding) },
            Reg{ SP, 8 },
            WordType::Quadword
        });
    }

    // Move arguments into registers
    size_t reg_index = 0;
    if (register_args.size() > 0)
        Comment(m_instructions, "Moving the first six arguments into registers");
    for (auto &arg : register_args) {
        WordType type = GetWordType(arg);
        Register r = s_argRegisters[reg_index++];
        m_instructions.push_back(Mov{
            std::visit(*this, arg),
            Reg{ r, getBytesOfWordType(type) },
            type
        });
    }

    // Push remaining arguments to the stack (in reverse order)
    if (stack_args.size() > 0)
        Comment(m_instructions, "Pushing the rest of the arguments onto the stack");
    for (auto &arg : std::views::reverse(stack_args)) {
        auto asm_arg = std::visit(*this, arg);
        if (std::holds_alternative<Reg>(asm_arg)
            || std::holds_alternative<Imm>(asm_arg)
            || GetWordType(arg) == WordType::Quadword)
            m_instructions.push_back(Push{ asm_arg });
        else {
            m_instructions.push_back(Mov{
                asm_arg,
                Reg{ AX, 4 },
                WordType::Longword
            });
            m_instructions.push_back(Push{ Reg{ AX, 8 } });
        }
    }

    // Call the function
    m_instructions.push_back(Call{ f.identifier });

    // The callee clears the stack
    size_t bytes_to_remove = 8 * stack_args.size() + stack_padding;
    if (bytes_to_remove != 0) {
        Comment(m_instructions, "Clearing the stack");
        m_instructions.push_back(Binary{
            ASMBinaryOperator::Add_AB,
            Imm{ static_cast<int>(bytes_to_remove) },
            Reg{ SP, 8 },
            WordType::Quadword
        });
    }

    // Retrieve the return value from AX
    Comment(m_instructions, "The return value is in AX");
    WordType type = GetWordType(f.dst);
    m_instructions.push_back(Mov{
        Reg{ AX, getBytesOfWordType(type) },
        std::visit(*this, f.dst),
        type
    });

    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::SignExtend &s)
{
    m_instructions.push_back(Movsx{
        std::visit(*this, s.src),
        std::visit(*this, s.dst)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Truncate &t)
{
    m_instructions.push_back(Mov{
        std::visit(*this, t.src),
        std::visit(*this, t.dst),
        WordType::Longword
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Constant &c)
{
    if (auto int_value = std::get_if<int>(&c.value))
        return Imm{ *int_value };
    else if (auto long_value = std::get_if<long>(&c.value))
        return Imm{ *long_value };
    assert(false);
    return Imm{ 0 };
}

Operand ASMBuilder::operator()(const tac::Variant &v)
{
    return Pseudo{ v.name };
}

Operand ASMBuilder::operator()(const tac::FunctionDefinition &f)
{
    auto func = Function{};
    func.name = f.name;
    func.global = f.global;

    // We copy each of the parameters into the current stack frame
    // to make life easier. This can be optimised out later.
    // First six parameters are in registers
    if (f.params.size() > 0)
        Comment(func.instructions, "Getting the first six parameters from registers");
    for (size_t i = 0; i < f.params.size() && i < 6; i++) {
        WordType type = WordType::Longword;
        // TODO: Refactor this duplication
        if (m_symbolTable->contains(f.params[i])) {
            const SymbolEntry &entry = (*m_symbolTable)[f.params[i]];
            if (const BasicType *basic_type = entry.type.getAs<BasicType>())
                type = (*basic_type == BasicType::Int)
                    ? WordType::Longword : WordType::Quadword;
        }
        func.instructions.push_back(Mov{
            Reg{ s_argRegisters[i], getBytesOfWordType(type) },
            Pseudo{ f.params[i] },
            type
        });
    }
    // The others are on the stack
    // The 8 byte RBP are already on the stack (prologue)
    if (f.params.size() > 6)
        Comment(func.instructions, "Getting the rest of the arguments from the stack");
    size_t stack_offset = 16;
    for (size_t i = 6; i < f.params.size(); i++) {
        std::string param_name = f.params[(size_t)i];
        WordType type = WordType::Longword;
        // TODO: Refactor this duplication
        if (m_symbolTable->contains(param_name)) {
            const SymbolEntry &entry = (*m_symbolTable)[param_name];
            if (const BasicType *basic_type = entry.type.getAs<BasicType>())
                type = (*basic_type == BasicType::Int)
                    ? WordType::Longword : WordType::Quadword;
        }
        func.instructions.push_back(Mov{
            Stack{ (int)stack_offset },
            Pseudo{ param_name },
            type
        });
        stack_offset += 8;
    }

    // Body
    ASMBuilder builder(m_symbolTable);
    auto body_instructions = builder.ConvertInstructions(f.inst);
    std::copy(body_instructions.begin(),
                body_instructions.end(),
                std::back_inserter(func.instructions));
    m_topLevel.push_back(std::move(func));
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::StaticVariable &s)
{
    m_topLevel.push_back(StaticVariable{
        .name = s.name,
        .global = s.global,
        .init = s.init,
        .alignment = std::holds_alternative<long>(s.init) ? 8 : 4
    });
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
