#include "asm_builder.h"
#include "asm_helper.h"
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

static const Register s_intReturnRegisters[2] = { AX, DX };

static const Register s_doubleReturnRegisters[2] = { XMM0, XMM1 };

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

static Operand addOffset(Operand op, size_t offset)
{
    if (Memory *m = std::get_if<Memory>(&op))
        return Memory{ m->reg, m->offset + static_cast<int>(offset) };
    if (PseudoAggregate *p = std::get_if<PseudoAggregate>(&op))
        return PseudoAggregate{ p->name, p->offset + offset };
    assert(false);
    return Operand{ };
}

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

ASMBuilder::ASMBuilder(Context *context, std::shared_ptr<ConstantMap> constants)
    : m_context(context)
    , m_typeTable(context->typeTable.get())
    , m_symbolTable(context->symbolTable.get())
    , m_asmSymbolTable(context->asmSymbolTable.get())
    , m_constants(constants)
{
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

TypeTable::AggregateEntry *ASMBuilder::GetAggregateEntry(const std::string *name)
{
    if (!name)
        return nullptr;
    Type type = m_symbolTable->getType(*name);
    const AggregateType *aggr_type = type.getAs<AggregateType>();
    if (!aggr_type)
        return nullptr;
    return m_typeTable->get(aggr_type->tag);
}

void ASMBuilder::Comment(std::list<Instruction> &i, const std::string &text)
{
    if (m_commentsEnabled)
        i.push_back(assembly::Comment{ text });
}

Operand ASMBuilder::operator()(const tac::Return &r)
{
    if (!r.val) {
        AddInstruction(Ret{ });
        return std::monostate();
    }

    FunEntry *fun_entry = m_asmSymbolTable->getAs<FunEntry>(m_currentFunctionName);
    assert(fun_entry);

    Operand ret_value = std::visit(*this, *r.val);
    Type return_type = GetType(*r.val);
    ClassifiedReturn ret = classifyReturnValue(ret_value, return_type, m_typeTable);

    std::vector<Register> ret_registers;
    ret_registers.reserve(ret.int_values.size());

    if (ret.in_memory) {
        // Retrieve the address of the return value into RAX
        AddInstruction(Mov{ Memory{ BP, -8 }, Reg{ AX, 8 }, Quadword });
        CopyBytes(
            m_instructions,
            ret_value,
            Memory { AX, 0 },
            return_type.size(m_typeTable)
        );
    } else {
        size_t reg_index = 0;
        for (auto &int_val : ret.int_values) {
            auto &[operand, type] = int_val;
            Register reg = s_intReturnRegisters[reg_index++];
            ret_registers.push_back(reg);
            if (const ByteArray *byte_array = type.getAs<ByteArray>())
                CopyBytesToReg(m_instructions, operand, reg, byte_array->size);
            else {
                AddInstruction(Mov{
                    operand,
                    Reg{ reg, static_cast<uint8_t>(type.size()) },
                    *type.getAs<WordType>()
                });
            }
        }
        reg_index = 0;
        for (auto &operand : ret.double_values) {
            Register reg = s_doubleReturnRegisters[reg_index++];
            ret_registers.push_back(reg);
            AddInstruction(Mov{ operand, Reg{ reg, 8 }, Doubleword });
        }
    }

    fun_entry->ret_registers = std::move(ret_registers);

    AddInstruction(Ret{ });
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
            AddInstruction(Binary{ BWXor_AB, Reg{ XMM0, 8 }, Reg{ XMM0, 8 }, srcType });
            AddInstruction(Cmp{ Reg{ XMM0, 8 }, src, srcType });
            // NaN evaluates to non-zero, !NaN is zero,
            // so we always zero the destination out
            AddInstruction(Mov{ Imm{ 0 }, dst, dstType });
            // Perform the SetCC instruction only for ordered operations
            AddInstruction(JmpCC{ "p", end_label });
            AddInstruction(SetCC{ "e", dst });
            AddInstruction(Label{ end_label });
        } else {
            AddInstruction(Cmp{ Imm{ 0 }, src, srcType });
            AddInstruction(Mov{ Imm{ 0 }, dst, dstType });
            AddInstruction(SetCC{ "e", dst });
        }
        return std::monostate();
    }

    if (u.op == Negate && srcType == Doubleword) {
        // Negating in floating point: XOR with -0.0
        std::string minus_zero = AddConstant(
            ConstantValue{ static_cast<double>(-0.0) },
            GenerateTempVariableName()
        );
        AddInstruction(Mov{ src, dst, Doubleword });
        AddInstruction(Binary{
            BWXor_AB,
            Data{ minus_zero },
            dst,
            Doubleword
        });
        return std::monostate();
    }

    ASMUnaryOperator op = toASMUnaryOperator(u.op);
    assert(op != Unknown_AU);
    AddInstruction(Mov{ src, dst, srcType });
    AddInstruction(Unary{ op, dst, dstType });
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
        AddInstruction(Mov{ src1, dst, srcType });
        AddInstruction(Binary{ op, src2, dst, srcType });
        Comment(m_instructions, "---");
        return std::monostate();
    }

    // Division, Remainder
    if (b.op == BinaryOperator::Divide || b.op == BinaryOperator::Remainder) {
        Comment(m_instructions, std::format("Binary operator {} ({})",
            toString(b.op), isSigned ? "signed" : "unsigned"));
        uint8_t bytes = GetBytesOfWordType(srcType);
        AddInstruction(Mov{ src1, Reg{ AX, bytes }, srcType });
        if (isSigned) {
            AddInstruction(Cdq{ srcType });
            AddInstruction(Idiv{ src2, srcType });
        } else {
            AddInstruction(Mov{ Imm{ 0 }, Reg{ DX, bytes }, srcType });
            AddInstruction(Div{ src2, srcType });
        }
        AddInstruction(Mov{
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
            AddInstruction(Cmp{ src2, src1, srcType });
            // The jp instruction jumps only in case of NaN comparison
            AddInstruction(JmpCC{ "p", unordered_label });
            // Null out the destination before placing the condition result in
            AddInstruction(Mov{ Imm{ 0 }, dst, dstType });
            // Conditionally set the lowest byte of the destination according to RFLAGS
            AddInstruction(SetCC{ toConditionCode(b.op, true), dst });
            AddInstruction(Jmp{ end_label });
            AddInstruction(Label{ unordered_label });
            // NaN != x is always true, other comparisons are always false
            AddInstruction(Mov{
                Imm{ static_cast<int64_t>(b.op == BinaryOperator::NotEqual ? 1 : 0) },
                dst,
                dstType
            });
            AddInstruction(Label{ end_label });
        } else {
            // Comparing arguments; result stored in RFLAGS
            AddInstruction(Cmp{ src2, src1, srcType });
            // Null out the destination before placing the condition result in
            AddInstruction(Mov{ Imm{ 0 }, dst, dstType });
            // Conditionally set the lowest byte of the destination according to RFLAGS
            AddInstruction(SetCC{ toConditionCode(b.op, !isSigned), dst });
        }
        Comment(m_instructions, "---");
        return std::monostate();
    }

    assert(false);
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Copy &c)
{
    const std::string *src_name = getString(c.src);
    if (auto entry = GetAggregateEntry(src_name)) {
        CopyBytes(
            m_instructions,
            PseudoAggregate{ *src_name, 0 },
            PseudoAggregate{ *getString(c.dst), 0 },
            entry->size);
        return std::monostate();
    }

    AddInstruction(Mov{
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
    AddInstruction(Lea{
        std::visit(*this, g.src),
        std::visit(*this, g.dst)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Load &l)
{
    // Store the pointer in RAX
    AddInstruction(Mov{
        std::visit(*this, l.src_ptr),
        Reg{ AX, 8 },
        Quadword
    });

    // Struct/union: copy chunks of data stored at offsets from the address in RAX
    const std::string *dst_name = getString(l.dst);
    if (auto entry = GetAggregateEntry(dst_name)) {
        CopyBytes(
            m_instructions,
            Memory{ AX, 0 },
            PseudoAggregate{ *dst_name, 0 },
            entry->size);
        return std::monostate();
    }

    // Scalar
    AddInstruction(Mov{
        Memory{ AX, 0 },
        std::visit(*this, l.dst),
        GetWordType(l.dst)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Store &s)
{
    // Store the pointer in RAX
    AddInstruction(Mov{
        std::visit(*this, s.dst_ptr),
        Reg{ AX, 8 },
        Quadword
    });

    // Struct/union: copy chunks of data stored to the address in RAX
    const std::string *src_name = getString(s.src);
    if (auto entry = GetAggregateEntry(src_name)) {
        CopyBytes(
            m_instructions,
            PseudoAggregate{ *src_name, 0 },
            Memory{ AX, 0 },
            entry->size
        );
        return std::monostate();
    }

    // Scalar
    AddInstruction(Mov{
        std::visit(*this, s.src),
        Memory{ AX, 0 },
        GetWordType(s.src)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Jump &j)
{
    AddInstruction(Jmp{ j.target });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::JumpIfZero &j)
{
    Comment(m_instructions, "Jump if zero");
    WordType wordType = GetWordType(j.condition);
    if (wordType == Doubleword) {
        // Zero out the XMMO register
        AddInstruction(Binary{
             BWXor_AB,
             Reg{ XMM0 },
             Reg{ XMM0 },
             Doubleword
        });
        // Compare with zero
        AddInstruction(Cmp{
            std::visit(*this, j.condition),
            Reg{ XMM0 },
            Doubleword
        });
    } else {
        // Compare with zero
        AddInstruction(Cmp{
            Imm{ 0 },
            std::visit(*this, j.condition),
            wordType
        });
    }
    AddInstruction(JmpCC{ "e", j.target });
    Comment(m_instructions, "---");
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::JumpIfNotZero &j)
{
    Comment(m_instructions, "Jump if not zero");
    WordType wordType = GetWordType(j.condition);
    if (wordType == Doubleword) {
        // Zero out the XMMO register
        AddInstruction(Binary{
             BWXor_AB,
             Reg{ XMM0 },
             Reg{ XMM0 },
             Doubleword
        });
        // Compare with zero
        AddInstruction(Cmp{
            std::visit(*this, j.condition),
            Reg{ XMM0 },
            Doubleword
        });
    } else {
        // Compare with zero
        AddInstruction(Cmp{
            Imm{ 0 },
            std::visit(*this, j.condition),
            wordType
        });
    }
    AddInstruction(JmpCC{ "ne", j.target });
    Comment(m_instructions, "---");
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Label &l)
{
    AddInstruction(Label{ l.identifier });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::FunctionCall &f)
{
    Operand dst_operand;
    ClassifiedReturn ret;
    if (f.dst) {
        dst_operand = std::visit(*this, *f.dst);
        ret = classifyReturnValue(dst_operand, GetType(*f.dst), m_typeTable);
    }

    size_t reg_index = 0;
    if (ret.in_memory) {
        Comment(m_instructions, "Return value is in memory, it's address is RDI");
        AddInstruction(Lea{ dst_operand, Reg{ DI, 8 } });
        // The first argument register (RDI) is occupied
        reg_index = 1;
    }

    auto args = classifyParameters(
        f.args,
        ret.in_memory,
        m_typeTable,
        [this](const tac::Value &v) {
            return GetType(v);
        },
        [this](const tac::Value &v) {
            return std::visit(*this, v);
        }
    );

    // Adjust stack alignment to 16 bytes and allocate stack
    size_t stack_padding = (args.stack.size() % 2 == 0) ? 0 : 8;
    if (stack_padding != 0) {
        Comment(m_instructions, "Allocating stack");
        AddInstruction(Binary{
            ASMBinaryOperator::Sub_AB,
            Imm{ static_cast<int64_t>(stack_padding) },
            Reg{ SP, 8 },
            WordType::Quadword
        });
    }

    // Move arguments into registers
    if (args.int_regs.size() > 0)
        Comment(m_instructions, "Moving the first six int arguments into registers");
    for (auto &arg : args.int_regs) {
        auto &[operand, type] = arg;
        Register reg = s_intArgRegisters[reg_index++];
        if (const ByteArray *byte_array = type.getAs<ByteArray>()) {
            Comment(m_instructions, "Irregular size struct will be passed in registers");
            CopyBytesToReg(m_instructions, operand, reg, byte_array->size);
        } else {
            AddInstruction(Mov{
                operand,
                Reg{ reg, static_cast<uint8_t>(type.size()) },
                *type.getAs<WordType>()
            });
        }
    }
    reg_index = 0;
    if (args.double_regs.size() > 0)
        Comment(m_instructions, "Moving the first eight double arguments into registers");
    for (auto &operand : args.double_regs) {
        AddInstruction(Mov{
            operand,
            Reg{ s_doubleArgRegisters[reg_index++], 8 },
            Doubleword
        });
    }

    // Push remaining arguments to the stack (in reverse order)
    if (args.stack.size() > 0)
        Comment(m_instructions, "Pushing the rest of the arguments onto the stack");
    for (auto &arg : std::views::reverse(args.stack)) {
        auto &[operand, type] = arg;
        if (const ByteArray *byte_array = type.getAs<ByteArray>()) {
            Comment(m_instructions, "Copying irregular size argument to the stack");
            AddInstruction(Binary{ Sub_AB, Imm{ 8 }, Reg{ SP, 8 }, Quadword });
            CopyBytes(m_instructions, operand, Memory{ SP, 0 }, byte_array->size);
        } else if (std::holds_alternative<Reg>(operand)
            || std::holds_alternative<Imm>(operand)
            || type.isWord(Quadword)
            || type.isWord(Doubleword))
            AddInstruction(Push{ operand });
        else {
            AddInstruction(Mov{
                operand,
                Reg{ AX, static_cast<uint8_t>(type.size()) },
                *type.getAs<WordType>()
            });
            AddInstruction(Push{ Reg{ AX, 8 } });
        }
    }

    // Call the function
    AddInstruction(Call{ f.identifier });

    // The callee clears the stack
    size_t bytes_to_remove = 8 * args.stack.size() + stack_padding;
    if (bytes_to_remove != 0) {
        Comment(m_instructions, "Clearing the stack");
        AddInstruction(Binary{
            Add_AB,
            Imm{ static_cast<int64_t>(bytes_to_remove) },
            Reg{ SP, 8 },
            WordType::Quadword
        });
    }

    // Retrieve the return value if needed
    if (!f.dst || ret.in_memory)
        return std::monostate();
    reg_index = 0;
    for (auto &int_val : ret.int_values) {
        auto &[operand, type] = int_val;
        Register reg = s_intReturnRegisters[reg_index++];
        if (const ByteArray *byte_array = type.getAs<ByteArray>()) {
            Comment(m_instructions, "Irregular size struct is coming from registers");
            CopyBytesFromReg(m_instructions, reg, operand, byte_array->size);
        } else
            AddInstruction(Mov{
                Reg{ reg, static_cast<uint8_t>(type.size()) },
                operand,
                *type.getAs<WordType>()
            });
    }
    reg_index = 0;
    for (auto &operand : ret.double_values) {
        Register reg = s_doubleReturnRegisters[reg_index++];
        AddInstruction(Mov{ Reg{ reg, 8 }, operand, Doubleword });
    }
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::SignExtend &s)
{
    AddInstruction(Movsx{
        std::visit(*this, s.src),
        std::visit(*this, s.dst),
        GetWordType(s.src),
        GetWordType(s.dst)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::Truncate &t)
{
    Operand src = std::visit(*this, t.src);
    WordType dst_type = GetWordType(t.dst);

    // GCC throws a warning when it has to automatically truncate an immediate value
    if (Imm *imm = std::get_if<Imm>(&src)) {
        switch (dst_type) {
        case Byte:
            imm->value = static_cast<int8_t>(imm->value);
            break;
        case Longword:
            imm->value = static_cast<int32_t>(imm->value);
            break;
        case Quadword:
        case Doubleword:
        default:
            break;
        }
    }

    AddInstruction(Mov{ src, std::visit(*this, t.dst), dst_type });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::ZeroExtend &z)
{
    AddInstruction(MovZeroExtend{
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
        AddInstruction(Cvttsd2si{ src, Reg{ AX, 4 }, Longword });
        AddInstruction(Mov{ Reg{ AX, 1 }, dst, Byte });
        return std::monostate();
    }

    Comment(m_instructions, "Double to signed integer");
    AddInstruction(Cvttsd2si{ src, dst, dst_type });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::DoubleToUInt &d)
{
    Operand src = std::visit(*this, d.src);
    Operand dst = std::visit(*this, d.dst);
    BasicType basicType = GetBasicType(d.dst);
    if (basicType == UInt) {
        Comment(m_instructions, "Double to UInt");
        AddInstruction(Cvttsd2si{ src, Reg{ AX, 8 }, Quadword });
        AddInstruction(Mov{ Reg{ AX, 4 }, dst, Longword });
    } else if (basicType == UChar) {
        Comment(m_instructions, "Double to UChar");
        AddInstruction(Cvttsd2si{ src, Reg{ AX, 4 }, Longword });
        AddInstruction(Mov{ Reg{ AX, 1 }, dst, Byte });
    } else if (basicType == ULong) {
        Comment(m_instructions, "Double to ULong");
        std::string upper_bound = AddConstant(
            ConstantValue{ 9223372036854775808.0 },
            MakeNameUnique("double_upper_bound")
        );
        std::string oor_label = MakeNameUnique("out_of_range");
        std::string end_label = MakeNameUnique("end");
        AddInstruction(Cmp{ Data{ upper_bound }, src, Doubleword });
        AddInstruction(JmpCC{ "ae", oor_label });
        AddInstruction(Cvttsd2si{ src, dst, Quadword });
        AddInstruction(Jmp{ end_label });
        AddInstruction(Label{ oor_label });
        AddInstruction(Mov{ src, Reg{ XMM0, 8 }, Doubleword });
        AddInstruction(Binary{ Sub_AB, Data{ upper_bound }, Reg{ XMM0, 8 }, Doubleword });
        AddInstruction(Cvttsd2si{ Reg{ XMM0, 8 }, dst, Quadword });
        AddInstruction(Mov{ Imm{ static_cast<int64_t>(0x8000000000000000ULL) }, Reg{ AX, 8 }, Quadword });
        AddInstruction(Binary{ Add_AB, Reg{ AX, 8 }, dst, Quadword });
        AddInstruction(Label{ end_label });
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
        AddInstruction(Movsx{ src, Reg{ AX, 4 }, Byte, Longword });
        AddInstruction(Cvtsi2sd{ Reg{ AX, 4 }, dst, Longword });
        return std::monostate();
    }

    Comment(m_instructions, "Signed integer to double");
    AddInstruction(Cvtsi2sd{ src, dst, src_type });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::UIntToDouble &u)
{
    Operand src = std::visit(*this, u.src);
    Operand dst = std::visit(*this, u.dst);
    BasicType basicType = GetBasicType(u.src);
    if (basicType == UInt) {
        Comment(m_instructions, "UInt to Double");
        AddInstruction(MovZeroExtend{ src, Reg{ AX, 8 }, Longword, Quadword });
        AddInstruction(Cvtsi2sd{ Reg{ AX, 8 }, dst, Quadword });
    } else if (basicType == UChar) {
        Comment(m_instructions, "UChar to Double");
        AddInstruction(MovZeroExtend{ src, Reg{ AX, 4 }, Byte, Longword });
        AddInstruction(Cvtsi2sd{ Reg{ AX, 4 }, dst, Longword });
    } else if (basicType == ULong) {
        Comment(m_instructions, "ULong to Double");
        std::string oor_label = MakeNameUnique("out_of_range");
        std::string end_label = MakeNameUnique("end");
        AddInstruction(Cmp{ Imm{ 0 }, src, Quadword });
        AddInstruction(JmpCC{ "l", oor_label });
        AddInstruction(Cvtsi2sd{ src, dst, Quadword });
        AddInstruction(Jmp{ end_label });
        AddInstruction(Label{ oor_label });
        AddInstruction(Mov{ src, Reg{ AX, 8 }, Quadword });
        AddInstruction(Mov{ Reg{ AX, 8 }, Reg{ DX, 8 }, Quadword });
        AddInstruction(Binary{ ShiftRU_AB, Imm{ 1 }, Reg{ DX, 8 }, Quadword });
        AddInstruction(Binary{ BWAnd_AB, Imm{ 1 }, Reg{ AX, 8 }, Quadword });
        AddInstruction(Binary{ BWOr_AB, Reg{ AX, 8 }, Reg{ DX, 8 }, Quadword });
        AddInstruction(Cvtsi2sd{ Reg{ DX, 8 }, dst, Quadword });
        AddInstruction(Binary{ Add_AB, dst, dst, Doubleword });
        AddInstruction(Label{ end_label });
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
        AddInstruction(Mov{ ptr, Reg{ AX, 8 }, Quadword });
        AddInstruction(Lea{ Memory{ AX, offset }, dst });
        return std::monostate();
    }

    // Load ptr and index into registers
    AddInstruction(Mov{ ptr, Reg{ AX, 8 }, Quadword });
    AddInstruction(Mov{ index, Reg{ DX, 8 }, Quadword });

    // Check if the scale is supported by Indexed operands
    if (a.scale == 1 || a.scale == 2 || a.scale == 4 || a.scale == 8) {
        AddInstruction(Lea{
            Indexed{ AX, DX, static_cast<uint8_t>(a.scale) },
            dst
        });
        return std::monostate();
    }

    // We have to multiply the scale by the index using an ASM instruction
    AddInstruction(Binary{
        Mult_AB,
        Imm{ static_cast<int64_t>(a.scale) },
        Reg{ DX, 8 },
        Quadword
    });
    AddInstruction(Lea{
        Indexed{ AX, DX, 1 },
        dst
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::CopyToOffset &c)
{
    const std::string *src_name = getString(c.src);
    if (auto entry = GetAggregateEntry(src_name)) {
        CopyBytes(
            m_instructions,
            PseudoAggregate{ *src_name, 0 },
            PseudoAggregate{ c.dst_identifier, c.offset },
            entry->size);
        return std::monostate();
    }

    AddInstruction(Mov{
        std::visit(*this, c.src),
        PseudoAggregate{ c.dst_identifier, c.offset },
        GetWordType(c.src)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::CopyFromOffset &c)
{
    const std::string *dst_name = getString(c.dst);
    if (auto entry = GetAggregateEntry(dst_name)) {
        CopyBytes(
            m_instructions,
            PseudoAggregate{ c.src_identifier, c.offset },
            PseudoAggregate{ *dst_name, 0 },
            entry->size);
        return std::monostate();
    }

    AddInstruction(Mov{
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
    return Imm{ castTo<int64_t>(c.value) };
}

Operand ASMBuilder::operator()(const tac::Variant &v)
{
    auto entry = m_symbolTable->get(v.name);
    if (entry->type.isArray() || entry->type.isAggregate())
        return PseudoAggregate{ v.name, 0 };
    else
        return Pseudo{ v.name };
}

Operand ASMBuilder::operator()(const tac::FunctionDefinition &f)
{
    auto func = Function{};
    func.name = f.name;
    func.global = f.global;
    CFGBlock &first_block = func.blocks.emplace_back(CFGBlock{
        .instructions = {},
        .id = 1
    });

    bool return_in_memory = false;
    auto symbol_entry = m_symbolTable->get(f.name);
    assert(symbol_entry);
    const FunctionType *func_type = symbol_entry->type.getAs<FunctionType>();
    if (const AggregateType *aggr_type = func_type->ret->getAs<AggregateType>()) {
        auto aggr_entry = m_typeTable->get(aggr_type->tag);
        auto &classes = aggr_entry->classes(m_typeTable);
        return_in_memory = (classes.front() == MEMORY);
    }

    auto args = classifyParameters(
        f.params,
        return_in_memory,
        m_typeTable,
        [this](const std::string &name) {
            return m_symbolTable->getType(name);
        },
        [](const std::string &name) {
            return Pseudo{ name };
        }
    );

    std::vector<Register> arg_registers;
    arg_registers.reserve(args.int_regs.size() + args.double_regs.size());

    size_t reg_index = 0;
    if (return_in_memory) {
        first_block.instructions.push_back(Mov{ Reg{ DI, 8 }, Memory{ BP, -8 }, Quadword });
        reg_index = 1;
    }

    // We copy each of the parameters into the current stack frame
    // to make life easier. This can be optimised out later.
    if (args.int_regs.size() > 0)
        Comment(first_block.instructions, "Getting integer parameters from registers");
    for (auto &int_reg : args.int_regs) {
        auto &[operand, type] = int_reg;
        Register reg = s_intArgRegisters[reg_index++];
        arg_registers.push_back(reg);
        if (type.isByteArray())
            CopyBytesFromReg(first_block.instructions, reg, operand, type.size());
        else {
            first_block.instructions.push_back(Mov{
                Reg{ reg, static_cast<uint8_t>(type.size()) },
                operand,
                *type.getAs<WordType>()
            });
        }
    }
    if (args.double_regs.size() > 0)
        Comment(first_block.instructions, "Getting double parameters from registers");
    for (size_t i = 0; i < args.double_regs.size(); i++) {
        Register reg = s_doubleArgRegisters[i];
        arg_registers.push_back(reg);
        first_block.instructions.push_back(Mov{
            Reg{ reg, 8 },
            args.double_regs[i],
            Doubleword
        });
    }
    if (args.stack.size() > 0)
        Comment(first_block.instructions, "Getting remaining parameters from the stack");
    size_t stack_offset = 16;
    for (auto &param : args.stack) {
        auto &[operand, type] = param;
        if (type.isByteArray()) {
            CopyBytes(
                first_block.instructions,
                Memory{ BP, static_cast<int>(stack_offset) },
                operand, type.size()
            );
        } else {
            first_block.instructions.push_back(Mov{
                Memory{ BP, static_cast<int>(stack_offset) },
                operand,
                *type.getAs<WordType>()
            });
        }
        stack_offset += 8;
    }
    if (!args.int_regs.empty() || !args.double_regs.empty() || !args.stack.empty())
        Comment(first_block.instructions, "---");

    // Save the symbol before processing the body; Return instructions need it
    m_asmSymbolTable->Insert(f.name, FunEntry{
        .defined = symbol_entry->attrs.defined,
        .return_on_stack = return_in_memory,
        .arg_registers = std::move(arg_registers)
    });

    // Body
    ASMBuilder builder(m_context, m_constants);
    builder.ConvertFunctionBody(f.name, f.blocks, func.blocks);

    m_topLevel->push_back(std::move(func));
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::StaticVariable &s)
{
    m_topLevel->push_back(StaticVariable{
        .name = s.name,
        .global = s.global,
        .list = s.list,
        .alignment = s.type.alignment(m_typeTable)
    });
    return std::monostate();
}

Operand ASMBuilder::operator()(const tac::StaticConstant &s)
{
    m_topLevel->push_back(StaticConstant{
        .name = s.name,
        .init = s.static_init,
        .alignment = s.type.alignment(m_typeTable)
    });
    return std::monostate();
}

void ASMBuilder::ConvertTopLevel(
    const std::list<tac::TopLevel> &top_level,
    std::list<TopLevel> &top_level_out)
{
    m_topLevel = &top_level_out;
    m_constants->clear();

    for (auto &inst : top_level)
        std::visit(*this, inst);

    for (auto const &[value, label] : *m_constants) {
        m_topLevel->push_back(StaticConstant{
            .name = label,
            .init = value,
            .alignment = 8
        });
    }
}

void ASMBuilder::ConvertFunctionBody(
    const std::string &name,
    const std::list<tac::CFGBlock> &tac_blocks,
    std::list<CFGBlock> &block_list_out)
{
    m_currentFunctionName = name;
    m_blocks = &block_list_out;
    for (auto &block : tac_blocks) {
        for (auto &i : block.instructions)
            std::visit(*this, i);
    }
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

void ASMBuilder::CopyBytes(std::list<Instruction> &i, Operand src, Operand dst, size_t size)
{
    std::vector<WordType> fragments = getMemoryFragments(size);
    size_t offset = 0;
    for (auto &word_type : fragments) {
        i.push_back(Mov{
            addOffset(src, offset),
            addOffset(dst, offset),
            word_type
        });
        offset += GetBytesOfWordType(word_type);
    }
}

void ASMBuilder::CopyBytesToReg(std::list<Instruction> &i, Operand src, Register dst, size_t size)
{
    assert(std::holds_alternative<Memory>(src) || std::holds_alternative<PseudoAggregate>(src));
    int offset = static_cast<int>(size) - 1;
    while (offset >= 0) {
        Operand src_byte = addOffset(src, static_cast<size_t>(offset));
        i.push_back(Mov{ src_byte, Reg{ dst, 1 }, Byte });
        if (offset > 0)
            i.push_back(Binary{ ShiftL_AB, Imm{ 8 }, Reg{ dst, 8 }, Quadword });
        offset -= 1;
    }
}

void ASMBuilder::CopyBytesFromReg(std::list<Instruction> &i, Register src, Operand dst, size_t size)
{
    assert(std::holds_alternative<Memory>(dst) || std::holds_alternative<PseudoAggregate>(dst));
    size_t offset = 0;
    while (offset < size) {
        Operand dst_byte = addOffset(dst, offset);
        i.push_back(Mov{ Reg{ src, 1 }, dst_byte, Byte });
        if (offset < size - 1)
            i.push_back(Binary{ ShiftRU_AB, Imm{ 8 }, Reg{ src, 8 }, Quadword });
        offset += 1;
    }
}

void ASMBuilder::CommitBlock()
{
    m_blocks->emplace_back(CFGBlock{
        .instructions = std::move(m_instructions),
        .id = m_nextBlockId++
    });
}

}; // assembly
