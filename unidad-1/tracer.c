/*
 * tracer.c - Enganche y trazado de procesos con ptrace
 *
 * ptrace permite que un proceso (el tracer, nuestro auditor) observe
 * y controle a otro (el tracee, la shell). Con PTRACE_SYSCALL el kernel
 * detiene al tracee cada vez que ENTRA y cada vez que SALE de una
 * llamada al sistema, y nos avisa por waitpid.
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <sys/ptrace.h>
#include <sys/wait.h>
#include <linux/ptrace.h>

#include "tracer.h"
#include "procs.h"
#include "syscalls.h"
#include "logger.h"


int attach_shell(pid_t pid)
{
    /* Opciones:
       TRACESYSGOOD -> distinguir paradas de syscall (SIGTRAP|0x80)
       TRACEFORK/VFORK/CLONE -> trazar también a los hijos
       TRACEEXEC -> avisar cuando un proceso hace execve
       (sin EXITKILL: si el auditor muere, la shell sigue viva) */
    long opts =
        PTRACE_O_TRACESYSGOOD |
        PTRACE_O_TRACEFORK    |
        PTRACE_O_TRACEVFORK   |
        PTRACE_O_TRACECLONE   |
        PTRACE_O_TRACEEXEC;

    /* SEIZE: engancharse sin detener ni mandar señales */
    if (ptrace(PTRACE_SEIZE, pid, NULL, (void *)opts) < 0) {
        perror("PTRACE_SEIZE");
        return -1;
    }

    /* Detenemos la shell una vez para empezar a trazarla */
    ptrace(PTRACE_INTERRUPT, pid, NULL, NULL);

    int status;
    waitpid(pid, &status, __WALL);

    get_proc(pid)->started = 1;
    return 0;
}


/* Copia un string de la memoria del proceso trazado a buf.
   Cada proceso tiene su propio espacio de direcciones: no podemos
   leer su puntero directamente, hay que pedírselo al kernel. */
static void read_string(pid_t pid, unsigned long long addr, char *buf)
{
    size_t pos = 0;

    while (pos < MAX_PATH - sizeof(long)) {

        /* PEEKDATA lee de a una palabra (8 bytes en x86_64) */
        errno = 0;
        long word = ptrace(PTRACE_PEEKDATA, pid, (void *)(addr + pos), NULL);

        if (word == -1 && errno)
            break;

        memcpy(buf + pos, &word, sizeof(word));

        if (memchr(&word, 0, sizeof(word)))   /* llegamos al '\0' */
            return;

        pos += sizeof(word);
    }

    buf[pos < MAX_PATH ? pos : MAX_PATH - 1] = 0;
}


/* Atiende una parada de syscall: puede ser ENTRADA o SALIDA */
static void handle_syscall_stop(pid_t pid)
{
    struct ptrace_syscall_info info = {0};

    if (ptrace(PTRACE_GET_SYSCALL_INFO, pid, sizeof(info), &info) < 0)
        return;

    Proc *p = get_proc(pid);

    if (info.op == PTRACE_SYSCALL_INFO_ENTRY) {

        /* ENTRADA al kernel: guardamos qué syscall es y su ruta */
        const char *name = sys_name(info.entry.nr);
        if (!name)
            return;                    /* no es de las que auditamos */

        p->syscall = info.entry.nr;
        snprintf(p->name, sizeof(p->name), "%s", name);
        p->path[0] = 0;

        int arg = path_arg(info.entry.nr);
        if (arg >= 0 && info.entry.args[arg])
            read_string(pid, info.entry.args[arg], p->path);

    } else if (info.op == PTRACE_SYSCALL_INFO_EXIT && p->syscall >= 0) {

        /* SALIDA del kernel: ya tenemos el resultado o el error */
        if (!is_noise(p->syscall, p->path))
            log_syscall(pid, p->name, p->path,
                        info.exit.is_error, (long long)info.exit.rval);

        p->syscall = -1;
    }
}


void trace(pid_t first)
{
    ptrace(PTRACE_SYSCALL, first, NULL, NULL);

    while (1) {

        /* Esperamos que CUALQUIER proceso trazado se detenga */
        int status;
        pid_t pid = waitpid(-1, &status, __WALL);

        if (pid < 0)
            break;                     /* no quedan procesos trazados */

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            log_exit(pid);
            continue;
        }

        if (!WIFSTOPPED(status))
            continue;

        int sig = WSTOPSIG(status);
        unsigned event = (unsigned)status >> 16;
        int inject = 0;                /* señal a reenviar al proceso */
        Proc *p = get_proc(pid);

        /* Primera parada de un hijo recién creado: solo lo arrancamos */
        if (!p->started) {
            p->started = 1;
            if (event == PTRACE_EVENT_STOP) {
                ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
                continue;
            }
        }

        if (sig == (SIGTRAP | 0x80)) {

            /* Caso 1: parada de syscall (entrada o salida) */
            handle_syscall_stop(pid);

        } else if (event == PTRACE_EVENT_FORK  ||
                   event == PTRACE_EVENT_VFORK ||
                   event == PTRACE_EVENT_CLONE) {

            /* Caso 2: el proceso creó un hijo */
            unsigned long child;
            if (!ptrace(PTRACE_GETEVENTMSG, pid, NULL, &child)) {
                get_proc((pid_t)child);
                log_fork(pid, child);
            }

        } else if (event == PTRACE_EVENT_STOP) {

            /* Caso 3: group-stop (Ctrl+Z, SIGSTOP...). El proceso debe
               quedar detenido: LISTEN lo deja parado sin soltarlo */
            if (sig == SIGSTOP || sig == SIGTSTP ||
                sig == SIGTTIN || sig == SIGTTOU) {
                ptrace(PTRACE_LISTEN, pid, NULL, NULL);
                continue;
            }

        } else if (event == 0 && sig != SIGTRAP) {

            /* Caso 4: señal real (Ctrl+C, SIGCHLD...): hay que
               reenviarla, si no el proceso nunca la recibe */
            inject = sig;
        }

        /* Dejamos seguir al proceso hasta su próxima syscall */
        if (ptrace(PTRACE_SYSCALL, pid, NULL, (void *)(long)inject) < 0 &&
            errno != ESRCH)
            perror("ptrace");
    }
}
