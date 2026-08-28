#pragma once

#include <string>

enum class CheckerPosition { START, MIDDLE, END, ONLY };

class CheckerInterface {
public:
    
    virtual void start() = 0;
    virtual bool is_done() const = 0;     // proof fully checked
    virtual bool succeeded() const = 0;   // only valid once is_done()
    virtual double cpu_seconds() const = 0;  // only valid once is_done() -- CPU time (user+sys), not wall clock
    virtual double wall_seconds() const = 0; // only valid once is_done() -- actual elapsed time of this step
    virtual void set_goal_cnf(const std::string& path) = 0;

    
    virtual bool checker_ok() const = 0;
    virtual bool cnf_match_ok() const = 0;
    virtual bool has_cnf_match() const { return true; }
    virtual ~CheckerInterface() = default;
};
