#pragma once

#include <fcntl.h>
#include <spawn.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string>
#include <vector>

extern char** environ;


inline pid_t spawn_process(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    // Suppress the child's stdout (e.g. "s VALID" from dsr-trim/lsr-check) --
    // we only evaluate the exit code anyway. stderr stays visible so that real
    // error messages from the tools get through.
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);

    pid_t pid;
    int rc = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    return (rc == 0) ? pid : -1;
}


inline pid_t spawn_process_capture_stderr(const std::vector<std::string>& args, int& err_read_fd) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    
    int pipefd[2];
    if (pipe2(pipefd, O_CLOEXEC) != 0) {
        err_read_fd = -1;
        return -1;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipefd[0]);
    posix_spawn_file_actions_addclose(&actions, pipefd[1]);

    pid_t pid;
    int rc = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[1]);

    if (rc != 0) {
        close(pipefd[0]);
        err_read_fd = -1;
        return -1;
    }
    err_read_fd = pipefd[0];
    return pid;
}

// Waits for the process. Returns true if it exited with code 0.
inline bool wait_for_process(pid_t pid) {
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}


inline bool wait_for_process(pid_t pid, double& cpu_seconds) {
    int status = 0;
    struct rusage usage;
    if (wait4(pid, &status, 0, &usage) < 0) {
        cpu_seconds = 0.0;
        return false;
    }
    cpu_seconds = (usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6)
                + (usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
