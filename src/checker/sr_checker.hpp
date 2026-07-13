#pragma once

#include <atomic>
#include <cassert>
#include <filesystem>
#include <optional>
#include <thread>
#include <string>
#include <vector>
#include "../checker_interface.hpp"
#include "../cnf.hpp"
#include "lsr_apply.hpp"
#include "../subprocess.hpp"
#include "external_tools.hpp"

// Verarbeitet DRAT/SR-Beweise: erst dsr-trim → LSR, dann lsr-check
class SrChecker : public CheckerInterface {
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
    SrChecker(CheckerPosition pos, const std::string& cnf_path, const std::string& proof)
        : _position(pos), _cnf_path(cnf_path), _proof_file(proof) {}

    void set_goal_cnf(const std::string& path) override {
        assert(expects_goal() && "set_goal_cnf ist nur für START/MIDDLE vorgesehen");
        _goal_path = path;
    }

    void start() override {
        // Muss zu diesem Zeitpunkt schon feststehen -- START/MIDDLE brauchen ein
        // Ziel, END/ONLY müssen stattdessen selbst UNSAT zeigen.
        assert(_goal_path.has_value() == expects_goal());

        _thread = std::thread([this]() {
            std::string lsr_path = (std::filesystem::temp_directory_path()
                / (std::filesystem::path(_proof_file).filename().string() + ".lsr")).string();

            // dsr-trim wandelt den DSR/DRAT-Beweis in einen LSR-Beweis um (blockierend,
            // denn erst danach existiert die LSR-Datei, die wir als Nächstes brauchen).
            // -f (forwards checking) statt dsr-trims Standard (backwards checking, das
            // eine Widerlegung mit leerer Klausel am Ende voraussetzt): backwards ist nur
            // eine Performance-Optimierung, die kausal irrelevante Klauseln überspringt --
            // wir brauchen diese Annahme nicht, weil wir selbst über has_empty_clause()/
            // check_equal_with_goal_path() prüfen, ob das Ergebnis stimmt (Teil 2).
            // dsr-trim prüft mit -f trotzdem weiterhin jeden Schritt auf Legitimität (Teil 1).
            pid_t trim_pid = spawn_process({DSR_TRIM_PATH, "-q", "-f", _cnf_path, _proof_file, lsr_path});
            if (trim_pid < 0 || !wait_for_process(trim_pid)) {
                _succeeded.store(false);
                _done.store(true);
                return;
            }

            // lsr-check (Legitimität der Beweisschritte, Teil 1) läuft als
            // externer Prozess parallel zu unserer eigenen Anwendung des Beweises.
            pid_t check_pid = spawn_process({LSR_CHECK_PATH, "-q", _cnf_path, lsr_path});

            Cnf cnf;
            bool loaded = cnf.load(_cnf_path);
            bool applied = loaded && apply_lsr_proof(cnf, lsr_path);

            bool check_ok = (check_pid >= 0) && wait_for_process(check_pid);
            std::filesystem::remove(lsr_path);

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
