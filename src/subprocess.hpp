#pragma once

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string>
#include <vector>

extern char** environ;

// Startet ein externes Programm, ohne auf es zu warten. Gibt die PID zurück
// (oder -1 bei Fehler). args[0] ist der Programmpfad/-name.
inline pid_t spawn_process(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    // stdout des Kindprozesses unterdrücken (z.B. "s VALID" von dsr-trim/
    // lsr-check) -- wir werten ohnehin nur den Exit-Code aus. stderr bleibt
    // sichtbar, damit echte Fehlermeldungen der Tools ankommen.
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);

    pid_t pid;
    int rc = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    return (rc == 0) ? pid : -1;
}

// Wartet auf den Prozess. Gibt true zurück, wenn er mit Exit-Code 0 beendet wurde.
inline bool wait_for_process(pid_t pid) {
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
