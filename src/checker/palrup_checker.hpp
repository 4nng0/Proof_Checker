#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include "../checker_interface.hpp"
#include "../cnf.hpp"
#include "lsr_apply.hpp"
#include "../subprocess.hpp"
#include "external_tools.hpp"

namespace fs = std::filesystem;

class PalRupChecker : public CheckerInterface {
    CheckerPosition _position;
    std::string _cnf_path;
    std::string _proof_file;

    std::atomic<bool> _done{false};
    std::atomic<bool> _succeeded{false};
    std::thread _thread;

    bool expects_goal() const {
        return false;
    }

    
    static std::optional<std::pair<fs::path, unsigned>> locate_fragments(const fs::path& proof_dir) {
        std::vector<unsigned> pal_ids;
        std::optional<fs::path> palrup_path;

        std::error_code ec;
        auto it = fs::recursive_directory_iterator(proof_dir, ec);
        if (ec) return std::nullopt;
        for (const auto& entry : it) {
            if (entry.path().filename() != "out.palrup") continue;

            fs::path pal_dir = entry.path().parent_path();
            fs::path hierarchy_dir = pal_dir.parent_path();
            fs::path this_palrup_path = hierarchy_dir.parent_path();

            if (!palrup_path) palrup_path = this_palrup_path;
            else if (*palrup_path != this_palrup_path) return std::nullopt;

            try {
                size_t consumed = 0;
                unsigned long id = std::stoul(pal_dir.filename().string(), &consumed);
                if (consumed != pal_dir.filename().string().size()) return std::nullopt;
                pal_ids.push_back((unsigned)id);
            } catch (...) {
                return std::nullopt;
            }
        }
        if (!palrup_path || pal_ids.empty()) return std::nullopt;

        // pal-ids müssen lückenlos 0..n-1 sein, damit num-solvers eindeutig ist.
        std::sort(pal_ids.begin(), pal_ids.end());
        for (size_t i = 0; i < pal_ids.size(); i++)
            if (pal_ids[i] != i) return std::nullopt;

        return std::make_pair(*palrup_path, (unsigned)pal_ids.size());
    }

    // Startet alle Kommandos einer Stufe parallel und wartet, bis jedes
    // fertig ist -- true nur, wenn ausnahmslos jedes mit Exit-Code 0 endete.
    static bool run_stage(const std::vector<std::vector<std::string>>& commands) {
        std::vector<pid_t> pids;
        pids.reserve(commands.size());
        for (const auto& cmd : commands)
            pids.push_back(spawn_process(cmd));

        bool ok = true;
        for (pid_t pid : pids)
            ok = (pid >= 0 && wait_for_process(pid)) && ok;
        return ok;
    }

    bool check_stages(const fs::path& palrup_path, const fs::path& working_dir,
                       unsigned num_solvers, unsigned root_ceil, unsigned comm_size) const {
        // Stufe 1: 
        
        std::vector<std::vector<std::string>> local_check_cmds;
        for (unsigned i = 0; i < num_solvers; i++)
            local_check_cmds.push_back({PALRUP_LOCAL_CHECK_PATH,
                "-formula-path=" + _cnf_path,
                "-palrup-path=" + palrup_path.string(),
                "-working-path=" + working_dir.string(),
                "-num-solvers=" + std::to_string(num_solvers),
                "-pal-id=" + std::to_string(i)});
        if (!run_stage(local_check_cmds)) return false;

        // Stufe 2: 
        std::vector<std::vector<std::string>> redistribute_cmds;
        for (unsigned i = 0; i < comm_size; i++)
            redistribute_cmds.push_back({PALRUP_REDISTRIBUTE_PATH,
                "-working-path=" + working_dir.string(),
                "-num-solvers=" + std::to_string(num_solvers),
                "-pal-id=" + std::to_string(i)});
        if (!run_stage(redistribute_cmds)) return false;

        // Stufe 3: 
        std::vector<std::vector<std::string>> confirm_cmds;
        for (unsigned i = 0; i < num_solvers; i++)
            confirm_cmds.push_back({PALRUP_CONFIRM_PATH,
                "-palrup-path=" + palrup_path.string(),
                "-working-path=" + working_dir.string(),
                "-num-solvers=" + std::to_string(num_solvers),
                "-pal-id=" + std::to_string(i)});
        if (!run_stage(confirm_cmds)) return false;

        std::error_code ec;
        if (!fs::is_directory(working_dir / ".unsat_found", ec)) return false;

        unsigned check_ok_count = 0;
        for (unsigned h = 0; h < root_ceil; h++) {
            fs::path hierarchy_dir = working_dir / std::to_string(h);
            if (!fs::is_directory(hierarchy_dir, ec)) continue;
            for (const auto& entry : fs::directory_iterator(hierarchy_dir, ec))
                if (fs::is_directory(entry.path() / ".check_ok", ec)) check_ok_count++;
        }
        return check_ok_count == num_solvers;
    }

    bool run_check() const {
        auto located = locate_fragments(_proof_file);
        if (!located) return false;
        const fs::path& palrup_path = located->first;
        unsigned num_solvers = located->second;

        unsigned root_ceil = (unsigned)std::ceil(std::sqrt((double)num_solvers));
        unsigned comm_size = root_ceil * root_ceil;

        fs::path working_dir = fs::temp_directory_path() /
            ("palrup_check_" + std::to_string(getpid()) + "_" +
             std::to_string(reinterpret_cast<uintptr_t>(this)));

        std::error_code ec;
        fs::remove_all(working_dir, ec);
        
        for (unsigned i = 0; i < comm_size; i++)
            fs::create_directories(working_dir / std::to_string(i / root_ceil) / std::to_string(i), ec);

        bool ok = check_stages(palrup_path, working_dir, num_solvers, root_ceil, comm_size);

        fs::remove_all(working_dir, ec);
        
        for (unsigned i = 0; i < num_solvers; i++) {
            fs::path fragment = palrup_path / std::to_string(i / root_ceil) / std::to_string(i) / "out.palrup";
            fs::remove(fragment.string() + ".hash", ec);
        }

        return ok;
    }

public:
    PalRupChecker(CheckerPosition pos, const std::string& cnf_path, const std::string& proof)
        : _position(pos), _cnf_path(cnf_path), _proof_file(proof) {
            assert(pos == CheckerPosition::END || pos == CheckerPosition::ONLY);
        }

    void set_goal_cnf(const std::string& path) override {
        assert(false && "set_goal_cnf ist nur für START/MIDDLE vorgesehen, PALRUP ist immer END/ONLY");
    }

    void start() override {
        _thread = std::thread([this]() {
            bool ok = run_check();
            _succeeded.store(ok);
            _done.store(true);
        });
        _thread.detach();
    }

    bool is_done() const override { return _done.load(); }
    bool succeeded() const override { return _succeeded.load(); }
};
