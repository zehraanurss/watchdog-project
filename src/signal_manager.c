#define _POSIX_C_SOURCE 200809L
#include "../include/signal_manager.h"
#include <stdio.h>
#include <string.h>

volatile sig_atomic_t g_shutdown_flag = 0;
volatile sig_atomic_t g_usr1_flag = 0;
volatile sig_atomic_t g_chld_flag = 0;
volatile sig_atomic_t g_alarm_flag = 0;

static void sig_shutdown_handler(int signo) {
    (void)signo;
    g_shutdown_flag = 1;
}

static void sig_usr1_handler(int signo) {
    (void)signo;
    g_usr1_flag = 1;
}

static void sig_chld_handler(int signo) {
    (void)signo;
    g_chld_flag = 1;
}

static void sig_alarm_handler(int signo) {
    (void)signo;
    g_alarm_flag = 1;
}

int init_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // 1. SIGINT ve SIGTERM (Graceful Shutdown)
    sa.sa_handler = sig_shutdown_handler;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction(SIGINT)");
        return -1;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction(SIGTERM)");
        return -1;
    }

    // 2. SIGUSR1 (Durum Raporu)
    sa.sa_handler = sig_usr1_handler;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction(SIGUSR1)");
        return -1;
    }

    // 3. SIGALRM (Periyodik kontrol icin)
    sa.sa_handler = sig_alarm_handler;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction(SIGALRM)");
        return -1;
    }

    // 4. SIGCHLD (Cocuk surec sonlanmasi - durdurulmalari yoksay)
    sa.sa_flags = SA_NOCLDSTOP;
    sa.sa_handler = sig_chld_handler;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction(SIGCHLD)");
        return -1;
    }

    return 0;
}
