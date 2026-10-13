// test_calc.cpp — tests de l'interpréteur, sans dépendance externe.
#include <cmath>
#include <functional>
#include <iostream>
#include <string>

#include "interpreter.hpp"

namespace {
int g_total = 0, g_fail = 0;

void check(bool ok, const std::string& what, int line) {
    ++g_total;
    if (!ok) { ++g_fail; std::cout << "ÉCHEC ligne " << line << " : " << what << "\n"; }
}
#define CHECK(c) check((c), #c, __LINE__)

bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

// Vérifie qu'une instruction lève une erreur, contenant `fragment`, à la position `pos` (si >= 0).
void expectError(calc::Interpreter& in, const std::string& src, const std::string& fragment, int pos, int line) {
    ++g_total;
    try {
        in.execute(src);
        ++g_fail;
        std::cout << "ÉCHEC ligne " << line << " : aucune erreur pour « " << src << " »\n";
    } catch (const calc::CalcError& e) {
        const bool msgOk = std::string(e.what()).find(fragment) != std::string::npos;
        const bool posOk = pos < 0 || static_cast<int>(e.position()) == pos;
        if (!msgOk || !posOk) {
            ++g_fail;
            std::cout << "ÉCHEC ligne " << line << " : « " << src << " » -> \"" << e.what() << "\" en position " << e.position() << "\n";
        }
    }
}
#define ERR(in, src, frag, pos) expectError(in, src, frag, pos, __LINE__)
}  // namespace

int main() {
    calc::Interpreter in;

    // Priorités et associativité
    CHECK(near(in.eval("1 + 2 * 3"), 7));
    CHECK(near(in.eval("(1 + 2) * 3"), 9));
    CHECK(near(in.eval("2 ^ 3 ^ 2"), 512));     // associatif à droite
    CHECK(near(in.eval("-2 ^ 2"), -4));         // la puissance passe avant le moins unaire
    CHECK(near(in.eval("2 ^ -1"), 0.5));
    CHECK(near(in.eval("10 - 4 - 3"), 3));      // associatif à gauche
    CHECK(near(in.eval("7 % 3"), 1));
    CHECK(near(in.eval("--3"), 3));
    CHECK(near(in.eval(".5 + 1.5e2"), 150.5));

    // Variables, constantes, ans
    CHECK(near(in.eval("rayon = 2"), 2));
    CHECK(near(in.eval("pi * rayon ^ 2"), 4 * std::acos(-1.0)));
    CHECK(near(in.eval("ans / 2"), 2 * std::acos(-1.0)));
    CHECK(near(in.eval("e"), std::exp(1.0)));

    // Fonctions intégrées et utilisateur
    CHECK(near(in.eval("sqrt(16) + abs(-2)"), 6));
    CHECK(near(in.eval("max(3, 9, 4) - min(5, 1)"), 8));
    CHECK(near(in.eval("hypot(3, 4)"), 5));
    in.execute("f(x) = x ^ 2 + 1");
    in.execute("aire(l, h) = l * h");
    CHECK(near(in.eval("f(3)"), 10));
    CHECK(near(in.eval("aire(f(1), 4)"), 8));
    in.execute("x = 100");
    CHECK(near(in.eval("f(2)"), 5));            // le paramètre masque la variable globale
    CHECK(near(in.eval("x"), 100));

    // Erreurs : message clair ET position exacte
    ERR(in, "1 / 0", "Division par zéro", 2);
    ERR(in, "5 % 0", "Modulo par zéro", 2);
    ERR(in, "inconnu + 1", "Variable inconnue", 0);
    ERR(in, "2 + * 3", "Valeur attendue", 4);
    ERR(in, "(1 + 2", "')' attendu", 6);
    ERR(in, "1 + 2)", "Inattendu", 5);
    ERR(in, "3x", "Nombre mal formé", 0);
    ERR(in, "2 $ 3", "Caractère inattendu", 2);
    ERR(in, "sqrt(-1)", "négatif", 0);
    ERR(in, "ln(0)", "Logarithme", 0);
    ERR(in, "(-8) ^ 0.5", "non entière", 5);
    ERR(in, "sqrt(1, 2)", "attend 1 argument", 0);
    ERR(in, "f(1, 2)", "attend 1 argument", 0);
    ERR(in, "nexiste(1)", "Fonction inconnue", 0);
    ERR(in, "pi = 3", "réservé", -1);
    ERR(in, "sin(x) = x", "intégrée", -1);
    ERR(in, "g(a, a) = a", "en double", 5);
    ERR(in, "10 ^ 400", "hors limites", 3);
    ERR(in, "f", "est une fonction", 0);
    ERR(in, "", "Valeur attendue", 0);

    // Récursion infinie : erreur propre, pas de plantage de la pile
    in.execute("boucle(n) = boucle(n + 1)");
    ERR(in, "boucle(1)", "Récursion trop profonde", -1);

    // Une erreur ne casse pas l'état : on peut continuer
    CHECK(near(in.eval("rayon"), 2));
    in.reset();
    ERR(in, "rayon", "Variable inconnue", 0);

    std::cout << (g_total - g_fail) << "/" << g_total << " tests réussis\n";
    return g_fail ? 1 : 0;
}
