#pragma once

#include <atomic>
#include <cassert>
#include <filesystem>
#include <optional>
#include <thread>
#include <string>
#include "../checker_interface.hpp"
#include "../cnf.hpp"
#include "lsr_apply.hpp"
#include "../subprocess.hpp"
#include "external_tools.hpp"

// Dummy: prüft PALRUP-Beweise (noch) nicht wirklich, meldet immer Erfolg.
class PalRupChecker : public CheckerInterface {
    CheckerPosition _position;
    std::string _cnf_path;
    std::string _proof_file;
    std::optional<std::string> _goal_path;

    std::atomic<bool> _done{false};
    std::atomic<bool> _succeeded{false};
    std::thread _thread;

    bool expects_goal() const {
        return false;
    }

public:
    PalRupChecker(CheckerPosition pos, const std::string& cnf_path, const std::string& proof)
        : _position(pos), _cnf_path(cnf_path), _proof_file(proof) {
            assert(pos == CheckerPosition::END || pos == CheckerPosition::ONLY);
        }

    void set_goal_cnf(const std::string& path) override {
        //das sollte nicht passieren
        assert(false);
        
    }

    void start() override {
        //hier erstmal dummy checken 
        _succeeded.store(true);
        _done.store(true); 
        

    }

    bool is_done() const override { return _done.load(); }
    bool succeeded() const override { return _succeeded.load(); }
};
