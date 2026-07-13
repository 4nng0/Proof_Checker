#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "../cnf.hpp"

// Liest die nächste durch Whitespace getrennte (ggf. negative) Zahl.
// Gibt false zurück, wenn vor einer Ziffer das Dateiende erreicht wird.
inline bool read_int(FILE* f, int& out) {
    int c, sign = 1, num = 0;
    bool began = false;
    while ((c = fgetc(f)) != EOF) {
        if (c == '-') {
            sign = -1;
            began = true;
        } else if (c >= '0' && c <= '9') {
            num = num * 10 + (c - '0');
            began = true;
        } else if (began) {
            break;
        }
    }
    if (!began) return false;
    out = sign * num;
    return true;
}

// Überspringt Whitespace und gibt das nächste Zeichen zurück, ohne es zu
// konsumieren (EOF bleibt EOF).
inline int peek_after_whitespace(FILE* f) {
    int c;
    while ((c = fgetc(f)) != EOF &&
           (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {}
    if (c != EOF) ungetc(c, f);
    return c;
}

// Wendet einen LSR-Beweis (Additionen/Deletionen) auf `cnf` an. Jede
// Addition-Zeile hat die Form `<id> [klausel] [witness] 0 [hints] 0`. Der
// Witness-Teil (falls vorhanden) beginnt beim erneuten Auftreten des ersten
// Literals ("Pivot") und wird für den Formel-Aufbau ignoriert, ebenso die
// Hints. Deletion-Zeilen haben die Form `<id> d <id1> <id2> ... 0`.
inline bool apply_lsr_proof(Cnf& cnf, const std::string& lsr_path) {
    FILE* f = fopen(lsr_path.c_str(), "r");
    if (!f) return false;

    int id;
    while (read_int(f, id)) {
        if (peek_after_whitespace(f) == 'd') {
            fgetc(f);  // 'd' konsumieren
            int del_id;
            while (read_int(f, del_id) && del_id != 0)
                cnf.remove_clause(del_id);
            continue;
        }

        // Klausel lesen: erstes Literal ist der Pivot. Taucht er erneut auf,
        // beginnt der Witness, dessen Rest bis zur 0 verworfen wird.
        std::vector<int> clause;
        int pivot = 0;
        bool have_pivot = false;
        int lit;
        while (read_int(f, lit) && lit != 0) {
            if (!have_pivot) {
                pivot = lit;
                have_pivot = true;
                clause.push_back(lit);
            } else if (lit == pivot) {
                while (read_int(f, lit) && lit != 0) {}  // Witness überspringen
                break;
            } else {
                clause.push_back(lit);
            }
        }

        int hint;
        while (read_int(f, hint) && hint != 0) {}  // Hints überspringen

        cnf.add_clause(id, std::move(clause));
    }

    fclose(f);
    return true;
}
