#include <iostream>
#include <filesystem>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <thread>
#include <regex>

#include "checker_factory.hpp"

namespace fs = std::filesystem;

static const std::array<std::string, 3> allowed_extensions = {".lrat", ".sr", ".palrup"};

static int parse_step_number(const fs::path& path) {
    std::string stem = path.stem().string();
    std::string ext  = path.extension().string();
    std::smatch m;

    if (std::regex_match(stem, m, std::regex(R"(post(\d+))")) && ext == ".cnf")
        return std::stoi(m[1].str());

    if (std::regex_match(stem, m, std::regex(R"(step(\d+))"))) {
        for (const auto& e : allowed_extensions)
            if (ext == e) return std::stoi(m[1].str());
    }

    return -1;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <formula.cnf> <proof-dir>\n";
        return 1;
    }

    fs::path formulaPath(argv[1]);
    fs::path proofDir(argv[2]);

    if (!fs::exists(formulaPath)) {
        std::cerr << "Error: formula file not found: " << formulaPath << "\n";
        return 1;
    }
    if (!fs::is_directory(proofDir)) {
        std::cerr << "Error: proof directory not found: " << proofDir << "\n";
        return 1;
    }

    // Dateien aufteilen: 's'-Dateien (step) und 'p'-Dateien (post)
    std::map<int, fs::path> stepMap;
    std::map<int, fs::path> postMap;

    for (const auto& entry : fs::directory_iterator(proofDir)) {
        // Kein is_regular_file()-Filter: ein Beweis kann auch ein Verzeichnis
        // sein (z.B. step2.palrup/ mit mehreren Dateien drin).
        const fs::path& p = entry.path();
        std::string stem = p.stem().string();
        if (stem.empty()) continue;
        int n = parse_step_number(p);
        if (n < 0) {
            std::cerr << "Error: unexpected file in proof directory: "
                      << p.filename() << "\n"
                      << "  Expected: step<N>.<lrat|sr|palrup> or post<N>.cnf\n";
            return 1;
        }

        if (stem.find("step") == 0)
            stepMap[n] = p;
        else if (stem.find("post") == 0)
            postMap[n] = p;
    }

    if (stepMap.empty()) {
        std::cerr << "Error: no step files found in " << proofDir << "\n";
        return 1;
    }

    int total = (int)stepMap.size();

    // post muss genau 1 kleiner sein als step (kein post nach letztem Schritt)
    if ((int)postMap.size() != total - 1) {
        std::cerr << "Error: expected " << total - 1 << " post files, found "
                  << postMap.size() << "\n";
        return 1;
    }

    std::cout << "Formula:   " << fs::absolute(formulaPath) << "\n";
    std::cout << "Proof dir: " << fs::absolute(proofDir) << "\n";
    std::cout << "Steps:     " << total << "\n\n";

    // Alle Checker erstellen. Jeder ist komplett unabhängig: er lädt seine
    // eigene Eingabe-CNF, wendet seinen eigenen Beweis an, und vergleicht das
    // Ergebnis entweder mit der nächsten post<N>.cnf (per set_goal_cnf, für
    // START/MIDDLE) oder zeigt selbst UNSAT (END/ONLY, kein Ziel gesetzt).
    std::vector<std::unique_ptr<CheckerInterface>> checkers;

    if (total == 1) {
        checkers.push_back(make_checker(CheckerPosition::ONLY, formulaPath.string(), stepMap[1].string()));
    } else {
        checkers.push_back(make_checker(CheckerPosition::START, formulaPath.string(), stepMap[1].string()));
        for (int i = 2; i < total; i++)
            checkers.push_back(make_checker(CheckerPosition::MIDDLE, postMap[i-1].string(), stepMap[i].string()));
        checkers.push_back(make_checker(CheckerPosition::END, postMap[total-1].string(), stepMap[total].string()));

        for (int i = 0; i < total - 1; i++)
            checkers[i]->set_goal_cnf(postMap[i + 1].string());
    }

    for (auto& c : checkers) c->start();

    // Meldet jeden Schritt (Datei + OK/FAILED), sobald er fertig ist, und
    // bricht beim ersten Fehler sofort ab.
    std::vector<bool> reported(checkers.size(), false);
    bool any_pending = true;
    while (any_pending) {
        any_pending = false;
        for (size_t i = 0; i < checkers.size(); i++) {
            if (reported[i]) continue;
            if (!checkers[i]->is_done()) {
                any_pending = true;
                continue;
            }
            reported[i] = true;
            bool ok = checkers[i]->succeeded();
            std::cout << "Step " << i + 1 << " (" << stepMap[(int)i + 1].filename().string() << "): "
                      << (ok ? "OK" : "FAILED") << "\n";
            if (!ok) {
                std::cerr << "\nError: proof verification failed\n";
                return 1;
            }
        }
        if (any_pending) std::this_thread::yield();
    }

    std::cout << "\nAll steps verified.\n";
    return 0;
}
