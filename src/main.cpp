// main.cpp — interface de Calc : REPL interactif, évaluation d'arguments ou d'un flux.
//
//   calc                     mode interactif
//   calc "2 * (3 + 4)"       évalue les expressions passées en arguments
//   calc < script.calc       évalue un fichier ligne par ligne
#include <iostream>
#include <string>

#include "interpreter.hpp"

#ifdef _WIN32
#  include <io.h>
#  include <windows.h>
#  define IS_TTY() _isatty(_fileno(stdin))
#else
#  include <unistd.h>
#  define IS_TTY() isatty(fileno(stdin))
#endif

namespace {

void printHelp() {
    std::cout <<
        "Calc — interpréteur d'expressions\n"
        "  Opérateurs   + - * / % ^ ( )        ex. 2 ^ 10 / (3 + 1)\n"
        "  Variables    rayon = 3              ans = dernier résultat, pi et e prédéfinis\n"
        "  Fonctions    f(x) = x^2 + 1         puis f(4)\n"
        "  Intégrées    sin cos tan asin acos atan sqrt exp ln log log2\n"
        "               abs floor ceil round pow hypot min max\n"
        "  Commandes    :aide  :vars  :fonctions  :reset  :quitter\n";
}

// Affiche l'erreur avec un repère sous la position fautive.
void printError(const std::string& line, const calc::CalcError& e, bool showCaret) {
    if (showCaret) std::cerr << "  " << line << "\n  " << std::string(e.position(), ' ') << "^\n";
    std::cerr << "Erreur : " << e.what() << "\n";
}

bool handle(calc::Interpreter& in, const std::string& raw, bool interactive) {
    const auto start = raw.find_first_not_of(" \t\r");
    if (start == std::string::npos) return true;
    const std::string line = raw.substr(start, raw.find_last_not_of(" \t\r") - start + 1);
    if (line[0] == '#') return true;

    if (line[0] == ':') {
        if (line == ":quitter" || line == ":q") return false;
        if (line == ":aide" || line == ":h") printHelp();
        else if (line == ":vars") for (const auto& v : in.variables()) std::cout << "  " << v << "\n";
        else if (line == ":fonctions") {
            const auto fs = in.functions();
            if (fs.empty()) std::cout << "  (aucune fonction définie)\n";
            for (const auto& f : fs) std::cout << "  " << f << "\n";
        } else if (line == ":reset") { in.reset(); std::cout << "  Tout est effacé.\n"; }
        else std::cerr << "Commande inconnue (tapez :aide)\n";
        return true;
    }
    try {
        const calc::Result r = in.execute(line);
        switch (r.kind) {
            case calc::Result::Kind::Value: std::cout << (interactive ? "= " : "") << calc::formatNumber(r.value) << "\n"; break;
            case calc::Result::Kind::Assigned: std::cout << r.name << " = " << calc::formatNumber(r.value) << "\n"; break;
            case calc::Result::Kind::Defined: if (interactive) std::cout << "Fonction " << r.name << " définie.\n"; break;
        }
    } catch (const calc::CalcError& e) {
        printError(line, e, interactive);
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // accents corrects dans la console Windows
#endif
    calc::Interpreter in;

    if (argc > 1) {  // expressions passées en arguments
        int status = 0;
        for (int i = 1; i < argc; ++i) {
            try {
                std::cout << calc::formatNumber(in.execute(argv[i]).value) << "\n";
            } catch (const calc::CalcError& e) {
                printError(argv[i], e, true);
                status = 1;
            }
        }
        return status;
    }

    const bool interactive = IS_TTY();
    if (interactive) std::cout << "Calc 1.0 — tapez :aide pour l'aide, :quitter pour sortir.\n";
    std::string line;
    while (true) {
        if (interactive) std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (!handle(in, line, interactive)) break;
    }
    return 0;
}
