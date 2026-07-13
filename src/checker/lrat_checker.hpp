#pragma once

#include <atomic>
#include <cassert>
#include <optional>
#include <thread>
#include <string>
#include "../checker_interface.hpp"
#include "../cnf.hpp"
#include "lsr_apply.hpp"
#include "../subprocess.hpp"
#include "external_tools.hpp"

// Verarbeitet LRAT-Beweise direkt mit lsr-check -- kein dsr-trim nötig, LRAT
// ist bereits im "Klausel + Hints"-Format, das lsr-check und apply_lsr_proof
// erwarten.
class LratChecker : public CheckerInterface {
    CheckerPosition _position;
    std::string _cnf_path;
    std::string _proof_file;
    std::optional<std::string> _goal_path;

    std::atomic<bool> _done{false};
    std::atomic<bool> _succeeded{false};
    std::thread _thread;

    bool expects_goal() const {
        return _position == CheckerPosition::START || _position == CheckerPosition::MIDDLE;
    }

public:
    LratChecker(CheckerPosition pos, const std::string& cnf_path, const std::string& proof)
        : _position(pos), _cnf_path(cnf_path), _proof_file(proof) {}

    void set_goal_cnf(const std::string& path) override {
        assert(expects_goal() && "set_goal_cnf ist nur für START/MIDDLE vorgesehen");
        _goal_path = path;
    }

    void start() override {
        assert(_goal_path.has_value() == expects_goal());

        _thread = std::thread([this]() {
            // lsr-check (Legitimität der Beweisschritte, Teil 1) läuft als
            // externer Prozess parallel zu unserer eigenen Anwendung des Beweises.
            pid_t check_pid = spawn_process({LSR_CHECK_PATH, "-q", _cnf_path, _proof_file});

            Cnf cnf;
            bool loaded = cnf.load(_cnf_path);
            bool applied = loaded && apply_lsr_proof(cnf, _proof_file);

            bool check_ok = (check_pid >= 0) && wait_for_process(check_pid);

            // Teil 2: führt der Beweis zur erwarteten Folgeformel (START/MIDDLE)
            // bzw. zur leeren Klausel (END/ONLY)?
            bool result_ok = applied &&
                (_goal_path ? cnf.check_equal_with_goal_path(*_goal_path)
                             : cnf.has_empty_clause());

            _succeeded.store(check_ok && result_ok);
            _done.store(true);
        });
        _thread.detach();
    }

    bool is_done() const override { return _done.load(); }
    bool succeeded() const override { return _succeeded.load(); }
};
