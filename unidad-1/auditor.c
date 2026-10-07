/*
 * auditor.c - Auditor de syscalls (versión MVP)
 *
 * Abre una shell (bash) bajo observación. Mientras el usuario trabaja
 * en ella, registra en audit.log las llamadas al sistema relevantes
 * que hacen la shell y todos los comandos que se ejecutan desde ella.
 * Cuando el usuario escribe "exit", la auditoría termina.
 *
 * Compilar:  gcc -Wall -o auditor_mvp auditor.c
 * Ejecutar:  ./auditor         (no necesita sudo)
 * Requiere:  Linux x86_64, kernel >= 5.3
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
#define MAX_PATH  256


/* ================================================================
 * 1. ESTADO
 *
 * Cada syscall produce DOS paradas: entrada y salida. En la entrada
 * guardamos qué syscall es y su ruta; en la salida escribimos el log
 * con el resultado. Esta tabla recuerda los datos entre ambas.
 * ================================================================ */

typedef struct {
    pid_t pid;              /* 0 = lugar libre */
    long  nr;               /* syscall en curso (-1 = ninguna) */
    char  path[MAX_PATH];
} EnCurso;

EnCurso tabla[MAX_PROCS];
FILE *log_file;


/* Devuelve el lugar de la tabla para ese PID (o uno libre) */
EnCurso *buscar(pid_t pid)
{
    EnCurso *libre = NULL;

    for (int i = 0; i < MAX_PROCS; i++) {
        if (tabla[i].pid == pid)
            return &tabla[i];
        if (tabla[i].pid == 0 && !libre)
            libre = &tabla[i];
    }

    if (libre) {
        libre->pid = pid;
        libre->nr = -1;
    }
    return libre;
}


/* ================================================================
 * 2. QUÉ SYSCALLS SE AUDITAN
 * ================================================================ */

/* Nombre de la syscall, o NULL si no nos interesa */
const char *nombre_syscall(long nr)
{
    switch (nr) {
        case SYS_execve:    return "execve";     /* procesos    */
        case SYS_clone:     return "fork";       /* procesos    */
        case SYS_openat:    return "crear";      /* archivos    */
        case SYS_unlinkat:  return "borrar";     /* archivos    */
        case SYS_mkdir:     return "mkdir";      /* directorios */
        case SYS_rmdir:     return "rmdir";      /* directorios */
        case SYS_renameat2: return "renombrar";  /* directorios */
        case SYS_fchmodat:  return "chmod";      /* permisos    */
        default:            return NULL;
    }
}

/* Posición del argumento que contiene la ruta (-1 = no tiene) */
int arg_ruta(long nr)
{
    switch (nr) {
        case SYS_execve:
        case SYS_mkdir:
        case SYS_rmdir:
            return 0;            /* mkdir("dir", ...)            */
        case SYS_openat:
        case SYS_unlinkat:
        case SYS_renameat2:
        case SYS_fchmodat:
            return 1;            /* openat(dirfd, "archivo", ...) */
        default:
            return -1;
    }
}


/* ================================================================
 * 3. LEER LA RUTA DESDE LA MEMORIA DEL OTRO PROCESO
 *
 * El argumento es un puntero dentro del espacio de direcciones de la
 * shell, no del nuestro. Se lo pedimos al kernel de a 8 bytes.
 * ================================================================ */

void leer_ruta(pid_t pid, unsigned long direccion, char *destino)
{
    for (int i = 0; i < MAX_PATH - 8; i += 8) {

        long palabra = ptrace(PTRACE_PEEKDATA, pid, direccion + i, NULL);
        memcpy(destino + i, &palabra, 8);

        if (memchr(&palabra, 0, 8))      /* encontramos el '\0' */
            return;
    }
    destino[MAX_PATH - 1] = 0;
}


/* ================================================================
 * 4. ATENDER UNA PARADA DE SYSCALL (entrada o salida)
 * ================================================================ */

void atender_syscall(pid_t pid)
{
    struct ptrace_syscall_info info;
    ptrace(PTRACE_GET_SYSCALL_INFO, pid, sizeof(info), &info);

    EnCurso *p = buscar(pid);
    if (!p)
        return;

    if (info.op == PTRACE_SYSCALL_INFO_ENTRY) {

        /* --- ENTRADA AL KERNEL: ¿nos interesa? guardamos los datos --- */
        long nr = info.entry.nr;
        if (!nombre_syscall(nr))
            return;

        /* De openat solo nos interesa cuando CREA un archivo */
        if (nr == SYS_openat && !(info.entry.args[2] & O_CREAT))
            return;

        p->nr = nr;
        p->path[0] = 0;

        int arg = arg_ruta(nr);
        if (arg >= 0)
            leer_ruta(pid, info.entry.args[arg], p->path);

    } else if (info.op == PTRACE_SYSCALL_INFO_EXIT && p->nr != -1) {

        /* --- SALIDA DEL KERNEL: ya está el resultado, escribimos --- */
        char hora[16];
        time_t t = time(NULL);
        strftime(hora, sizeof(hora), "%H:%M:%S", localtime(&t));

        fprintf(log_file, "[%s] PID=%d %s(", hora, pid, nombre_syscall(p->nr));
        if (p->path[0])
            fprintf(log_file, "\"%s\"", p->path);
        fprintf(log_file, ")");

        if (info.exit.is_error)          /* el kernel devuelve -errno */
            fprintf(log_file, " = ERROR %lld (%s)\n",
                    -info.exit.rval, strerror(-info.exit.rval));
        else
            fprintf(log_file, " = %lld\n", info.exit.rval);

        p->nr = -1;
    }
}


/* ================================================================
 * 5. MAIN
 * ================================================================ */

int main(void)
{
    log_file = fopen("audit.log", "a");
    if (!log_file) {
        perror("audit.log");
        return 1;
    }
    setvbuf(log_file, NULL, _IOLBF, 0);  /* escribir línea por línea */

    /* --- 5.1 Creamos el proceso hijo que va a ser la shell --- */
    pid_t shell = fork();

    if (shell == 0) {
        /* HIJO: pide ser trazado por su padre y se convierte en bash */
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        raise(SIGSTOP);                  /* espera a que el padre esté listo */
        execlp("bash", "bash", NULL);
        perror("execlp");
        exit(1);
    }

    /* --- 5.2 PADRE: configuramos el trazado --- */
    waitpid(shell, NULL, 0);             /* esperamos el SIGSTOP del hijo */

    ptrace(PTRACE_SETOPTIONS, shell, NULL,
           PTRACE_O_TRACESYSGOOD |       /* marcar paradas de syscall   */
           PTRACE_O_TRACEFORK    |       /* trazar también a los hijos  */
           PTRACE_O_TRACEVFORK   |
           PTRACE_O_TRACECLONE);

    printf("=== Auditoría iniciada (escribí 'exit' para terminar) ===\n");
    fprintf(log_file, "=== Inicio de auditoría ===\n");

    ptrace(PTRACE_SYSCALL, shell, NULL, NULL);

    /* --- 5.3 Bucle: esperar paradas, atenderlas, dejar seguir --- */
    while (1) {
        int status;
        pid_t pid = waitpid(-1, &status, __WALL);

        if (pid < 0)
            break;                       /* no quedan procesos trazados */

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            EnCurso *p = buscar(pid);
            if (p) p->pid = 0;           /* liberamos su lugar */
            continue;
        }

        int sig = WSTOPSIG(status);
        int reenviar = 0;

        if (sig == (SIGTRAP | 0x80)) {
            /* parada de syscall */
            atender_syscall(pid);

        } else if ((status >> 16) == 0 &&
                   sig != SIGTRAP && sig != SIGSTOP &&
                   sig != SIGTSTP && sig != SIGTTIN && sig != SIGTTOU) {
            /* señal real (Ctrl+C, SIGCHLD...): se la devolvemos */
            reenviar = sig;
        }

        ptrace(PTRACE_SYSCALL, pid, NULL, reenviar);
    }

    fprintf(log_file, "=== Fin de auditoría ===\n");
    fclose(log_file);
    printf("=== Auditoría finalizada. Ver audit.log ===\n");
    return 0;
}