#pragma once

#include <cstdio>


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


inline int peek_after_whitespace(FILE* f) {
    int c;
    while ((c = fgetc(f)) != EOF &&
           (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {}
    if (c != EOF) ungetc(c, f);
    return c;
}
