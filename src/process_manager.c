#define _POSIX_C_SOURCE 200809L
#include "../include/process_manager.h"
#include "../include/logger.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static ChildProcess g_children[MAX_CHILDREN];
static int g_child_count = 0;

static int find_child_by_pid(pid_t pid) {
    for (int i = 0; i < g_child_count; i++) {
        if (g_children[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

static int count_running_children(void) {
    int count = 0;
    for (int i = 0; i < g_child_count; i++) {
        if (g_children[i].state == PROC_RUNNING && g_children[i].pid > 0) {
            count++;
        }
    }
    return count;
}

static void attempt_restart(int index) {
    ChildProcess *child = &g_children[index];

    if (child->restart_count < MAX_RESTARTS) {
        child->restart_count++;
        printf("[RESTART] '%s' beklenmedik sekilde sonlandi. Yeniden baslatiliyor (%d/%d)...\n",
               child->name, child->restart_count, MAX_RESTARTS);
        logger_write("RESTART", "'%s' exited unexpectedly. Restarting (%d/%d).",
               child->name, child->restart_count, MAX_RESTARTS);
        if (start_child_process(index) != 0) {
            child->state = PROC_FAILED;
            fprintf(stderr, "[ERROR] '%s' yeniden baslatilamadi.\n", child->name);
        }
    } else {
        child->state = PROC_FAILED;
        printf("[LIMIT] '%s' icin maksimum yeniden baslatma limitine (%d) ulasildi. Yeniden baslatilmayacak.\n",
               child->name, MAX_RESTARTS);
        logger_write("LIMIT", "'%s' reached the restart limit (%d). No further restarts.",
               child->name, MAX_RESTARTS);
    }
}

int add_child_process(const char *name, const char *exec_path, const char *mode) {
    if (g_child_count >= MAX_CHILDREN || name == NULL || exec_path == NULL) {
        return -1;
    }

    ChildProcess *child = &g_children[g_child_count];
    memset(child, 0, sizeof(*child));

    child->id = g_child_count + 1;
    snprintf(child->name, sizeof(child->name), "%s", name);
    snprintf(child->exec_path, sizeof(child->exec_path), "%s", exec_path);
    snprintf(child->mode, sizeof(child->mode), "%s", (mode != NULL) ? mode : "loop");
    child->pid = -1;
    child->state = PROC_STOPPED;
    child->restart_count = 0;
    child->last_exit_code = 0;
    child->last_term_signal = 0;

    g_child_count++;
    return g_child_count - 1;
}

int init_process_manager(const char *worker_path) {
    const char *path = (worker_path != NULL && worker_path[0] != '\0') ? worker_path : "./worker";

    g_child_count = 0;
    memset(g_children, 0, sizeof(g_children));

    if (add_child_process("worker-1", path, "loop") < 0) {
        return -1;
    }
    if (add_child_process("worker-2", path, "loop") < 0) {
        return -1;
    }

    return 0;
}

int start_child_process(int index) {
    if (index < 0 || index >= g_child_count) {
        return -1;
    }

    ChildProcess *child = &g_children[index];

    // fork() oncesi tamponlari bosaltarak ciktilarin cocukta tekrarlanmasini onle
    fflush(NULL);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        // Cocuk surec: sinyal maskesini temizle ve varsayilan davranislara don
        sigset_t empty_mask;
        sigemptyset(&empty_mask);
        sigprocmask(SIG_SETMASK, &empty_mask, NULL);

        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGUSR1, SIG_DFL);
        signal(SIGALRM, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        char *argv[] = {
            child->exec_path,
            child->name,
            child->mode,
            NULL
        };

        execv(child->exec_path, argv);
        perror("execv");
        _exit(127);
    }

    // Ebeveyn surec
    child->pid = pid;
    child->state = PROC_RUNNING;

    printf("[PROCESS] '%s' baslatildi (PID: %d, mode: %s)\n",
           child->name, (int)child->pid, child->mode);
    logger_write("PROCESS", "'%s' started (PID: %d, mode: %s)",
           child->name, (int)child->pid, child->mode);

    return 0;
}

int start_all_children(void) {
    int failures = 0;
    for (int i = 0; i < g_child_count; i++) {
        if (start_child_process(i) != 0) {
            failures++;
        }
    }
    return (failures == 0) ? 0 : -1;
}

void handle_terminated_children(int allow_restart) {
    while (1) {
        int status = 0;
        pid_t pid = waitpid(-1, &status, WNOHANG);

        if (pid > 0) {
            int idx = find_child_by_pid(pid);
            if (idx < 0) {
                continue;
            }

            ChildProcess *child = &g_children[idx];
            child->pid = -1;

            if (WIFEXITED(status)) {
                int exit_code = WEXITSTATUS(status);
                child->last_exit_code = exit_code;
                child->last_term_signal = 0;

                if (exit_code == 0) {
                    child->state = PROC_EXITED_NORMAL;
                    printf("[EXIT] '%s' (PID: %d) normal sekilde sonlandi (exit code: 0).\n",
                           child->name, (int)pid);
                    logger_write("EXIT", "'%s' (PID: %d) exited normally (exit code: 0).",
                           child->name, (int)pid);
                } else {
                    child->state = PROC_EXITED_ERROR;
                    printf("[EXIT] '%s' (PID: %d) hata kodu ile sonlandi (exit code: %d).\n",
                           child->name, (int)pid, exit_code);
                    logger_write("EXIT", "'%s' (PID: %d) exited with an error (exit code: %d).",
                           child->name, (int)pid, exit_code);
                    if (allow_restart) {
                        attempt_restart(idx);
                    }
                }
            } else if (WIFSIGNALED(status)) {
                int term_sig = WTERMSIG(status);
                child->last_term_signal = term_sig;
                child->last_exit_code = -1;
                child->state = PROC_EXITED_ERROR;

                printf("[EXIT] '%s' (PID: %d) sinyal ile sonlandi (signal: %d).\n",
                       child->name, (int)pid, term_sig);
                logger_write("EXIT", "'%s' (PID: %d) was terminated by a signal (signal: %d).",
                       child->name, (int)pid, term_sig);
                if (allow_restart) {
                    attempt_restart(idx);
                }
            }
        } else if (pid == 0) {
            break;
        } else {
            if (errno == EINTR) {
                continue;
            }
            if (errno != ECHILD) {
                perror("waitpid");
            }
            break;
        }
    }
}

int start_periodic_timer(int interval_sec) {
    if (interval_sec <= 0) {
        interval_sec = DEFAULT_CHECK_INTERVAL_SEC;
    }

    struct itimerval timer;
    memset(&timer, 0, sizeof(timer));
    timer.it_interval.tv_sec = interval_sec;
    timer.it_interval.tv_usec = 0;
    timer.it_value.tv_sec = interval_sec;
    timer.it_value.tv_usec = 0;

    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("setitimer");
        return -1;
    }

    return 0;
}

void stop_periodic_timer(void) {
    struct itimerval timer;
    memset(&timer, 0, sizeof(timer));
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("setitimer(stop)");
    }
}

void perform_periodic_check(int allow_restart) {
    // Once bekleyen herhangi bir sonlanmis cocugu topla
    handle_terminated_children(allow_restart);

    int running = 0;
    for (int i = 0; i < g_child_count; i++) {
        ChildProcess *child = &g_children[i];
        if (child->state == PROC_RUNNING && child->pid > 0) {
            if (kill(child->pid, 0) == 0 || errno == EPERM) {
                running++;
            } else if (errno == ESRCH) {
                // Surec kaybolmus; tekrar waitpid dene ve gerekirse yeniden baslat
                printf("[CHECK] '%s' (PID: %d) artik mevcut degil.\n",
                       child->name, (int)child->pid);
                child->pid = -1;
                child->state = PROC_EXITED_ERROR;
                if (allow_restart) {
                    attempt_restart(i);
                    if (child->state == PROC_RUNNING) {
                        running++;
                    }
                }
            }
        }
    }

    printf("[CHECK] Periyodik kontrol tamamlandi: %d/%d cocuk surec aktif.\n",
           running, g_child_count);
}

void shutdown_all_children(int timeout_sec) {
    if (timeout_sec <= 0) {
        timeout_sec = DEFAULT_SHUTDOWN_TIMEOUT_SEC;
    }

    stop_periodic_timer();

    // 1. Calisan tum cocuklara SIGTERM gonder
    for (int i = 0; i < g_child_count; i++) {
        ChildProcess *child = &g_children[i];
        if (child->state == PROC_RUNNING && child->pid > 0) {
            printf("[SHUTDOWN] '%s' (PID: %d) surecine SIGTERM gonderiliyor...\n",
                   child->name, (int)child->pid);
            logger_write("SHUTDOWN", "Sending SIGTERM to '%s' (PID: %d).",
                   child->name, (int)child->pid);
            if (kill(child->pid, SIGTERM) == -1 && errno != ESRCH) {
                perror("kill(SIGTERM)");
            }
        }
    }

    // 2. Timeout suresince cocuklarin kontrollu kapanmasini bekle
    struct timespec start_ts, now_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);

    while (count_running_children() > 0) {
        handle_terminated_children(0);
        if (count_running_children() == 0) {
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &now_ts);
        double elapsed = (double)(now_ts.tv_sec - start_ts.tv_sec) +
                         (double)(now_ts.tv_nsec - start_ts.tv_nsec) / 1e9;
        if (elapsed >= (double)timeout_sec) {
            break;
        }

        struct timespec sleep_ts = {0, 100000000L}; // 100 ms
        nanosleep(&sleep_ts, NULL);
    }

    // 3. Timeout sonunda hala kapanmayanlara SIGKILL gonder ve topla
    for (int i = 0; i < g_child_count; i++) {
        ChildProcess *child = &g_children[i];
        if (child->state == PROC_RUNNING && child->pid > 0) {
            pid_t pid = child->pid;
            printf("[SHUTDOWN] '%s' (PID: %d) zaman asimina ugradi, SIGKILL gonderiliyor...\n",
                   child->name, (int)pid);
            logger_write("SHUTDOWN", "'%s' (PID: %d) exceeded the shutdown timeout. Sending SIGKILL.",
                   child->name, (int)pid);
            if (kill(pid, SIGKILL) == -1 && errno != ESRCH) {
                perror("kill(SIGKILL)");
            }

            int status = 0;
            pid_t reaped;
            do {
                reaped = waitpid(pid, &status, 0);
            } while (reaped == -1 && errno == EINTR);

            if (reaped == pid) {
                if (WIFSIGNALED(status)) {
                    logger_write("EXIT", "'%s' (PID: %d) was terminated by a signal (signal: %d).",
                                 child->name, (int)pid, WTERMSIG(status));
                } else if (WIFEXITED(status)) {
                    logger_write("EXIT", "'%s' (PID: %d) exited (exit code: %d).",
                                 child->name, (int)pid, WEXITSTATUS(status));
                }
            }
            child->pid = -1;
            child->state = PROC_STOPPED;
        }
    }

    // Son temizlik (zombie kalmadigindan emin ol)
    handle_terminated_children(0);
}

int get_child_count(void) {
    return g_child_count;
}

const ChildProcess *get_child_processes(void) {
    return g_children;
}

const char *process_state_to_string(ProcessState state) {
    switch (state) {
        case PROC_STOPPED:       return "STOPPED";
        case PROC_RUNNING:       return "RUNNING";
        case PROC_EXITED_NORMAL: return "EXITED_NORMAL";
        case PROC_EXITED_ERROR:  return "EXITED_ERROR";
        case PROC_FAILED:        return "FAILED";
        default:                 return "UNKNOWN";
    }
}
