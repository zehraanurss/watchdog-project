#define _POSIX_C_SOURCE 200809L
#include "../include/signal_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static void perform_graceful_shutdown(void) {
    printf("\n[SHUTDOWN] Graceful shutdown baslatildi. Yeni surec baslatma durduruldu.\n");
    printf("[SHUTDOWN] Tum kaynaklar temizlendi. Watchdog kapaniyor.\n");
}

int main(void) {
    if (init_signal_handlers() != 0) {
        return EXIT_FAILURE;
    }

    printf("[START] Watchdog baslatildi (PID: %d)\n", getpid());
    printf("Test etmek icin Ctrl+C yapabilir veya baska terminalden 'kill -SIGUSR1 %d' gonderebilirsiniz.\n", getpid());

    sigset_t block_mask, empty_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGINT);
    sigaddset(&block_mask, SIGTERM);
    sigaddset(&block_mask, SIGUSR1);
    sigaddset(&block_mask, SIGCHLD);
    sigaddset(&block_mask, SIGALRM);
    sigemptyset(&empty_mask);

    sigprocmask(SIG_BLOCK, &block_mask, NULL);

    // Ana Kontrol Dongusu
    while (!g_shutdown_flag) {
        if (!g_shutdown_flag && !g_chld_flag && !g_usr1_flag && !g_alarm_flag) {
            sigsuspend(&empty_mask);
        }

        sigprocmask(SIG_UNBLOCK, &block_mask, NULL);

        if (g_chld_flag) {
            g_chld_flag = 0;
            if (!g_shutdown_flag) {
                printf("[EVENT] SIGCHLD alindi -> 2. Ogrenci modulu tetiklenecek.\n");
            }
        }

        if (g_usr1_flag) {
            g_usr1_flag = 0;
            printf("[EVENT] SIGUSR1 alindi -> 3. Ogrenci modulu (durum raporu) tetiklenecek.\n");
        }

        if (g_alarm_flag) {
            g_alarm_flag = 0;
            if (!g_shutdown_flag) {
                printf("[EVENT] SIGALRM alindi -> Periyodik kontrol yapilacak.\n");
            }
        }

        sigprocmask(SIG_BLOCK, &block_mask, NULL);
    }

    sigprocmask(SIG_UNBLOCK, &block_mask, NULL);

    perform_graceful_shutdown();

    return EXIT_SUCCESS;
}
