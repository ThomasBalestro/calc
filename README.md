# Calc 🧮

Interpréteur d'expressions mathématiques **en C++17**, sans dépendance : variables, fonctions définies
par l'utilisateur, et des messages d'erreur qui **pointent l'endroit exact** du problème.

```
> rayon = 3
rayon = 3
> pi * rayon ^ 2
= 28.2743338823
> f(x) = x^2 + 1
Fonction f définie.
> f(ans)
= 800.437956488
> 2 + * 3
  2 + * 3
      ^
Erreur : Valeur attendue, trouvé « * »
```

## Démarrage

**Sans compiler (Windows)** : `dist\calc.exe`

**Compiler** : `build.bat` sous Windows, ou :

```bash
cmake -S . -B build && cmake --build build
ctest --test-dir build      # 43 tests
./build/calc
```

Trois façons de l'utiliser :

```bash
calc                          # mode interactif (:aide pour la liste des commandes)
calc "2^10" "hypot(3, 4)"     # évalue les arguments (code retour 1 en cas d'erreur)
calc < formules.calc          # évalue un fichier ligne par ligne (# pour les commentaires)
```

## Le langage

| Élément | Exemples |
|---|---|
| Opérateurs | `+ - * / % ^`, parenthèses, moins unaire |
| Nombres | `42`, `.5`, `1.5e3` |
| Variables | `taux = 0.2`, `ans` (dernier résultat), `pi`, `e` |
| Fonctions utilisateur | `aire(l, h) = l * h`, puis `aire(3, 4)` |
| Fonctions intégrées | `sin cos tan asin acos atan sqrt exp ln log log2 abs floor ceil round pow hypot min max` |
| Commandes | `:aide :vars :fonctions :reset :quitter` |

Règles de priorité standard : `2 ^ 3 ^ 2` vaut 512 (puissance associative à droite)
et `-2 ^ 2` vaut -4 (la puissance passe avant le moins unaire), comme en mathématiques.

## Architecture

Le traitement suit les étapes classiques d'un compilateur :

```
texte ──► lexer ──► jetons ──► parser ──► arbre syntaxique ──► interpréteur ──► résultat
         (lexer.cpp)           (parser.cpp, descente récursive)   (interpreter.cpp)
```

- **Analyseur lexical** : découpe en nombres, identifiants et opérateurs, en mémorisant la position de chaque jeton.
- **Analyseur syntaxique à descente récursive** : une fonction par niveau de priorité (`expr`, `terme`, `unaire`, `puissance`, `primaire`), la grammaire est documentée en tête de `parser.hpp`.
- **Arbre syntaxique** partagé (`shared_ptr<const Node>`) : le corps d'une fonction utilisateur est stocké une fois et réévalué à chaque appel, avec une **portée locale** pour ses paramètres.
- **Erreurs typées** (`CalcError`) portant la colonne fautive, affichée avec un repère `^`.

## Robustesse

Chaque erreur produit un message clair au lieu d'un `nan` silencieux ou d'un plantage :
division ou modulo par zéro, racine d'un nombre négatif, logarithme de zéro, puissance non entière
d'un négatif, dépassement de capacité (`10^400`), nombre d'arguments incorrect, variable ou fonction inconnue,
nom réservé (`pi = 3`), paramètre en double, et **récursion infinie** (`f(n) = f(n+1)`) arrêtée par une limite
de profondeur plutôt que par un dépassement de pile.

## Tests

43 tests dans `tests/test_calc.cpp`, sans framework externe : priorités, associativité, portée des variables,
et pour chaque erreur, **le message et la position exacte**. Ils passent sous Linux (gcc) et Windows (MinGW).
Compilation sans avertissement en `-Wall -Wextra -Wpedantic -Wshadow`.

## Pistes

Conditions (`si(x > 0, x, -x)`) pour permettre la récursion utile, nombres complexes, historique avec
les flèches, tracé de courbes en ASCII.

---
Projet réalisé par **Thomas Balestro** — licence MIT.
