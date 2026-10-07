/*
 * auditor.c - Auditor de syscalls (MVP)
 *
 * Abre una shell bash bajo observación y registra en audit.log
 * algunas llamadas al sistema relevantes.
 *
 * Compilar:
 *   gcc -Wall auditor.c -o auditor
 *
 * Ejecutar:
 *   ./auditor
 *
 * Requiere:
 *   Linux x86_64, kernel >= 5.3
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <linux/ptrace.h>

#define MAX_PROCS 128
#define MAX_PATH 256

typedef struct {
    pid_t pid;
    long nr;
    char path[MAX_PATH];
} EnCurso;

EnCurso tabla[MAX_PROCS];
FILE *log_file;

EnCurso *buscar(pid_t pid)
{
    EnCurso *libre = NULL;

    for (int i = 0; i < MAX_PROCS; i++) {
        if (tabla[i].pid == pid)
            return &tabla[i];

        if (tabla[i].pid == 0 && libre == NULL)
            libre = &tabla[i];
    }

    if (libre != NULL) {
        libre->pid = pid;
        libre->nr = -1;
        libre->path[0] = '\0';
    }

    return libre;
}

const char *nombre_syscall(long nr)
{
    switch (nr) {
        case SYS_execve:
            return "execve";

        case SYS_clone:
            return "fork";

        case SYS_openat:
            return "crear";

        case SYS_unlinkat:
            return "borrar";

        case SYS_mkdir:
            return "mkdir";

        case SYS_rmdir:
            return "rmdir";

        case SYS_renameat2:
            return "renombrar";

        case SYS_fchmodat:
            return "chmod";

        default:
            return NULL;
    }
}

int arg_ruta(long nr)
{
    switch (nr) {
        case SYS_execve:
        case SYS_mkdir:
        case SYS_rmdir:
            return 0;

        case SYS_openat:
        case SYS_unlinkat:
        case SYS_renameat2:
        case SYS_fchmodat:
            return 1;

        default:
            return -1;
    }
}

void leer_ruta(pid_t pid, unsigned long direccion, char *destino)
{
    destino[0] = '\0';

    if (direccion == 0)
        return;

    for (int i = 0; i < MAX_PATH - (int)sizeof(long); i += sizeof(long)) {

        errno = 0;

        long palabra = ptrace(
            PTRACE_PEEKDATA,
            pid,
            (void *)(direccion + i),
            NULL
        );

        if (palabra == -1 && errno != 0) {
            destino[0] = '\0';
            return;
        }

        memcpy(destino + i, &palabra, sizeof(long));

        if (memchr(&palabra, '\0', sizeof(long)) != NULL)
            return;
    }

    destino[MAX_PATH - 1] = '\0';
}

void atender_syscall(pid_t pid)
{
    struct ptrace_syscall_info info;

    memset(&info, 0, sizeof(info));

    long resultado = ptrace(
        PTRACE_GET_SYSCALL_INFO,
        pid,
        sizeof(info),
        &info
    );

    if (resultado == -1) {
        perror("PTRACE_GET_SYSCALL_INFO");
        return;
    }

    EnCurso *p = buscar(pid);

    if (p == NULL)
        return;

    if (info.op == PTRACE_SYSCALL_INFO_ENTRY) {

        long nr = info.entry.nr;

        if (nombre_syscall(nr) == NULL)
            return;

        if (nr == SYS_openat) {
            int flags = (int)info.entry.args[2];

            if (!(flags & O_CREAT))
                return;
        }

        p->nr = nr;
        p->path[0] = '\0';

        int arg = arg_ruta(nr);

        if (arg >= 0) {
            leer_ruta(
                pid,
                info.entry.args[arg],
                p->path
            );
        }

    } else if (
        info.op == PTRACE_SYSCALL_INFO_EXIT &&
        p->nr != -1
    ) {

        char hora[16];

        time_t ahora = time(NULL);

        struct tm *tm_info = localtime(&ahora);

        if (tm_info != NULL)
            strftime(hora, sizeof(hora), "%H:%M:%S", tm_info);
        else
            strcpy(hora, "--:--:--");

        fprintf(
            log_file,
            "[%s] PID=%d %s(",
            hora,
            pid,
            nombre_syscall(p->nr)
        );

        if (p->path[0] != '\0')
            fprintf(log_file, "\"%s\"", p->path);

        fprintf(log_file, ")");

        if (info.exit.is_error) {

            long error = -info.exit.rval;

            fprintf(
                log_file,
                " = ERROR %ld (%s)\n",
                error,
                strerror(error)
            );

        } else {

            fprintf(
                log_file,
                " = %lld\n",
                (long long)info.exit.rval
            );
        }

        fflush(log_file);

        p->nr = -1;
    }
}

int main(void)
{
    log_file = fopen("audit.log", "a");

    if (log_file == NULL) {
        perror("audit.log");
        return 1;
    }

    setvbuf(log_file, NULL, _IOLBF, 0);

    pid_t shell = fork();

    if (shell < 0) {
        perror("fork");
        fclose(log_file);
        return 1;
    }

    if (shell == 0) {

        if (ptrace(
            PTRACE_TRACEME,
            0,
            NULL,
            NULL
        ) == -1) {
            perror("PTRACE_TRACEME");
            exit(1);
        }

        raise(SIGSTOP);

        execlp("bash", "bash", NULL);

        perror("execlp");
        exit(1);
    }

    int status;

    if (waitpid(shell, &status, 0) == -1) {
        perror("waitpid inicial");
        fclose(log_file);
        return 1;
    }

    long opciones =
        PTRACE_O_TRACESYSGOOD |
        PTRACE_O_TRACEFORK |
        PTRACE_O_TRACEVFORK |
        PTRACE_O_TRACECLONE;

    if (ptrace(
        PTRACE_SETOPTIONS,
        shell,
        NULL,
        (void *)opciones
    ) == -1) {
        perror("PTRACE_SETOPTIONS");
        fclose(log_file);
        return 1;
    }

    printf("=== Auditoria iniciada ===\n");
    printf("Escribi comandos normalmente.\n");
    printf("Escribi exit para terminar.\n\n");

    fprintf(
        log_file,
        "=== Inicio de auditoria ===\n"
    );

    if (ptrace(
        PTRACE_SYSCALL,
        shell,
        NULL,
        NULL
    ) == -1) {
        perror("PTRACE_SYSCALL inicial");
        fclose(log_file);
        return 1;
    }

    while (1) {

        pid_t pid = waitpid(
            -1,
            &status,
            __WALL
        );

        if (pid < 0) {

            if (errno == ECHILD)
                break;

            perror("waitpid");
            break;
        }

        if (
            WIFEXITED(status) ||
            WIFSIGNALED(status)
        ) {

            for (int i = 0; i < MAX_PROCS; i++) {
                if (tabla[i].pid == pid) {
                    tabla[i].pid = 0;
                    tabla[i].nr = -1;
                    tabla[i].path[0] = '\0';
                    break;
                }
            }

            continue;
        }

        if (!WIFSTOPPED(status))
            continue;

        int sig = WSTOPSIG(status);

        int evento = status >> 16;

        if (sig == (SIGTRAP | 0x80)) {

            atender_syscall(pid);

        } else if (
            sig == SIGTRAP &&
            evento != 0
        ) {

            unsigned long nuevo_pid = 0;

            if (
                evento == PTRACE_EVENT_FORK ||
                evento == PTRACE_EVENT_VFORK ||
                evento == PTRACE_EVENT_CLONE
            ) {

                if (ptrace(
                    PTRACE_GETEVENTMSG,
                    pid,
                    NULL,
                    &nuevo_pid
                ) == -1) {
                    perror("PTRACE_GETEVENTMSG");
                } else {
                    buscar((pid_t)nuevo_pid);
                }
            }
        }

        int reenviar = 0;

        if (
            sig != SIGTRAP &&
            sig != (SIGTRAP | 0x80) &&
            sig != SIGSTOP &&
            sig != SIGTSTP &&
            sig != SIGTTIN &&
            sig != SIGTTOU
        ) {
            reenviar = sig;
        }

        if (ptrace(
            PTRACE_SYSCALL,
            pid,
            NULL,
            (void *)(long)reenviar
        ) == -1) {

            if (errno != ESRCH)
                perror("PTRACE_SYSCALL");
        }
    }

    fprintf(
        log_file,
        "=== Fin de auditoria ===\n"
    );

    fclose(log_file);

    printf("\n=== Auditoria finalizada ===\n");
    printf("Ver audit.log\n");

    return 0;
}