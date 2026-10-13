// lexer.hpp — découpe le texte en jetons (nombres, identifiants, opérateurs).
#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace calc {

// Erreur avec la position (colonne, à partir de 0) pour pointer l'endroit fautif.
class CalcError : public std::runtime_error {
public:
    CalcError(const std::string& msg, std::size_t pos) : std::runtime_error(msg), pos_(pos) {}
    std::size_t position() const { return pos_; }
private:
    std::size_t pos_;
};

enum class Tok { Number, Ident, Plus, Minus, Star, Slash, Percent, Caret, LParen, RParen, Comma, Equal, End };

struct Token {
    Tok type;
    std::string text;
    double value = 0.0;
    std::size_t pos = 0;
};

std::vector<Token> tokenize(const std::string& src);
std::string tokName(Tok t);

}  // namespace calc
