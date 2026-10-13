#include "interpreter.hpp"

#include <cmath>
#include <cstdio>
#include <functional>

namespace calc {
namespace {

const double kPi = std::acos(-1.0);

struct Builtin { std::size_t minArgs, maxArgs; std::function<double(const std::vector<double>&)> fn; };

// Fonctions intégrées. maxArgs = 0 signifie « illimité ».
const std::map<std::string, Builtin>& builtins() {
    static const std::map<std::string, Builtin> table = {
        {"sin",  {1, 1, [](auto& a) { return std::sin(a[0]); }}},
        {"cos",  {1, 1, [](auto& a) { return std::cos(a[0]); }}},
        {"tan",  {1, 1, [](auto& a) { return std::tan(a[0]); }}},
        {"asin", {1, 1, [](auto& a) { return std::asin(a[0]); }}},
        {"acos", {1, 1, [](auto& a) { return std::acos(a[0]); }}},
        {"atan", {1, 1, [](auto& a) { return std::atan(a[0]); }}},
        {"sqrt", {1, 1, [](auto& a) { return std::sqrt(a[0]); }}},
        {"exp",  {1, 1, [](auto& a) { return std::exp(a[0]); }}},
        {"ln",   {1, 1, [](auto& a) { return std::log(a[0]); }}},
        {"log",  {1, 1, [](auto& a) { return std::log10(a[0]); }}},
        {"log2", {1, 1, [](auto& a) { return std::log2(a[0]); }}},
        {"abs",  {1, 1, [](auto& a) { return std::fabs(a[0]); }}},
        {"floor", {1, 1, [](auto& a) { return std::floor(a[0]); }}},
        {"ceil", {1, 1, [](auto& a) { return std::ceil(a[0]); }}},
        {"round", {1, 1, [](auto& a) { return std::round(a[0]); }}},
        {"pow",  {2, 2, [](auto& a) { return std::pow(a[0], a[1]); }}},
        {"hypot", {2, 2, [](auto& a) { return std::hypot(a[0], a[1]); }}},
        {"min",  {1, 0, [](auto& a) { double m = a[0]; for (double v : a) m = std::fmin(m, v); return m; }}},
        {"max",  {1, 0, [](auto& a) { double m = a[0]; for (double v : a) m = std::fmax(m, v); return m; }}},
    };
    return table;
}

bool isConstant(const std::string& n) { return n == "pi" || n == "e"; }

}  // namespace

std::string formatNumber(double v) {
    if (v == 0.0) v = 0.0;  // supprime le « -0 »
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.12g", v);
    return buf;
}

Interpreter::Interpreter() { reset(); }

void Interpreter::reset() {
    vars_.clear();
    funcs_.clear();
    vars_["pi"] = kPi;
    vars_["e"] = std::exp(1.0);
    vars_["ans"] = 0.0;
}

std::vector<std::string> Interpreter::variables() const {
    std::vector<std::string> out;
    for (const auto& [k, v] : vars_) out.push_back(k + " = " + formatNumber(v));
    return out;
}

std::vector<std::string> Interpreter::functions() const {
    std::vector<std::string> out;
    for (const auto& [k, f] : funcs_) {
        std::string s = k + "(";
        for (std::size_t i = 0; i < f.params.size(); ++i) s += (i ? ", " : "") + f.params[i];
        out.push_back(s + ")");
    }
    return out;
}

double Interpreter::eval(const std::string& expression) { return execute(expression).value; }

Result Interpreter::execute(const std::string& line) {
    Statement st = parse(line);
    switch (st.kind) {
        case Statement::Kind::Assignment: {
            if (isConstant(st.name) || st.name == "ans" || builtins().count(st.name) || funcs_.count(st.name))
                throw CalcError("« " + st.name + " » est réservé et ne peut pas être modifié", 0);
            const double v = evalNode(st.body, nullptr, 0);
            vars_[st.name] = v;
            return {Result::Kind::Assigned, v, st.name};
        }
        case Statement::Kind::FunctionDef: {
            if (builtins().count(st.name) || isConstant(st.name) || st.name == "ans")
                throw CalcError("« " + st.name + " » est une fonction intégrée", 0);
            if (vars_.count(st.name))
                throw CalcError("« " + st.name + " » est déjà une variable", 0);
            funcs_[st.name] = {st.params, st.body};
            return {Result::Kind::Defined, 0.0, st.name};
        }
        case Statement::Kind::Expression:
        default: {
            const double v = evalNode(st.body, nullptr, 0);
            vars_["ans"] = v;
            return {Result::Kind::Value, v, ""};
        }
    }
}

double Interpreter::callBuiltin(const Node& call, const std::vector<double>& args, bool& found) {
    const auto it = builtins().find(call.name);
    found = it != builtins().end();
    if (!found) return 0.0;
    const Builtin& b = it->second;
    if (args.size() < b.minArgs || (b.maxArgs && args.size() > b.maxArgs)) {
        const std::string expected = b.maxArgs == 0 ? "au moins " + std::to_string(b.minArgs)
                                   : b.minArgs == b.maxArgs ? std::to_string(b.minArgs)
                                   : std::to_string(b.minArgs) + " à " + std::to_string(b.maxArgs);
        throw CalcError(call.name + "() attend " + expected + " argument(s), " + std::to_string(args.size()) + " reçu(s)", call.pos);
    }
    // Domaines de définition : message clair plutôt qu'un « nan » silencieux
    if (call.name == "sqrt" && args[0] < 0) throw CalcError("Racine carrée d'un nombre négatif", call.pos);
    if ((call.name == "ln" || call.name == "log" || call.name == "log2") && args[0] <= 0)
        throw CalcError("Logarithme d'un nombre négatif ou nul", call.pos);
    if ((call.name == "asin" || call.name == "acos") && std::fabs(args[0]) > 1)
        throw CalcError(call.name + "() n'est définie qu'entre -1 et 1", call.pos);
    return b.fn(args);
}

double Interpreter::evalNode(const NodePtr& n, const std::map<std::string, double>* locals, int depth) {
    if (depth > kMaxDepth) throw CalcError("Récursion trop profonde (une fonction s'appelle-t-elle elle-même ?)", n->pos);
    double result = 0.0;
    switch (n->kind) {
        case Node::Kind::Number:
            return n->value;

        case Node::Kind::Variable: {
            if (locals) {
                const auto it = locals->find(n->name);
                if (it != locals->end()) return it->second;
            }
            const auto it = vars_.find(n->name);
            if (it == vars_.end()) {
                if (funcs_.count(n->name) || builtins().count(n->name))
                    throw CalcError("« " + n->name + " » est une fonction : utilisez " + n->name + "(...)", n->pos);
                throw CalcError("Variable inconnue « " + n->name + " »", n->pos);
            }
            return it->second;
        }

        case Node::Kind::Unary: {
            const double v = evalNode(n->children[0], locals, depth + 1);
            return n->op == '-' ? -v : v;
        }

        case Node::Kind::Binary: {
            const double a = evalNode(n->children[0], locals, depth + 1);
            const double b = evalNode(n->children[1], locals, depth + 1);
            switch (n->op) {
                case '+': result = a + b; break;
                case '-': result = a - b; break;
                case '*': result = a * b; break;
                case '/':
                    if (b == 0.0) throw CalcError("Division par zéro", n->pos);
                    result = a / b;
                    break;
                case '%':
                    if (b == 0.0) throw CalcError("Modulo par zéro", n->pos);
                    result = std::fmod(a, b);
                    break;
                case '^':
                    if (a < 0 && std::floor(b) != b) throw CalcError("Puissance non entière d'un nombre négatif", n->pos);
                    result = std::pow(a, b);
                    break;
            }
            break;
        }

        case Node::Kind::Call: {
            std::vector<double> args;
            args.reserve(n->children.size());
            for (const auto& c : n->children) args.push_back(evalNode(c, locals, depth + 1));

            const auto uf = funcs_.find(n->name);
            if (uf != funcs_.end()) {
                if (args.size() != uf->second.params.size())
                    throw CalcError(n->name + "() attend " + std::to_string(uf->second.params.size()) + " argument(s), " +
                                    std::to_string(args.size()) + " reçu(s)", n->pos);
                std::map<std::string, double> scope;
                for (std::size_t i = 0; i < args.size(); ++i) scope[uf->second.params[i]] = args[i];
                result = evalNode(uf->second.body, &scope, depth + 1);
                break;
            }
            bool found = false;
            result = callBuiltin(*n, args, found);
            if (!found) throw CalcError("Fonction inconnue « " + n->name + " »", n->pos);
            break;
        }
    }
    if (!std::isfinite(result)) throw CalcError("Résultat hors limites (dépassement de capacité)", n->pos);
    return result;
}

}  // namespace calc
