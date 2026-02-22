#pragma once

#include "parser/ast_visitor.h"
#include "tac_nodes.h"
#include <vector>

class Context;

namespace tac {

struct PlainOperand { Value val; };
struct DereferencedPointer { Value ptr; };
using ExpResult = std::variant<PlainOperand, DereferencedPointer, std::monostate>;

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

    std::vector<TopLevel> ConvertTopLevel(const std::vector<parser::Declaration> &list);
    std::vector<Instruction> ConvertBlock(const std::vector<parser::BlockItem> &list);

private:
    Variant CreateTemporaryVariable(const Type &type);
    Variant CastValue(
        std::vector<Instruction> &i,
        const Value &result,
        const Type &from_type,
        const Type &to_type);
    Type GetType(const Value &value);
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
            m_instructions.push_back(Load{ deref->ptr, dst });
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

    std::vector<TopLevel> m_topLevel;
    std::vector<Instruction> m_instructions;

    Context *m_context;
    TypeTable *m_typeTable;
};

}; // tac
