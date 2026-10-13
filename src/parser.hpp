// parser.hpp — analyse syntaxique par descente récursive, produit un arbre (AST).
//
// Grammaire (du moins au plus prioritaire) :
//   instruction := IDENT '(' params ')' '=' expr      définition de fonction
//                | IDENT '=' expr                     affectation
//                | expr
//   expr   := terme (('+' | '-') terme)*
//   terme  := unaire (('*' | '/' | '%') unaire)*
//   unaire := ('-' | '+') unaire | puissance
//   puissance := primaire ('^' unaire)?               associatif à droite : 2^3^2 = 2^9
//   primaire  := NOMBRE | IDENT | IDENT '(' args ')' | '(' expr ')'
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lexer.hpp"

namespace calc {

struct Node;
using NodePtr = std::shared_ptr<const Node>;

struct Node {
    enum class Kind { Number, Variable, Unary, Binary, Call };
    Kind kind = Kind::Number;
    double value = 0.0;            // Number
    std::string name;              // Variable, Call
    char op = 0;                   // Unary, Binary
    std::vector<NodePtr> children; // opérandes ou arguments
    std::size_t pos = 0;           // position dans le texte, pour les messages d'erreur
};

struct Statement {
    enum class Kind { Expression, Assignment, FunctionDef } kind;
    std::string name;                 // variable ou fonction
    std::vector<std::string> params;  // FunctionDef
    NodePtr body;
};

Statement parse(const std::string& source);

}  // namespace calc
