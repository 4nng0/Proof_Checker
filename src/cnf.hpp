#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

// Bringt eine Klausel in kanonische Form: Literale nach Variable sortiert,
// doppelte Literale entfernt. Gibt true zurück, wenn die Klausel dadurch als
// tautologisch erkannt wird (x und -x beide enthalten) -- eine tautologische
// Klausel ist immer erfüllt und wird beim Vergleich ignoriert.
inline bool canonicalize_clause(std::vector<int>& clause) {
    std::sort(clause.begin(), clause.end(), [](int a, int b) {
        int va = std::abs(a), vb = std::abs(b);
        return va != vb ? va < vb : a < b;
    });
    clause.erase(std::unique(clause.begin(), clause.end()), clause.end());

    for (size_t i = 1; i < clause.size(); i++)
        if (clause[i] == -clause[i - 1]) return true;
    return false;
}

// Repräsentiert eine CNF-Formel als ID-indiziertes Klausel-Array (1-indiziert,
// wie in LRAT/LSR-Beweisen). Gelöschte Klauseln werden als std::nullopt
// gehalten statt entfernt, damit IDs stabile Indizes bleiben.
class Cnf {
public:
    // Lädt eine DIMACS-CNF-Datei; Klauseln bekommen IDs 1..n in Dateireihenfolge.
    bool load(const std::string& path) {
        FILE* f = fopen(path.c_str(), "r");
        if (!f) return false;

        clauses_.clear();
        derived_empty_clause_ = false;
        declared_num_vars_ = 0;
        declared_num_clauses_ = 0;

        int next_id = 1;
        std::vector<int> clause;
        auto handle_lit = [&](int lit) {
            if (lit == 0) {
                set_clause(next_id++, std::move(clause));
                clause.clear();
            } else {
                clause.push_back(lit);
            }
        };

        int c, sign = 1, num = 0;
        bool began = false, in_comment = false;
        while ((c = fgetc(f)) != EOF) {
            if (in_comment) {
                if (c == '\n') in_comment = false;
                continue;
            }
            switch (c) {
            case 'c':
                in_comment = true;
                break;
            case 'p': {
                char line[256];
                int len = 0, ch;
                while ((ch = fgetc(f)) != EOF && ch != '\n' && len < (int)sizeof(line) - 1)
                    line[len++] = (char)ch;
                line[len] = '\0';
                int nv = 0, nc = 0;
                if (std::sscanf(line, " cnf %d %d", &nv, &nc) == 2) {
                    declared_num_vars_ = nv;
                    declared_num_clauses_ = nc;
                    clauses_.reserve(nc);
                }
                break;
            }
            case '-':
                sign = -1;
                began = true;
                break;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                num = num * 10 + (c - '0');
                began = true;
                break;
            case ' ': case '\t': case '\n': case '\r':
                if (began) {
                    handle_lit(sign * num);
                    sign = 1; num = 0; began = false;
                }
                break;
            }
        }
        if (began) handle_lit(sign * num);

        fclose(f);
        return true;
    }

    void add_clause(int id, std::vector<int> clause) {
        set_clause(id, std::move(clause));
    }

    void remove_clause(int id) {
        if (id >= 1 && id <= (int)clauses_.size())
            clauses_[id - 1] = std::nullopt;
    }

    // Lädt die Ziel-CNF unter `path` unabhängig und vergleicht sie mit *this,
    // bis auf Klausel-/Literal-Reihenfolge, doppelte Literale/Klauseln und Tautologien 
    //keiner dieser Unterschiede ändert die erfüllenden Belegungen der Formel.
    bool check_equal_with_goal_path(const std::string& path) const {
        Cnf goal;
        if (!goal.load(path)) return false;
        return canonical_clause_set() == goal.canonical_clause_set();
    }

    // true, sobald irgendwann eine leere Klausel hinzugefügt wurde 
    bool has_empty_clause() const { return derived_empty_clause_; }

    int declared_num_vars() const { return declared_num_vars_; }
    int declared_num_clauses() const { return declared_num_clauses_; }

private:
    void set_clause(int id, std::vector<int> clause) {
        if (clause.empty()) derived_empty_clause_ = true;
        if (id > (int)clauses_.size()) {
            if (id > (int)clauses_.capacity())
                clauses_.reserve(std::max<size_t>(id, clauses_.capacity() * 2));
            clauses_.resize(id);
        }
        clauses_[id - 1] = std::move(clause);
    }

    std::vector<std::vector<int>> canonical_clause_set() const {
        std::vector<std::vector<int>> result;
        result.reserve(clauses_.size());
        for (const auto& c : clauses_) {
            if (!c) continue;
            std::vector<int> clause = *c;
            if (!canonicalize_clause(clause))
                result.push_back(std::move(clause));
        }
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

    std::vector<std::optional<std::vector<int>>> clauses_;
    bool derived_empty_clause_ = false;
    int declared_num_vars_ = 0;
    int declared_num_clauses_ = 0;
};
