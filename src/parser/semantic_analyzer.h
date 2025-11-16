#pragma once

#include "ast_mutating_visitor.h"
#include "common/error.h"
#include <unordered_map>

namespace parser {

struct SemanticAnalyzer : public IASTMutatingVisitor<void> {
    void operator()(NumberExpression &n) override;
    void operator()(VariableExpression &v) override;
    void operator()(UnaryExpression &u) override;
    void operator()(BinaryExpression &b) override;
    void operator()(AssignmentExpression &a) override;
    void operator()(ConditionalExpression &c) override;
    void operator()(FuncDeclStatement &f) override;
    void operator()(ReturnStatement &r) override;
    void operator()(IfStatement &i) override;
    void operator()(BlockStatement &b) override;
    void operator()(ExpressionStatement &e) override;
    void operator()(NullStatement &e) override;
    void operator()(Declaration &d) override;
    void operator()(std::monostate) override;

    Error CheckAndMutate(std::vector<parser::BlockItem> &);
    void Abort(std::string_view);

private:
    std::unordered_map<std::string, std::string> m_variables;
};

} // namespace parser
