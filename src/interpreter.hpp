// interpreter.hpp — évalue les instructions, conserve variables et fonctions.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "parser.hpp"

namespace calc {

struct Result {
    enum class Kind { Value, Assigned, Defined } kind;
    double value = 0.0;
    std::string name;
};

class Interpreter {
public:
    Interpreter();
    Result execute(const std::string& line);
    double eval(const std::string& expression);   // raccourci pour les tests

    std::vector<std::string> variables() const;
    std::vector<std::string> functions() const;
    void reset();

    static constexpr int kMaxDepth = 200;          // garde-fou contre la récursion infinie

private:
    struct UserFunction { std::vector<std::string> params; NodePtr body; };
    std::map<std::string, double> vars_;
    std::map<std::string, UserFunction> funcs_;

    double evalNode(const NodePtr& n, const std::map<std::string, double>* locals, int depth);
    double callBuiltin(const Node& call, const std::vector<double>& args, bool& found);
};

std::string formatNumber(double v);

}  // namespace calc
