#include "asm_builder.h"
#include "common/context.h"
#include "common/labeling.h"
#include <format>
#include <ranges>

namespace assembly {

static const std::array<Register, 6> s_intArgRegisters = {
    DI, SI, DX, CX, R8, R9
};

static const std::array<Register, 8> s_doubleArgRegisters = {
    XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7
};

DIAG_PUSH
DIAG_IGNORE("-Wswitch-enum")
static std::string toConditionCode(BinaryOperator op, bool another)
{
    switch (op) {
        case BinaryOperator::Equal:
            return "e";
        case BinaryOperator::NotEqual:
            return "ne";
        case BinaryOperator::LessThan:
            return another ? "b" : "l"; // less, below
        case BinaryOperator::LessOrEqual:
            return another ? "be" : "le";
        case BinaryOperator::GreaterThan:
            return another ? "a" : "g"; // greater, above
        case BinaryOperator::GreaterOrEqual:
            return another ? "ae" : "ge";
        default:
            return "UNKNOWN_COND";
    }
}
DIAG_POP

static std::vector<WordType> getMemoryFragments(size_t size)
{
    std::vector<WordType> ret;
    while (size >= 8) {
        ret.push_back(Quadword);
        size -= 8;
    }
    if (size >= 4) {
        ret.push_back(Longword);
        size -= 4;
    }
    while (size-- > 0)
        ret.push_back(Byte);
    return ret;
}

/*
enum StructClass {
    INTEGER,
    SSE,
    MEMORY
};

static std::vector<StructClass> classifyStruct(TypeTable::StructEntry *entry)
{
    if (entry->members.empty()) {
        assert(false);
        return {};
    }

    // The struct can't fit on two registers
    size_t size = entry->size;
    if (size > 16) {
        std::vector<StructClass> ret;
        while (size > 0) {
            ret.push_back(MEMORY);
            size -= 8;
        }
        return ret;
    }

    // Classify the two registers
    bool first_is_double = entry->members.front().type.storedType().isBasic(Double);
    bool last_is_double = entry->members.back().type.storedType().isBasic(Double);
    if (size > 8) {
        if (first_is_double && last_is_double)
            return { SSE, SSE };
        if (first_is_double)
            return { SSE, INTEGER };
        if (last_is_double)
            return { INTEGER, SSE };
        return { INTEGER, INTEGER };
    } else if (first_is_double)
        return { SSE };
    else
        return { INTEGER };
}
*/

template<typename T>
struct ClassifiedParams {
    std::vector<T> int_regs;
    std::vector<T> double_regs;
    std::vector<T> stack;
};

template<typename T, typename GetTypeFn>
static ClassifiedParams<T> classifyParameters(
    const std::vector<T> &values,
    GetTypeFn getType)
{
    ClassifiedParams<T> result;
    for (const auto &v : values) {
        WordType type = getType(v);
        if (type == WordType::Doubleword) {
            if (result.double_regs.size() < 8)
                result.double_regs.push_back(v);
            else
                result.stack.push_back(v);
        } else {
            if (result.int_regs.size() < 6)
                result.int_regs.push_back(v);
            else
                result.stack.push_back(v);
        }
    }
    return result;
}

ASMBuilder::ASMBuilder(Context *context, std::shared_ptr<ConstantMap> constants)
    : m_context(context)
    , m_typeTable(context->typeTable.get())
    , m_symbolTable(context->symbolTable.get())
    , m_constants(constants)
{
    assert(m_context);
}

Type ASMBuilder::GetType(const tac::Value &value)
{
    if (auto c = std::get_if<tac::Constant>(&value))
        return getType(c->value);
    auto v = std::get_if<tac::Variant>(&value);
    assert(v);
    auto entry = m_symbolTable->get(v->name);
    return entry->type;
}

BasicType ASMBuilder::GetBasicType(const tac::Value &value)
{
    return *GetType(value).getAs<BasicType>();
}

WordType ASMBuilder::GetWordType(const tac::Value &value)
{
    return GetType(value).wordType();
}

std::optional<std::pair<std::string, const TypeTable::StructEntry *>>
ASMBuilder::GetStructEntry(const tac::Value &value)
{
    if (auto v = std::get_if<tac::Variant>(&value))
        std::make_pair(v->name, m_typeTable->get(v->name));
    return std::nullopt;
}

void ASMBuilder::Comment(std::list<Instruction> &i, const std::string &text)
{
    if (m_commentsEnabled)
        i.push_back(assembly::Comment{ text });
}

Operand ASMBuilder::operator()(const tac::Return &r)
{
    if (!r.val) {
        m_instructions.push_back(Ret{ });
        return std::monostate();
    }

    WordType type = GetWordType(*r.val);
    if (type == Doubleword) {
        m_instructions.push_back(Mov{
            std::visit(*this, *r.val),
            Reg{ XMM0, 8 },
            Doubleword
        });
    } else {
        m_instructions.push_back(Mov{
            std::visit(*this, *r.val),
            Reg{ AX, GetBytesOfWordType(type) },
            type
        });
    }
    m_instructions.push_back(Ret{ });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Unary &u)
{
    WordType srcType = GetWordType(u.src);
    WordType dstType = GetWordType(u.dst);
    Operand src = std::visit(*this, u.src);
    Operand dst = std::visit(*this, u.dst);

    // !x is equivalent to x==0, so we implement it as a binary comparison
    if (u.op == UnaryOperator::Not) {
        if (srcType == Doubleword) {
            // Label for handling a potentional unordered (NaN) case
            std::string end_label = MakeNameUnique("end_not");
            m_instructions.push_back(Binary{ BWXor_AB, Reg{ XMM0, 8 }, Reg{ XMM0, 8 }, srcType });
            m_instructions.push_back(Cmp{ Reg{ XMM0, 8 }, src, srcType });
            // NaN evaluates to non-zero, !NaN is zero,
            // so we always zero the destination out
            m_instructions.push_back(Mov{ Imm{ 0 }, dst, dstType });
            // Perform the SetCC instruction only for ordered operations
            m_instructions.push_back(JmpCC{ "p", end_label });
            m_instructions.push_back(SetCC{ "e", dst });
            m_instructions.push_back(Label{ end_label });
        } else {
            m_instructions.push_back(Cmp{ Imm{ 0 }, src, srcType });
            m_instructions.push_back(Mov{ Imm{ 0 }, dst, dstType });
            m_instructions.push_back(SetCC{ "e", dst });
        }
        return std::monostate();
    }

    if (u.op == Negate && srcType == Doubleword) {
        // Negating in floating point: XOR with -0.0
        std::string minus_zero = AddConstant(
            ConstantValue{ static_cast<double>(-0.0) },
            GenerateTempVariableName()
        );
        m_instructions.push_back(Mov{ src, dst, Doubleword });
        m_instructions.push_back(Binary{
            BWXor_AB,
            Data{ minus_zero },
            dst,
            Doubleword
        });
        return std::monostate();
    }

    ASMUnaryOperator op = toASMUnaryOperator(u.op);
    assert(op != Unknown_AU);
    m_instructions.push_back(Mov{ src, dst, srcType });
    m_instructions.push_back(Unary{ op, dst, dstType });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Binary &b)
{
    // The src1 and src2 have common type, dst is always int (Longword)
    WordType srcType = GetWordType(b.src1);
    WordType dstType = GetWordType(b.dst);
    bool isSigned = GetType(b.src1).isSigned();
    ASMBinaryOperator op = toASMBinaryOperator(b.op, srcType, isSigned);
    Operand src1 = std::visit(*this, b.src1);
    Operand src2 = std::visit(*this, b.src2);
    Operand dst = std::visit(*this, b.dst);

    // Easier binary operators with common format:
    // Add, Subtract, Multiply...
    if (op != ASMBinaryOperator::Unknown_AB) {
        Comment(m_instructions, std::format("Binary operator {}", toString(b.op)));
        m_instructions.push_back(Mov{ src1, dst, srcType });
        m_instructions.push_back(Binary{ op, src2, dst, srcType });
        Comment(m_instructions, "---");
        return std::monostate();
    }

    // Division, Remainder
    if (b.op == BinaryOperator::Divide || b.op == BinaryOperator::Remainder) {
        Comment(m_instructions, std::format("Binary operator {} ({})",
            toString(b.op), isSigned ? "signed" : "unsigned"));
        uint8_t bytes = GetBytesOfWordType(srcType);
        m_instructions.push_back(Mov{ src1, Reg{ AX, bytes }, srcType });
        if (isSigned) {
            m_instructions.push_back(Cdq{ srcType });
            m_instructions.push_back(Idiv{ src2, srcType });
        } else {
            m_instructions.push_back(Mov{ Imm{ 0 }, Reg{ DX, bytes }, srcType });
            m_instructions.push_back(Div{ src2, srcType });
        }
        m_instructions.push_back(Mov{
            (b.op == BinaryOperator::Divide
                ? Reg{ AX, bytes } : Reg{ DX, bytes }),
            dst,
            srcType
        });
        Comment(m_instructions, "---");
        return std::monostate();
    }

    // Equal, NotEqual, LessThan, LessOrEqual, GreaterThan, GreaterOrEqual
    if (isRelationOperator(b.op)) {
        Comment(m_instructions, std::format("Relational operator {}", toString(b.op)));
        if (srcType == Doubleword) {
            // Labels for handling a potentional unordered (NaN) case
            std::string unordered_label = MakeNameUnique("unordered_comparison");
            std::string end_label = MakeNameUnique("end_comparison");
            // Comparing arguments; result stored in RFLAGS
            m_instructions.push_back(Cmp{ src2, src1, srcType });
            // The jp instruction jumps only in case of NaN comparison
            m_instructions.push_back(JmpCC{ "p", unordered_label });
            // Null out the destination before placing the condition result in
            m_instructions.push_back(Mov{ Imm{ 0 }, dst, dstType });
            // Conditionally set the lowest byte of the destination according to RFLAGS
            m_instructions.push_back(SetCC{ toConditionCode(b.op, true), dst });
            m_instructions.push_back(Jmp{ end_label });
            m_instructions.push_back(Label{ unordered_label });
            // NaN != x is always true, other comparisons are always false
            m_instructions.push_back(Mov{
                Imm{ static_cast<uint64_t>(b.op == BinaryOperator::NotEqual ? 1 : 0) },
                dst,
                dstType
            });
            m_instructions.push_back(Label{ end_label });
        } else {
            // Comparing arguments; result stored in RFLAGS
            m_instructions.push_back(Cmp{ src2, src1, srcType });
            // Null out the destination before placing the condition result in
            m_instructions.push_back(Mov{ Imm{ 0 }, dst, dstType });
            // Conditionally set the lowest byte of the destination according to RFLAGS
            m_instructions.push_back(SetCC{ toConditionCode(b.op, !isSigned), dst });
        }
        Comment(m_instructions, "---");
        return std::monostate();
    }

    assert(false);
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Copy &c)
{
    if (auto s = GetStructEntry(c.src)) {
        auto &[tag, entry] = *s;
        std::vector<WordType> fragments = getMemoryFragments(entry->size);
        size_t offset = 0;
        for (auto &word_type : fragments) {
            m_instructions.push_back(Mov{
                PseudoAggregate{ tag, offset },
                PseudoAggregate{ tag, offset },
                word_type
            });
            offset += GetBytesOfWordType(word_type);
        }
        return std::monostate();
    }

    m_instructions.push_back(Mov{
        std::visit(*this, c.src),
        std::visit(*this, c.dst),
        GetWordType(c.dst)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::GetAddress &g)
{
    // Load Effective Address;
    // moves the address of source to the destination.
    m_instructions.push_back(Lea{
        std::visit(*this, g.src),
        std::visit(*this, g.dst)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Load &l)
{
    // Store the pointer in RAX
    m_instructions.push_back(Mov{
        std::visit(*this, l.src_ptr),
        Reg{ AX, 8 },
        Quadword
    });

    // Copy chunks of data stored at offsets from the address in RAX
    if (auto s = GetStructEntry(l.dst)) {
        auto &[tag, entry] = *s;
        std::vector<WordType> fragments = getMemoryFragments(entry->size);
        size_t offset = 0;
        for (auto &word_type : fragments) {
            m_instructions.push_back(Mov{
                Memory{ AX, static_cast<int>(offset) },
                PseudoAggregate{ tag, offset },
                word_type
            });
            offset += GetBytesOfWordType(word_type);
        }
        return std::monostate();
    }

    m_instructions.push_back(Mov{
        Memory{ AX, 0 },
        std::visit(*this, l.dst),
        GetWordType(l.dst)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Store &s)
{
    m_instructions.push_back(Mov{
        std::visit(*this, s.dst_ptr),
        Reg{ AX, 8 },
        Quadword
    });
    m_instructions.push_back(Mov{
        std::visit(*this, s.src),
        Memory{ AX, 0 },
        GetWordType(s.src)
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
    Comment(m_instructions, "Jump if zero");
    WordType wordType = GetWordType(j.condition);
    if (wordType == Doubleword) {
        // Zero out the XMMO register
        m_instructions.push_back(Binary{
             BWXor_AB,
             Reg{ XMM0 },
             Reg{ XMM0 },
             Doubleword
        });
        // Compare with zero
        m_instructions.push_back(Cmp{
            std::visit(*this, j.condition),
            Reg{ XMM0 },
            Doubleword
        });
    } else {
        // Compare with zero
        m_instructions.push_back(Cmp{
            Imm{ 0 },
            std::visit(*this, j.condition),
            wordType
        });
    }
    m_instructions.push_back(JmpCC{ "e", j.target });
    Comment(m_instructions, "---");
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::JumpIfNotZero &j)
{
    Comment(m_instructions, "Jump if not zero");
    WordType wordType = GetWordType(j.condition);
    if (wordType == Doubleword) {
        // Zero out the XMMO register
        m_instructions.push_back(Binary{
             BWXor_AB,
             Reg{ XMM0 },
             Reg{ XMM0 },
             Doubleword
        });
        // Compare with zero
        m_instructions.push_back(Cmp{
            std::visit(*this, j.condition),
            Reg{ XMM0 },
            Doubleword
        });
    } else {
        // Compare with zero
        m_instructions.push_back(Cmp{
            Imm{ 0 },
            std::visit(*this, j.condition),
            wordType
        });
    }
    m_instructions.push_back(JmpCC{ "ne", j.target });
    Comment(m_instructions, "---");
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Label &l)
{
    m_instructions.push_back(Label{ l.identifier });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::FunctionCall &f)
{
    auto args = classifyParameters(f.args, [this](const tac::Value &v) {
        return GetWordType(v);
    });

    // Adjust stack alignment to 16 bytes and allocate stack
    size_t stack_padding = (args.stack.size() % 2 == 0) ? 0 : 8;
    if (stack_padding != 0) {
        Comment(m_instructions, "Allocating stack");
        m_instructions.push_back(Binary{
            ASMBinaryOperator::Sub_AB,
            Imm{ stack_padding },
            Reg{ SP, 8 },
            WordType::Quadword
        });
    }

    // Move arguments into registers
    size_t reg_index = 0;
    if (args.int_regs.size() > 0)
        Comment(m_instructions, "Moving the first six int arguments into registers");
    for (auto &arg : args.int_regs) {
        WordType type = GetWordType(arg);
        m_instructions.push_back(Mov{
            std::visit(*this, arg),
            Reg{ s_intArgRegisters[reg_index++], GetBytesOfWordType(type) },
            type
        });
    }
    reg_index = 0;
    if (args.double_regs.size() > 0)
        Comment(m_instructions, "Moving the first eight double arguments into registers");
    for (auto &arg : args.double_regs) {
        m_instructions.push_back(Mov{
            std::visit(*this, arg),
            Reg{ s_doubleArgRegisters[reg_index++], 8 },
            Doubleword
        });
    }

    // Push remaining arguments to the stack (in reverse order)
    if (args.stack.size() > 0)
        Comment(m_instructions, "Pushing the rest of the arguments onto the stack");
    for (auto &arg : std::views::reverse(args.stack)) {
        auto asm_arg = std::visit(*this, arg);
        WordType arg_type = GetWordType(arg);
        if (std::holds_alternative<Reg>(asm_arg)
            || std::holds_alternative<Imm>(asm_arg)
            || arg_type == WordType::Quadword
            || arg_type == WordType::Doubleword)
            m_instructions.push_back(Push{ asm_arg });
        else {
            m_instructions.push_back(Mov{
                asm_arg,
                Reg{ AX, GetBytesOfWordType(arg_type) },
                arg_type
            });
            m_instructions.push_back(Push{ Reg{ AX, 8 } });
        }
    }

    // Call the function
    m_instructions.push_back(Call{ f.identifier });

    // The callee clears the stack
    size_t bytes_to_remove = 8 * args.stack.size() + stack_padding;
    if (bytes_to_remove != 0) {
        Comment(m_instructions, "Clearing the stack");
        m_instructions.push_back(Binary{
            Add_AB,
            Imm{ bytes_to_remove },
            Reg{ SP, 8 },
            WordType::Quadword
        });
    }

    // Retrieve the return value if needed
    if (!f.dst)
        return std::monostate();
    WordType return_type = GetWordType(*f.dst);
    if (return_type == Doubleword) {
        Comment(m_instructions, "The return value is in XMM0");
        m_instructions.push_back(Mov{
            Reg{ XMM0, GetBytesOfWordType(return_type) },
            std::visit(*this, *f.dst),
            Doubleword
        });
    } else {
        Comment(m_instructions, "The return value is in AX");
        m_instructions.push_back(Mov{
            Reg{ AX, GetBytesOfWordType(return_type) },
            std::visit(*this, *f.dst),
            return_type
        });
    }

    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::SignExtend &s)
{
    m_instructions.push_back(Movsx{
        std::visit(*this, s.src),
        std::visit(*this, s.dst),
        GetWordType(s.src),
        GetWordType(s.dst)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Truncate &t)
{
    m_instructions.push_back(Mov{
        std::visit(*this, t.src),
        std::visit(*this, t.dst),
        GetWordType(t.dst)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::ZeroExtend &z)
{
    m_instructions.push_back(MovZeroExtend{
        std::visit(*this, z.src),
        std::visit(*this, z.dst),
        GetWordType(z.src),
        GetWordType(z.dst)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::DoubleToInt &d)
{
    Operand src = std::visit(*this, d.src);
    Operand dst = std::visit(*this, d.dst);
    WordType dst_type = GetWordType(d.dst);

    if (dst_type == Byte) {
        Comment(m_instructions, "Double to signed char");
        m_instructions.push_back(Cvttsd2si{ src, Reg{ AX, 4 }, Longword });
        m_instructions.push_back(Mov{ Reg{ AX, 1 }, dst, Byte });
        return std::monostate();
    }

    Comment(m_instructions, "Double to signed integer");
    m_instructions.push_back(Cvttsd2si{ src, dst, dst_type });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::DoubleToUInt &d)
{
    Operand src = std::visit(*this, d.src);
    Operand dst = std::visit(*this, d.dst);
    BasicType basicType = GetBasicType(d.dst);
    if (basicType == UInt) {
        Comment(m_instructions, "Double to UInt");
        m_instructions.push_back(Cvttsd2si{ src, Reg{ AX, 8 }, Quadword });
        m_instructions.push_back(Mov{ Reg{ AX, 4 }, dst, Longword });
    } else if (basicType == UChar) {
        Comment(m_instructions, "Double to UChar");
        m_instructions.push_back(Cvttsd2si{ src, Reg{ AX, 4 }, Longword });
        m_instructions.push_back(Mov{ Reg{ AX, 1 }, dst, Byte });
    } else if (basicType == ULong) {
        Comment(m_instructions, "Double to ULong");
        std::string upper_bound = AddConstant(
            ConstantValue{ 9223372036854775808.0 },
            MakeNameUnique("double_upper_bound")
        );
        std::string oor_label = MakeNameUnique("out_of_range");
        std::string end_label = MakeNameUnique("end");
        m_instructions.push_back(Cmp{ Data{ upper_bound }, src, Doubleword });
        m_instructions.push_back(JmpCC{ "ae", oor_label });
        m_instructions.push_back(Cvttsd2si{ src, dst, Quadword });
        m_instructions.push_back(Jmp{ end_label });
        m_instructions.push_back(Label{ oor_label });
        m_instructions.push_back(Mov{ src, Reg{ XMM0, 8 }, Doubleword });
        m_instructions.push_back(Binary{ Sub_AB, Data{ upper_bound }, Reg{ XMM0, 8 }, Doubleword });
        m_instructions.push_back(Cvttsd2si{ Reg{ XMM0, 8 }, dst, Quadword });
        m_instructions.push_back(Mov{ Imm{ 9223372036854775808ULL }, Reg{ AX, 8 }, Quadword });
        m_instructions.push_back(Binary{ Add_AB, Reg{ AX, 8 }, dst, Quadword });
        m_instructions.push_back(Label{ end_label });
    } else
        assert(false);
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::IntToDouble &i)
{
    Operand src = std::visit(*this, i.src);
    Operand dst = std::visit(*this, i.dst);
    WordType src_type = GetWordType(i.src);

    if (src_type == Byte) {
        Comment(m_instructions, "Char to double");
        m_instructions.push_back(Movsx{ src, Reg{ AX, 4 }, Byte, Longword });
        m_instructions.push_back(Cvtsi2sd{ Reg{ AX, 4 }, dst, Longword });
        return std::monostate();
    }

    Comment(m_instructions, "Signed integer to double");
    m_instructions.push_back(Cvtsi2sd{ src, dst, src_type });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::UIntToDouble &u)
{
    Operand src = std::visit(*this, u.src);
    Operand dst = std::visit(*this, u.dst);
    BasicType basicType = GetBasicType(u.src);
    if (basicType == UInt) {
        Comment(m_instructions, "UInt to Double");
        m_instructions.push_back(MovZeroExtend{ src, Reg{ AX, 8 }, Longword, Quadword });
        m_instructions.push_back(Cvtsi2sd{ Reg{ AX, 8 }, dst, Quadword });
    } else if (basicType == UChar) {
        Comment(m_instructions, "UChar to Double");
        m_instructions.push_back(MovZeroExtend{ src, Reg{ AX, 4 }, Byte, Longword });
        m_instructions.push_back(Cvtsi2sd{ Reg{ AX, 4 }, dst, Longword });
    } else if (basicType == ULong) {
        Comment(m_instructions, "ULong to Double");
        std::string oor_label = MakeNameUnique("out_of_range");
        std::string end_label = MakeNameUnique("end");
        m_instructions.push_back(Cmp{ Imm{ 0 }, src, Quadword });
        m_instructions.push_back(JmpCC{ "l", oor_label });
        m_instructions.push_back(Cvtsi2sd{ src, dst, Quadword });
        m_instructions.push_back(Jmp{ end_label });
        m_instructions.push_back(Label{ oor_label });
        m_instructions.push_back(Mov{ src, Reg{ AX, 8 }, Quadword });
        m_instructions.push_back(Mov{ Reg{ AX, 8 }, Reg{ DX, 8 }, Quadword });
        m_instructions.push_back(Binary{ ShiftRU_AB, Imm{ 1 }, Reg{ DX, 8 }, Quadword });
        m_instructions.push_back(Binary{ BWAnd_AB, Imm{ 1 }, Reg{ AX, 8 }, Quadword });
        m_instructions.push_back(Binary{ BWOr_AB, Reg{ AX, 8 }, Reg{ DX, 8 }, Quadword });
        m_instructions.push_back(Cvtsi2sd{ Reg{ DX, 8 }, dst, Quadword });
        m_instructions.push_back(Binary{ Add_AB, dst, dst, Doubleword });
        m_instructions.push_back(Label{ end_label });
    } else
        assert(false);
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::AddPtr &a)
{
    Operand ptr = std::visit(*this, a.ptr);
    Operand index = std::visit(*this, a.index);
    Operand dst = std::visit(*this, a.dst);

    // If the index operand is a constant, we can save an instruction
    // by computing index * scale at compile time.
    if (auto const_index = std::get_if<tac::Constant>(&a.index)) {
        int offset = castTo<int>(const_index->value) * static_cast<int>(a.scale);
        m_instructions.push_back(Mov{ ptr, Reg{ AX, 8 }, Quadword });
        m_instructions.push_back(Lea{ Memory{ AX, offset }, dst });
        return std::monostate();
    }

    // Load ptr and index into registers
    m_instructions.push_back(Mov{ ptr, Reg{ AX, 8 }, Quadword });
    m_instructions.push_back(Mov{ index, Reg{ DX, 8 }, Quadword });

    // Check if the scale is supported by Indexed operands
    if (a.scale == 1 || a.scale == 2 || a.scale == 4 || a.scale == 8) {
        m_instructions.push_back(Lea{
            Indexed{ AX, DX, static_cast<uint8_t>(a.scale) },
            dst
        });
        return std::monostate();
    }

    // We have to multiply the scale by the index using an ASM instruction
    m_instructions.push_back(Binary{
        Mult_AB,
        Imm{ static_cast<uint64_t>(a.scale) },
        Reg{ DX, 8 },
        Quadword
    });
    m_instructions.push_back(Lea{
        Indexed{ AX, DX, 1 },
        dst
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::CopyToOffset &c)
{
    if (auto s = GetStructEntry(c.src)) {
        auto &[tag, entry] = *s;
        std::vector<WordType> fragments = getMemoryFragments(entry->size);
        size_t offset = 0;
        for (auto &word_type : fragments) {
            m_instructions.push_back(Mov{
                PseudoAggregate{ tag, offset },
                PseudoAggregate{ c.dst_identifier, offset + c.offset },
                word_type
            });
            offset += GetBytesOfWordType(word_type);
        }
        return std::monostate();
    }

    m_instructions.push_back(Mov{
        std::visit(*this, c.src),
        PseudoAggregate{ c.dst_identifier, c.offset },
        GetWordType(c.src)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::CopyFromOffset &c)
{
    if (auto s = GetStructEntry(c.dst)) {
        auto &[tag, entry] = *s;
        std::vector<WordType> fragments = getMemoryFragments(entry->size);
        size_t offset = 0;
        for (auto &word_type : fragments) {
            m_instructions.push_back(Mov{
                PseudoAggregate{ c.src_identifier, offset + c.offset },
                PseudoAggregate{ tag, offset },
                word_type
            });
            offset += GetBytesOfWordType(word_type);
        }
        return std::monostate();
    }

    m_instructions.push_back(Mov{
        PseudoAggregate{ c.src_identifier, c.offset },
        std::visit(*this, c.dst),
        GetWordType(c.dst)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Constant &c)
{
    if (getType(c.value).isBasic(Double)) {
        // Add the constant later to the data segment
        // and avoid duplications
        return Data{ AddConstant(c.value, GenerateTempVariableName()) };
    }
    return Imm{ castTo<uint64_t>(c.value) };
}

Operand ASMBuilder::operator()(const tac::Variant &v)
{
    auto entry = m_context->symbolTable->get(v.name);
    if (entry->type.isArray() || entry->type.isStruct())
        return PseudoAggregate{ v.name, 0 };
    else
        return Pseudo{ v.name }; // TODO: Rename to PseudoScalar?
}

Operand ASMBuilder::operator()(const tac::FunctionDefinition &f)
{
    auto func = Function{};
    func.name = f.name;
    func.global = f.global;

    auto args = classifyParameters(f.params, [this](const std::string &name) {
        return m_symbolTable->getWordType(name);
    });

    // We copy each of the parameters into the current stack frame
    // to make life easier. This can be optimised out later.
    if (args.int_regs.size() > 0)
        Comment(func.instructions, "Getting integer parameters from registers");
    for (size_t i = 0; i < args.int_regs.size(); i++) {
        const std::string &p = args.int_regs[i];
        WordType type = m_symbolTable->getWordType(p);
        func.instructions.push_back(Mov{
            Reg{ s_intArgRegisters[i], GetBytesOfWordType(type) },
            Pseudo{ p },
            type
        });
    }
    if (args.double_regs.size() > 0)
        Comment(func.instructions, "Getting double parameters from registers");
    for (size_t i = 0; i < args.double_regs.size(); i++) {
        func.instructions.push_back(Mov{
            Reg{ s_doubleArgRegisters[i], 8 },
            Pseudo{ args.double_regs[i] },
            Doubleword
        });
    }
    if (args.stack.size() > 0)
        Comment(func.instructions, "Getting remaining parameters from the stack");
    size_t stack_offset = 16;
    for (auto &param : args.stack) {
        func.instructions.push_back(Mov{
            Memory{ BP, static_cast<int>(stack_offset) },
            Pseudo{ param },
            m_symbolTable->getWordType(param)
        });
        stack_offset += 8;
    }
    if (!args.int_regs.empty() || !args.double_regs.empty() || !args.stack.empty())
        Comment(func.instructions, "---");

    // Body
    ASMBuilder builder(m_context, m_constants);
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
        .list = s.list,
        .alignment = s.type.alignment(m_context->typeTable.get())
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::StaticConstant &s)
{
    m_topLevel.push_back(StaticConstant{
        .name = s.name,
        .init = s.static_init,
        .alignment = s.type.alignment(m_context->typeTable.get())
    });
    return std::monostate();
}

std::list<TopLevel> ASMBuilder::ConvertTopLevel(const std::vector<tac::TopLevel> instructions)
{
    m_topLevel.clear();
    m_constants->clear();

    for (auto &inst : instructions)
        std::visit(*this, inst);

    for (auto const &[value, label] : *m_constants) {
        m_topLevel.push_back(StaticConstant{
            .name = label,
            .init = value,
            .alignment = 8
        });
    }

    return std::move(m_topLevel);
}

std::list<Instruction> ASMBuilder::ConvertInstructions(const std::vector<tac::Instruction> instructions)
{
    m_instructions.clear();
    for (auto &inst : instructions)
        std::visit(*this, inst);
    return std::move(m_instructions);
}

std::string ASMBuilder::AddConstant(const ConstantValue &c, const std::string &name)
{
    auto it = m_constants->find(c);
    if (it != m_constants->end())
        return it->second;
    else {
        m_constants->insert({ c, name });
        return name;
    }
}

}; // assembly
