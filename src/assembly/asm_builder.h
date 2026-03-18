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

    std::list<TopLevel> ConvertTopLevel(const std::list<tac::TopLevel> &);
    std::list<Instruction> ConvertFunctionBody(const std::list<tac::CFGBlock> &);

private:
    Type GetType(const tac::Value &);
    BasicType GetBasicType(const tac::Value &);
    WordType GetWordType(const tac::Value &);
    const TypeTable::AggregateEntry *GetAggregateEntry(const std::string *name);
    void Comment(std::list<Instruction> &i, const std::string &text);
    std::string AddConstant(const ConstantValue &c, const std::string &name);

    // Copy between Memory and PseudoAggregate operands in a specified size
    void CopyBytes(std::list<Instruction> &i, Operand src, Operand dst, size_t size);
    // Only for copying irregular size structs between register and memory
    void CopyBytesToReg(std::list<Instruction> &i, Operand src, Register dst, size_t size);
    void CopyBytesFromReg(std::list<Instruction> &i, Register src, Operand dst, size_t size);

    bool m_commentsEnabled = true;
    std::list<TopLevel> m_topLevel;
    std::list<Instruction> m_instructions;

    Context *m_context;
    TypeTable *m_typeTable;
    SymbolTable *m_symbolTable;
    // Keep track of static constants and their IDs to avoid duplications
    std::shared_ptr<ConstantMap> m_constants;
};

}; // assembly
