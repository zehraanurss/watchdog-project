#define _POSIX_C_SOURCE 200809L
#include "../include/signal_manager.h"
#include "../include/process_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static void perform_graceful_shutdown(void) {
    printf("\n[SHUTDOWN] Graceful shutdown baslatildi. Yeni surec baslatma durduruldu.\n");
    shutdown_all_children(DEFAULT_SHUTDOWN_TIMEOUT_SEC);
    printf("[SHUTDOWN] Tum kaynaklar temizlendi. Watchdog kapaniyor.\n");
}

int main(void) {
    if (init_signal_handlers() != 0) {
        return EXIT_FAILURE;
    }

    printf("[START] Watchdog baslatildi (PID: %d)\n", getpid());
    printf("Test etmek icin Ctrl+C yapabilir veya baska terminalden 'kill -SIGUSR1 %d' gonderebilirsiniz.\n", getpid());

    if (init_process_manager("./worker") != 0) {
        fprintf(stderr, "[ERROR] Process manager baslatilamadi.\n");
        return EXIT_FAILURE;
    }

    if (start_all_children() != 0) {
        fprintf(stderr, "[WARN] Bazi cocuk surecler baslatilamadi.\n");
    }

    if (start_periodic_timer(DEFAULT_CHECK_INTERVAL_SEC) != 0) {
        fprintf(stderr, "[WARN] Periyodik kontrol zamanlayicisi baslatilamadi.\n");
    }

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

        // Sinyaller blokluyken bayraklari guvenli sekilde al ve sifirla
        sig_atomic_t chld_event = g_chld_flag;
        sig_atomic_t usr1_event = g_usr1_flag;
        sig_atomic_t alarm_event = g_alarm_flag;
        g_chld_flag = 0;
        g_usr1_flag = 0;
        g_alarm_flag = 0;

        sigprocmask(SIG_UNBLOCK, &block_mask, NULL);

        if (chld_event) {
            if (!g_shutdown_flag) {
                handle_terminated_children(1);
            }
        }

        if (usr1_event) {
            printf("[EVENT] SIGUSR1 alindi -> 3. Ogrenci modulu (durum raporu) tetiklenecek.\n");
        }

        if (alarm_event) {
            if (!g_shutdown_flag) {
                perform_periodic_check(1);
            }
        }

        sigprocmask(SIG_BLOCK, &block_mask, NULL);
    }

    sigprocmask(SIG_UNBLOCK, &block_mask, NULL);

    perform_graceful_shutdown();

    return EXIT_SUCCESS;
}
