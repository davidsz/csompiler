#pragma once

#include "ast_mutating_visitor.h"
#include "common/error.h"
#include "common/symbol_table.h"

class Context;

namespace parser {

class TypeChecker : public IASTMutatingVisitor<Type> {
public:
    TypeChecker(Context *context);

    Type operator()(ConstantExpression &c) override;
    Type operator()(StringExpression &s) override;
    Type operator()(VariableExpression &v) override;
    Type operator()(CastExpression &c) override;
    Type operator()(UnaryExpression &u) override;
    Type operator()(BinaryExpression &b) override;
    Type operator()(AssignmentExpression &a) override;
    Type operator()(CompoundAssignmentExpression &c) override;
    Type operator()(ConditionalExpression &c) override;
    Type operator()(FunctionCallExpression &f) override;
    Type operator()(DereferenceExpression &d) override;
    Type operator()(AddressOfExpression &a) override;
    Type operator()(SubscriptExpression &s) override;
    Type operator()(SizeOfExpression &s) override;
    Type operator()(SizeOfTypeExpression &s) override;
    Type operator()(DotExpression &d) override;
    Type operator()(ArrowExpression &a) override;
    Type operator()(ReturnStatement &r) override;
    Type operator()(IfStatement &i) override;
    Type operator()(GotoStatement &g) override;
    Type operator()(LabeledStatement &l) override;
    Type operator()(BlockStatement &b) override;
    Type operator()(ExpressionStatement &e) override;
    Type operator()(NullStatement &e) override;
    Type operator()(BreakStatement &b) override;
    Type operator()(ContinueStatement &c) override;
    Type operator()(WhileStatement &w) override;
    Type operator()(DoWhileStatement &d) override;
    Type operator()(ForStatement &f) override;
    Type operator()(SwitchStatement &s) override;
    Type operator()(CaseStatement &c) override;
    Type operator()(DefaultStatement &d) override;
    Type operator()(FunctionDeclaration &f) override;
    Type operator()(VariableDeclaration &v) override;
    Type operator()(StructDeclaration &s) override;
    Type operator()(SingleInit &s) override;
    Type operator()(CompoundInit &c) override;
    Type operator()(std::monostate) override;

    Error CheckAndMutate(std::vector<parser::Declaration> &);
    void Abort(std::string_view);

private:
    Type VisitAndConvert(std::unique_ptr<Expression> &expr);
    void ValidateTypeSpecifier(const Type &type);
    Type GetCommonType(const Type &first, const Type &second);
    std::vector<ConstantValue> ToConstantValueList(const Initializer *init, const Type &type);
    InitialValue InitializeStaticPointer(const Initializer *init, const Type &type);

    bool m_fileScope = false;
    bool m_forLoopInitializer = false;
    std::vector<Type> m_functionTypeStack;
    std::vector<SwitchStatement *> m_switches;
    Type m_targetTypeForInitializer;

    Context *m_context;
    TypeTable *m_typeTable;
};

}; // namespace parser
