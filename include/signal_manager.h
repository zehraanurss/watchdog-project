#ifndef SIGNAL_MANAGER_H
#define SIGNAL_MANAGER_H

#include <signal.h>

// Async-signal-safe durum bayraklari
extern volatile sig_atomic_t g_shutdown_flag;   // SIGINT veya SIGTERM
extern volatile sig_atomic_t g_usr1_flag;       // SIGUSR1 (Durum raporu)
extern volatile sig_atomic_t g_chld_flag;       // SIGCHLD (Cocuk sonlandi)
extern volatile sig_atomic_t g_alarm_flag;      // SIGALRM (Periyodik kontrol)

// Tum POSIX sinyal yakalayicilarini kuran fonksiyon
int init_signal_handlers(void);

#endif // SIGNAL_MANAGER_H
