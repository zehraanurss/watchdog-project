#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;

static void worker_sig_handler(int signo) {
    (void)signo;
    g_stop = 1;
}

int main(int argc, char *argv[]) {
    const char *name = (argc > 1) ? argv[1] : "worker";
    const char *mode = (argc > 2) ? argv[2] : "loop";

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = worker_sig_handler;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    printf("[WORKER:%s] Baslatildi (PID: %d, mode: %s)\n", name, (int)getpid(), mode);
    fflush(stdout);

    if (strncmp(mode, "normal", 6) == 0) {
        int duration = 5;
        if (mode[6] == ':' && mode[7] != '\0') {
            int parsed = atoi(mode + 7);
            if (parsed > 0) {
                duration = parsed;
            }
        }
        for (int i = 0; i < duration && !g_stop; i++) {
            sleep(1);
        }
        printf("[WORKER:%s] Gorev tamamlandi, normal cikis yapiliyor (PID: %d).\n",
               name, (int)getpid());
        fflush(stdout);
        return EXIT_SUCCESS;
    }

    if (strncmp(mode, "crash", 5) == 0) {
        int duration = 3;
        if (mode[5] == ':' && mode[6] != '\0') {
            int parsed = atoi(mode + 6);
            if (parsed > 0) {
                duration = parsed;
            }
        }
        for (int i = 0; i < duration && !g_stop; i++) {
            sleep(1);
        }
        if (!g_stop) {
            fprintf(stderr, "[WORKER:%s] Beklenmedik hata ile cikis yapiliyor (PID: %d).\n",
                    name, (int)getpid());
            fflush(stderr);
            return EXIT_FAILURE;
        }
    }

    // Varsayilan "loop" modu: SIGTERM/SIGINT gelene kadar calisir
    while (!g_stop) {
        sleep(1);
    }

    printf("[WORKER:%s] Kapanma sinyali alindi, temiz cikis yapiliyor (PID: %d).\n",
           name, (int)getpid());
    fflush(stdout);
    return EXIT_SUCCESS;
}
