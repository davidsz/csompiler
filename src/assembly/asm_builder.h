#pragma once

#include "asm_nodes.h"
#include "asm_symbol_table.h"
#include "constant_map.h"
#include "common/type_table.h"
#include "tac/tac_visitor.h"

class Context;

namespace assembly {

class ASMBuilder : public tac::ITACVisitor<Operand> {
public:
    ASMBuilder(
        Context *context,
        std::shared_ptr<ConstantMap> constants);

    Operand operator()(const tac::Return &) override;
    Operand operator()(const tac::Unary &) override;
    Operand operator()(const tac::Binary &) override;
    Operand operator()(const tac::Copy &) override;
    Operand operator()(const tac::GetAddress &) override;
    Operand operator()(const tac::Load &) override;
    Operand operator()(const tac::Store &) override;
    Operand operator()(const tac::Jump &) override;
    Operand operator()(const tac::JumpIfZero &) override;
    Operand operator()(const tac::JumpIfNotZero &) override;
    Operand operator()(const tac::Label &) override;
    Operand operator()(const tac::Constant &) override;
    Operand operator()(const tac::Variant &) override;
    Operand operator()(const tac::FunctionCall &) override;
    Operand operator()(const tac::SignExtend &) override;
    Operand operator()(const tac::Truncate &) override;
    Operand operator()(const tac::ZeroExtend &) override;
    Operand operator()(const tac::DoubleToInt &) override;
    Operand operator()(const tac::DoubleToUInt &) override;
    Operand operator()(const tac::IntToDouble &) override;
    Operand operator()(const tac::UIntToDouble &) override;
    Operand operator()(const tac::AddPtr &) override;
    Operand operator()(const tac::CopyToOffset &) override;
    Operand operator()(const tac::CopyFromOffset &) override;
    Operand operator()(const tac::FunctionDefinition &) override;
    Operand operator()(const tac::StaticVariable &) override;
    Operand operator()(const tac::StaticConstant &) override;
    Operand operator()(std::monostate) override {
        assert(false);
        return std::monostate();
    }

    void ConvertTopLevel(const std::list<tac::TopLevel> &, std::list<TopLevel> &);
    void ConvertFunctionBody(const std::list<tac::CFGBlock> &, std::list<CFGBlock> &);

private:
    Type GetType(const tac::Value &);
    BasicType GetBasicType(const tac::Value &);
    WordType GetWordType(const tac::Value &);
    TypeTable::AggregateEntry *GetAggregateEntry(const std::string *name);
    void Comment(std::list<Instruction> &i, const std::string &text);
    std::string AddConstant(const ConstantValue &c, const std::string &name);

    // Copy between Memory and PseudoAggregate operands in a specified size
    void CopyBytes(std::list<Instruction> &i, Operand src, Operand dst, size_t size);
    // Only for copying irregular size structs between register and memory
    void CopyBytesToReg(std::list<Instruction> &i, Operand src, Register dst, size_t size);
    void CopyBytesFromReg(std::list<Instruction> &i, Register src, Operand dst, size_t size);

    bool m_commentsEnabled = true;
    // Functions, static variables and constants of the translation unit
    std::list<TopLevel> *m_topLevel;
    // Building blocks for a Control Flow Graph
    // The first block is built immediately at Function creation
    size_t m_nextBlockId = 2;
    std::list<CFGBlock> *m_blocks;
    // Storing the instructions of the currently built CFGBlock
    std::list<Instruction> m_instructions;

    // Add instruction to the currently built CFGBlock
    template <typename T>
    void AddInstruction(T &&instruction) {
        using U = std::decay_t<T>;
        if constexpr (std::is_same_v<U, Label>) {
            if (!m_instructions.empty())
                CommitBlock();
            m_instructions.clear();
            m_instructions.emplace_back(std::forward<T>(instruction));
        } else if constexpr (std::same_as<U, Jmp>
            || std::same_as<U, JmpCC>
            || std::same_as<U, Ret>) {
            m_instructions.emplace_back(std::forward<T>(instruction));
            CommitBlock();
            m_instructions.clear();
        } else
            m_instructions.emplace_back(std::forward<T>(instruction));
    }

    // Commit the current CFGBlock to start a new one
    void CommitBlock();

    Context *m_context;
    TypeTable *m_typeTable;
    SymbolTable *m_symbolTable;
    // Keep track of static constants and their IDs to avoid duplications
    std::shared_ptr<ConstantMap> m_constants;
};

}; // assembly
