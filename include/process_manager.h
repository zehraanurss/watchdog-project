#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <sys/types.h>

#define MAX_CHILDREN 8
#define MAX_RESTARTS 3
#define DEFAULT_CHECK_INTERVAL_SEC 5
#define DEFAULT_SHUTDOWN_TIMEOUT_SEC 5

typedef enum {
    PROC_STOPPED = 0,
    PROC_RUNNING,
    PROC_EXITED_NORMAL,
    PROC_EXITED_ERROR,
    PROC_FAILED
} ProcessState;

typedef struct {
    int id;
    char name[64];
    char exec_path[256];
    char mode[32];
    pid_t pid;
    ProcessState state;
    int restart_count;
    int last_exit_code;
    int last_term_signal;
} ChildProcess;

// Varsayilan cocuk surecleri (en az 2 adet) yapilandirir
int init_process_manager(const char *worker_path);

// Yeni bir cocuk surec tanimi ekler
int add_child_process(const char *name, const char *exec_path, const char *mode);

// Belirtilen indisteki cocuk sureci fork() + execv() ile baslatir
int start_child_process(int index);

// Kayitli tum cocuk surecleri baslatir
int start_all_children(void);

// SIGCHLD sonrasi sonlanan cocuklari waitpid(..., WNOHANG) ile toplar ve gerekirse yeniden baslatir
void handle_terminated_children(int allow_restart);

// setitimer() ile periyodik SIGALRM zamanlayicisini baslatir/durdurur
int start_periodic_timer(int interval_sec);
void stop_periodic_timer(void);

// SIGALRM geldiginde calisan periyodik saglik kontrolu
void perform_periodic_check(int allow_restart);

// Graceful shutdown: SIGTERM -> timeout -> SIGKILL -> waitpid
void shutdown_all_children(int timeout_sec);

// 3. Ogrenci (Logging / SIGUSR1 raporlama) modulu icin salt-okunur erisim fonksiyonlari
int get_child_count(void);
const ChildProcess *get_child_processes(void);
const char *process_state_to_string(ProcessState state);

#endif // PROCESS_MANAGER_H
