#include "lexer.hpp"

#include <cctype>
#include <cerrno>
#include <cstdlib>

namespace calc {

std::string tokName(Tok t) {
    switch (t) {
        case Tok::Number: return "nombre";
        case Tok::Ident: return "identifiant";
        case Tok::Plus: return "'+'";
        case Tok::Minus: return "'-'";
        case Tok::Star: return "'*'";
        case Tok::Slash: return "'/'";
        case Tok::Percent: return "'%'";
        case Tok::Caret: return "'^'";
        case Tok::LParen: return "'('";
        case Tok::RParen: return "')'";
        case Tok::Comma: return "','";
        case Tok::Equal: return "'='";
        case Tok::End: return "fin de l'expression";
    }
    return "?";
}

std::vector<Token> tokenize(const std::string& src) {
    std::vector<Token> out;
    std::size_t i = 0;
    while (i < src.size()) {
        const unsigned char c = static_cast<unsigned char>(src[i]);
        if (std::isspace(c)) { ++i; continue; }
        if (c == '#') break;  // commentaire jusqu'à la fin de la ligne

        if (std::isdigit(c) || (c == '.' && i + 1 < src.size() && std::isdigit(static_cast<unsigned char>(src[i + 1])))) {
            const char* begin = src.c_str() + i;
            char* end = nullptr;
            errno = 0;
            const double v = std::strtod(begin, &end);
            if (errno == ERANGE) throw CalcError("Nombre trop grand", i);
            Token t{Tok::Number, std::string(begin, static_cast<const char*>(end)), v, i};
            i += static_cast<std::size_t>(end - begin);
            if (i < src.size() && (std::isalpha(static_cast<unsigned char>(src[i])) || src[i] == '_'))
                throw CalcError("Nombre mal formé", t.pos);  // ex. « 3x » : opérateur manquant
            out.push_back(t);
            continue;
        }
        if (std::isalpha(c) || c == '_') {
            const std::size_t start = i;
            while (i < src.size() && (std::isalnum(static_cast<unsigned char>(src[i])) || src[i] == '_')) ++i;
            out.push_back({Tok::Ident, src.substr(start, i - start), 0.0, start});
            continue;
        }
        Tok type;
        switch (c) {
            case '+': type = Tok::Plus; break;
            case '-': type = Tok::Minus; break;
            case '*': type = Tok::Star; break;
            case '/': type = Tok::Slash; break;
            case '%': type = Tok::Percent; break;
            case '^': type = Tok::Caret; break;
            case '(': type = Tok::LParen; break;
            case ')': type = Tok::RParen; break;
            case ',': type = Tok::Comma; break;
            case '=': type = Tok::Equal; break;
            default: throw CalcError(std::string("Caractère inattendu '") + src[i] + "'", i);
        }
        out.push_back({type, std::string(1, src[i]), 0.0, i});
        ++i;
    }
    out.push_back({Tok::End, "", 0.0, src.size()});
    return out;
}

}  // namespace calc
