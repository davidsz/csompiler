#pragma once

#include "parser/ast_visitor.h"
#include "tac_nodes.h"
#include <list>

class Context;

namespace tac {

struct PlainOperand {
    Value val;
};
struct DereferencedPointer {
    Value ptr;
};
struct SubObject {
    std::string base_identifier;
    size_t offset;
};
using ExpResult = std::variant<PlainOperand, DereferencedPointer, SubObject, std::monostate>;

class TACBuilder : public parser::IASTVisitor<tac::ExpResult> {
public:
    TACBuilder(Context *context);

    ExpResult operator()(const parser::ConstantExpression &c) override;
    ExpResult operator()(const parser::StringExpression &s) override;
    ExpResult operator()(const parser::VariableExpression &v) override;
    ExpResult operator()(const parser::CastExpression &c) override;
    ExpResult operator()(const parser::UnaryExpression &u) override;
    ExpResult operator()(const parser::BinaryExpression &b) override;
    ExpResult operator()(const parser::AssignmentExpression &a) override;
    ExpResult operator()(const parser::CompoundAssignmentExpression &c) override;
    ExpResult operator()(const parser::ConditionalExpression &c) override;
    ExpResult operator()(const parser::FunctionCallExpression &f) override;
    ExpResult operator()(const parser::DereferenceExpression &d) override;
    ExpResult operator()(const parser::AddressOfExpression &a) override;
    ExpResult operator()(const parser::SubscriptExpression &s) override;
    ExpResult operator()(const parser::SizeOfExpression &s) override;
    ExpResult operator()(const parser::SizeOfTypeExpression &s) override;
    ExpResult operator()(const parser::DotExpression &d) override;
    ExpResult operator()(const parser::ArrowExpression &a) override;
    ExpResult operator()(const parser::ReturnStatement &r) override;
    ExpResult operator()(const parser::IfStatement &i) override;
    ExpResult operator()(const parser::GotoStatement &g) override;
    ExpResult operator()(const parser::LabeledStatement &l) override;
    ExpResult operator()(const parser::BlockStatement &b) override;
    ExpResult operator()(const parser::ExpressionStatement &e) override;
    ExpResult operator()(const parser::NullStatement &e) override;
    ExpResult operator()(const parser::BreakStatement &b) override;
    ExpResult operator()(const parser::ContinueStatement &c) override;
    ExpResult operator()(const parser::WhileStatement &w) override;
    ExpResult operator()(const parser::DoWhileStatement &d) override;
    ExpResult operator()(const parser::ForStatement &f) override;
    ExpResult operator()(const parser::SwitchStatement &s) override;
    ExpResult operator()(const parser::CaseStatement &c) override;
    ExpResult operator()(const parser::DefaultStatement &d) override;
    ExpResult operator()(const parser::FunctionDeclaration &f) override;
    ExpResult operator()(const parser::VariableDeclaration &v) override;
    ExpResult operator()(const parser::StructDeclaration &s) override;
    ExpResult operator()(const parser::SingleInit &s) override;
    ExpResult operator()(const parser::CompoundInit &c) override;
    ExpResult operator()(std::monostate) override;

    std::list<TopLevel> ConvertTopLevel(
        const std::vector<parser::Declaration> &list);
    std::list<CFGBlock> ConvertFunctionBlock(
        const std::vector<parser::BlockItem> &list);

private:
    Variant CreateTemporaryVariable(const Type &type);
    Variant CastValue(
        std::list<Instruction> &i,
        const Value &result,
        const Type &from_type,
        const Type &to_type);
    Type GetType(const Value &value);
    Type GetExpressionType(const parser::Expression &expr);
    void ProcessStaticSymbols();

    template <typename... Variants>
    Value VisitAndConvert(Variants&&... variants) {
        ExpResult result = std::visit(*this, std::forward<Variants>(variants)...);
        if (PlainOperand *plain = std::get_if<PlainOperand>(&result))
            return plain->val;
        else if (DereferencedPointer *deref = std::get_if<DereferencedPointer>(&result)) {
            PointerType *ptr_type = GetType(deref->ptr).getAs<PointerType>();
            assert(ptr_type);
            Variant dst = CreateTemporaryVariable(ptr_type->referenced->storedType());
            AddInstruction(Load{ deref->ptr, dst });
            return dst;
        } else if (SubObject *sub = std::get_if<SubObject>(&result)) {
            Variant dst = CreateTemporaryVariable(GetExpressionType(variants ...));
            AddInstruction(CopyFromOffset{
                .src_identifier = sub->base_identifier,
                .offset = sub->offset,
                .dst = dst
            });
            return dst;
        }
        assert(false);
        return std::monostate();
    }

    struct LHSInfo {
        enum class Kind { Plain, Deref } kind;
        Value address;          // Where to write the result
        Type original_type;     // Before a potential cast
    };
    LHSInfo AnalyzeLHS(const parser::Expression &expr);

    void EmitZeroInit(const Type &type, const std::string &base, size_t &offset);
    void EmitRuntimeInit(
        const parser::Initializer *init,
        const std::string &base,
        const Type &type,
        size_t &offset
    );

    // Functions, static variables and constants of the translation unit
    std::list<TopLevel> m_topLevel;
    // Building blocks for a Control Flow Graph
    size_t m_nextBlockId = 0;
    std::list<CFGBlock> m_blocks;
    // Storing the instructions of the currently built CFGBlock
    std::list<Instruction> m_instructions;

    template <typename T>
    void AddInstruction(T &&instruction)
    {
        using U = std::decay_t<T>;
        if constexpr (std::is_same_v<U, Label>) {
            if (!m_instructions.empty())
                m_blocks.emplace_back(CFGBlock{
                    .id = m_nextBlockId++,
                    .instructions = std::move(m_instructions)
                });
            m_instructions.clear();
            m_instructions.emplace_back(std::forward<T>(instruction));
        } else if constexpr (std::same_as<T, Jump>
            || std::same_as<T, JumpIfZero>
            || std::same_as<T, JumpIfNotZero>
            || std::same_as<T, Return>) {
                m_instructions.emplace_back(std::forward<T>(instruction));
            m_blocks.emplace_back(CFGBlock{
                .id = m_nextBlockId++,
                .instructions = std::move(m_instructions)
            });
            m_instructions.clear();
        } else
            m_instructions.emplace_back(std::forward<T>(instruction));
    }

    Context *m_context;
    TypeTable *m_typeTable;
};

}; // tac
