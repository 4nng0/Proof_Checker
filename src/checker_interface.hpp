#pragma once

#include <string>

enum class CheckerPosition { START, MIDDLE, END, ONLY };

class CheckerInterface {
public:
    
    virtual void start() = 0;
    virtual bool is_done() const = 0;     // Beweis vollständig geprüft
    virtual bool succeeded() const = 0;   // nur gültig wenn is_done()
    virtual void set_goal_cnf(const std::string& path) = 0;
    virtual ~CheckerInterface() = default;
};
