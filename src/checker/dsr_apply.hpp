#pragma once

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "../cnf.hpp"
#include "parse_util.hpp"

inline bool apply_dsr_proof(Cnf& cnf, const std::string& dsr_path) {
    FILE* f = fopen(dsr_path.c_str(), "r");
    if (!f) return false;

    int next_id = cnf.next_free_id();
    int c;
    while ((c = peek_after_whitespace(f)) != EOF) {
        bool is_deletion = (c == 'd');
        if (is_deletion) fgetc(f);  // consume the 'd'

        std::vector<int> clause;
        int pivot = 0;
        bool have_pivot = false;
        int lit;
        while (read_int(f, lit) && lit != 0) {
            if (!have_pivot) {
                pivot = lit;
                have_pivot = true;
                clause.push_back(lit);
            } else if (!is_deletion && lit == pivot) {
                while (read_int(f, lit) && lit != 0) {}  // skip the witness
                break;
            } else {
                clause.push_back(lit);
            }
        }

        if (is_deletion)
            cnf.remove_clause_by_literals(std::move(clause));
        else
            cnf.add_clause(next_id++, std::move(clause));
    }

    fclose(f);
    return true;
}
