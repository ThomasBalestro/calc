#include "parser.hpp"

#include <algorithm>

namespace calc {
namespace {

class Parser {
public:
    explicit Parser(std::vector<Token> toks) : t_(std::move(toks)) {}

    Statement statement() {
        Statement st;
        if (isFunctionDef()) {
            st.kind = Statement::Kind::FunctionDef;
            st.name = next().text;
            expect(Tok::LParen);
            if (!check(Tok::RParen)) {
                do {
                    const Token& p = expect(Tok::Ident, "nom de paramètre");
                    if (std::find(st.params.begin(), st.params.end(), p.text) != st.params.end())
                        throw CalcError("Paramètre « " + p.text + " » en double", p.pos);
                    st.params.push_back(p.text);
                } while (accept(Tok::Comma));
            }
            expect(Tok::RParen);
            expect(Tok::Equal);
        } else if (check(Tok::Ident) && peek(1).type == Tok::Equal) {
            st.kind = Statement::Kind::Assignment;
            st.name = next().text;
            next();  // '='
        } else {
            st.kind = Statement::Kind::Expression;
        }
        st.body = expr();
        if (!check(Tok::End)) throw CalcError("Inattendu : " + describe(cur()), cur().pos);
        return st;
    }

private:
    std::vector<Token> t_;
    std::size_t i_ = 0;

    const Token& cur() const { return t_[i_]; }
    const Token& peek(std::size_t k) const { return t_[std::min(i_ + k, t_.size() - 1)]; }
    bool check(Tok k) const { return cur().type == k; }
    const Token& next() { const Token& t = t_[i_]; if (i_ + 1 < t_.size()) ++i_; return t; }
    bool accept(Tok k) { if (check(k)) { next(); return true; } return false; }

    static std::string describe(const Token& t) {
        return t.type == Tok::End ? tokName(t.type) : "« " + t.text + " »";
    }
    const Token& expect(Tok k, const std::string& what = "") {
        if (!check(k)) throw CalcError((what.empty() ? tokName(k) : what) + " attendu, trouvé " + describe(cur()), cur().pos);
        return next();
    }

    // f(a, b) = ...  : identifiant, '(', identifiants séparés par des virgules, ')', '='
    bool isFunctionDef() const {
        if (!check(Tok::Ident) || peek(1).type != Tok::LParen) return false;
        std::size_t k = 2;
        if (peek(k).type == Tok::RParen) return peek(k + 1).type == Tok::Equal;
        while (true) {
            if (peek(k).type != Tok::Ident) return false;
            ++k;
            if (peek(k).type == Tok::Comma) { ++k; continue; }
            return peek(k).type == Tok::RParen && peek(k + 1).type == Tok::Equal;
        }
    }

    static NodePtr make(Node n) { return std::make_shared<const Node>(std::move(n)); }

    NodePtr binary(char op, NodePtr l, NodePtr r, std::size_t pos) {
        Node n;
        n.kind = Node::Kind::Binary;
        n.op = op;
        n.pos = pos;
        n.children = {std::move(l), std::move(r)};
        return make(std::move(n));
    }

    NodePtr expr() {
        NodePtr left = term();
        while (check(Tok::Plus) || check(Tok::Minus)) {
            const Token& op = next();
            left = binary(op.text[0], left, term(), op.pos);
        }
        return left;
    }

    NodePtr term() {
        NodePtr left = unary();
        while (check(Tok::Star) || check(Tok::Slash) || check(Tok::Percent)) {
            const Token& op = next();
            left = binary(op.text[0], left, unary(), op.pos);
        }
        return left;
    }

    NodePtr unary() {
        if (check(Tok::Minus) || check(Tok::Plus)) {
            const Token& op = next();
            Node n;
        n.kind = Node::Kind::Unary;
            n.op = op.text[0];
            n.pos = op.pos;
            n.children = {unary()};
            return make(std::move(n));
        }
        return power();
    }

    NodePtr power() {
        NodePtr base = primary();
        if (check(Tok::Caret)) {
            const Token& op = next();
            return binary('^', base, unary(), op.pos);  // unaire -> 2^-1 autorisé, associatif à droite
        }
        return base;
    }

    NodePtr primary() {
        const Token& t = cur();
        if (accept(Tok::Number)) {
            Node n;
        n.kind = Node::Kind::Number;
            n.value = t.value;
            n.pos = t.pos;
            return make(std::move(n));
        }
        if (accept(Tok::Ident)) {
            if (accept(Tok::LParen)) {
                Node n;
        n.kind = Node::Kind::Call;
                n.name = t.text;
                n.pos = t.pos;
                if (!check(Tok::RParen)) {
                    do n.children.push_back(expr()); while (accept(Tok::Comma));
                }
                expect(Tok::RParen);
                return make(std::move(n));
            }
            Node n;
        n.kind = Node::Kind::Variable;
            n.name = t.text;
            n.pos = t.pos;
            return make(std::move(n));
        }
        if (accept(Tok::LParen)) {
            NodePtr inner = expr();
            expect(Tok::RParen);
            return inner;
        }
        throw CalcError("Valeur attendue, trouvé " + describe(t), t.pos);
    }
};

}  // namespace

Statement parse(const std::string& source) {
    return Parser(tokenize(source)).statement();
}

}  // namespace calc
