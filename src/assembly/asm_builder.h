#pragma once

#include "asm_nodes.h"
#include "asm_symbol_table.h"
#include "tac/tac_visitor.h"

namespace assembly {

class ASMBuilder : public tac::ITACVisitor<Operand> {
public:
    ASMBuilder(std::shared_ptr<SymbolTable> symbolTable);

    Operand operator()(const tac::Return &) override;
    Operand operator()(const tac::Unary &) override;
    Operand operator()(const tac::Binary &) override;
    Operand operator()(const tac::Copy &) override;
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
    Operand operator()(const tac::FunctionDefinition &) override;
    Operand operator()(const tac::StaticVariable &) override;
    Operand operator()(std::monostate) override {
        assert(false);
        return std::monostate();
    }

    std::list<TopLevel> ConvertTopLevel(const std::vector<tac::TopLevel>);
    std::list<Instruction> ConvertInstructions(const std::vector<tac::Instruction>);

    std::string AddConstant(const ConstantValue &c, const std::string &name);
    const std::unordered_map<ConstantValue, std::string> &StaticConstants();

private:
    BasicType GetBasicType(const tac::Value &);
    WordType GetWordType(const tac::Value &);
    bool CheckSignedness(const tac::Value &);
    void Comment(std::list<Instruction> &i, const std::string &text);

    bool m_commentsEnabled = true;
    std::list<TopLevel> m_topLevel;
    std::list<Instruction> m_instructions;
    std::shared_ptr<SymbolTable> m_symbolTable;
    // Keep track of static constants and their IDs to avoid duplications
    std::unordered_map<ConstantValue, std::string> m_constants;
};

}; // assembly
