
#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <linux/ptrace.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Tipos de operaciones que vamos a explicar */
enum Tipo {
    OTRO, RUTA, APERTURA, BYTES, FD,
    CREACION, EJECUCION, ESPERA
};

typedef struct {
    long numero;
    const char *nombre;
    const char *explicacion;
    enum Tipo tipo;
    int argumento_ruta;
    int solo_io;
} Definicion;

#define DEF(n, texto, tipo, ruta) \
    { SYS_##n, #n, texto, tipo, ruta, 0 }

#define IO(n, texto, tipo) \
    { SYS_##n, #n, texto, tipo, -1, 1 }

/*
 * Diccionario de syscalls.
 * Incluye las vistas en clase y sus variantes Linux.
 */
static const Definicion definiciones[] = {
#ifdef SYS_fork
    DEF(fork, "Crea un proceso hijo.", CREACION, -1),
#endif
#ifdef SYS_vfork
    DEF(vfork, "Crea un hijo con semantica especial de espera y memoria.", CREACION, -1),
#endif
#ifdef SYS_clone
    DEF(clone, "Crea una tarea; los flags determinan los recursos compartidos.", CREACION, -1),
#endif
#ifdef SYS_clone3
    DEF(clone3, "Crea una tarea usando una estructura de argumentos.", CREACION, -1),
#endif
#ifdef SYS_execve
    DEF(execve, "Reemplaza el programa actual sin crear otro proceso.", EJECUCION, 0),
#endif
#ifdef SYS_execveat
    DEF(execveat, "Ejecuta un programa mediante ruta y directorio base.", EJECUCION, 1),
#endif
#ifdef SYS_wait4
    DEF(wait4, "Espera o consulta el estado de procesos hijos.", ESPERA, -1),
#endif
#ifdef SYS_waitid
    DEF(waitid, "Consulta o espera cambios de estado de un hijo.", ESPERA, -1),
#endif
#ifdef SYS_open
    DEF(open, "Abre un archivo y devuelve un descriptor.", APERTURA, 0),
#endif
#ifdef SYS_openat
    DEF(openat, "Abre un archivo relativo a un directorio base.", APERTURA, 1),
#endif
#ifdef SYS_openat2
    DEF(openat2, "Abre un archivo con opciones adicionales de resolucion.", APERTURA, 1),
#endif
#ifdef SYS_getdents64
    DEF(getdents64, "Lee entradas de un directorio abierto.", BYTES, -1),
#endif
#ifdef SYS_mkdir
    DEF(mkdir, "Crea un directorio.", RUTA, 0),
#endif
#ifdef SYS_mkdirat
    DEF(mkdirat, "Crea un directorio relativo a un directorio base.", RUTA, 1),
#endif
#ifdef SYS_rmdir
    DEF(rmdir, "Elimina un directorio vacio.", RUTA, 0),
#endif
#ifdef SYS_unlinkat
    DEF(unlinkat, "Elimina una entrada de directorio.", RUTA, 1),
#endif
#ifdef SYS_link
    DEF(link, "Crea un enlace fisico.", RUTA, 1),
#endif
#ifdef SYS_linkat
    DEF(linkat, "Crea un enlace fisico usando directorios base.", RUTA, 3),
#endif
#ifdef SYS_chdir
    DEF(chdir, "Cambia el directorio actual del proceso.", RUTA, 0),
#endif
#ifdef SYS_fchdir
    DEF(fchdir, "Cambia el directorio actual mediante un descriptor.", FD, -1),
#endif
#ifdef SYS_chmod
    DEF(chmod, "Modifica los permisos de un archivo.", RUTA, 0),
#endif
#ifdef SYS_fchmod
    DEF(fchmod, "Modifica permisos mediante un descriptor.", FD, -1),
#endif
#ifdef SYS_fchmodat
    DEF(fchmodat, "Modifica permisos utilizando una ruta relativa.", RUTA, 1),
#endif
#ifdef SYS_fchmodat2
    DEF(fchmodat2, "Modifica permisos con opciones adicionales.", RUTA, 1),
#endif
#ifdef SYS_read
    IO(read, "Lee bytes desde un descriptor hacia memoria.", BYTES),
#endif
#ifdef SYS_write
    IO(write, "Escribe bytes desde memoria hacia un descriptor.", BYTES),
#endif
#ifdef SYS_close
    IO(close, "Cierra un descriptor del proceso.", FD),
#endif
};

#define MAX_PROCESOS 1024

typedef struct {
    pid_t pid;
    int parada_inicial;
    const Definicion *def;
    uint64_t argumentos[6];
    char ruta[256];
} Proceso;

static Proceso procesos[MAX_PROCESOS];
static FILE *log_file;
static int incluir_io = 0;

static const Definicion *buscar_syscall(long numero)
{
    size_t cantidad = sizeof(definiciones) / sizeof(definiciones[0]);

    for (size_t i = 0; i < cantidad; i++) {
        const Definicion *d = &definiciones[i];

        if (d->numero == numero && (incluir_io || !d->solo_io))
            return d;
    }

    return NULL;
}

static Proceso *buscar_proceso(pid_t pid)
{
    for (int i = 0; i < MAX_PROCESOS; i++)
        if (procesos[i].pid == pid)
            return &procesos[i];

    for (int i = 0; i < MAX_PROCESOS; i++) {
        if (procesos[i].pid == 0) {
            procesos[i].pid = pid;
            return &procesos[i];
        }
    }

    fprintf(stderr, "Demasiados procesos observados\n");
    exit(1);
}

static void eliminar_proceso(pid_t pid)
{
    for (int i = 0; i < MAX_PROCESOS; i++) {
        if (procesos[i].pid == pid) {
            memset(&procesos[i], 0, sizeof(procesos[i]));
            return;
        }
    }
}

/*
 * Lee una cadena desde la memoria de otro proceso.
 * Se utiliza para recuperar rutas de archivos.
 */
static void leer_cadena(pid_t pid, uint64_t direccion,
                         char *destino, size_t capacidad)
{
    size_t i = 0;
    destino[0] = '\0';

    if (!direccion)
        return;

    while (i + 1 < capacidad) {
        errno = 0;

        long palabra = ptrace(
            PTRACE_PEEKDATA,
            pid,
            (void *)(uintptr_t)(direccion + i),
            NULL
        );

        if (palabra == -1 && errno)
            break;

        for (size_t j = 0; j < sizeof(palabra) && i + 1 < capacidad; j++) {
            char c = ((char *)&palabra)[j];

            if (!c) {
                destino[i] = '\0';
                return;
            }

            destino[i++] = (c >= 32 && c < 127) ? c : '?';
        }
    }

    destino[i] = '\0';
}

/* Genera el log didactico */
static void registrar(const Proceso *p, long long retorno, int error)
{
    char hora[32];
    time_t ahora = time(NULL);
    struct tm tm;

    localtime_r(&ahora, &tm);
    strftime(hora, sizeof(hora), "%H:%M:%S", &tm);

    const Definicion *d = p->def;

    fprintf(log_file,
        "\n[%s] PID=%ld | SYSCALL=%s | %s (retorno=%lld)\n",
        hora, (long)p->pid, d->nombre,
        error ? "ERROR" : "OK", retorno
    );

    if (p->ruta[0])
        fprintf(log_file, "  RUTA OBSERVADA: %s\n", p->ruta);

    fprintf(log_file, "  EXPLICACION: %s\n", d->explicacion);
    fprintf(log_file, "  EN ESTA EJECUCION: ");

    if (error) {
        int codigo = (int)-retorno;

        fprintf(log_file, "La operacion fallo: %s.\n",
            codigo > 0 && codigo < 4096
                ? strerror(codigo)
                : "error desconocido");

    } else {
        switch (d->tipo) {
            case APERTURA:
                fprintf(log_file,
                    "El kernel devolvio el descriptor %lld.\n",
                    retorno);
                break;

            case BYTES:
                fprintf(log_file,
                    "Descriptor=%llu; resultado=%lld bytes.\n",
                    (unsigned long long)p->argumentos[0], retorno);
                break;

            case FD:
                fprintf(log_file,
                    "Operacion completada sobre el descriptor %llu.\n",
                    (unsigned long long)p->argumentos[0]);
                break;

            case EJECUCION:
                fprintf(log_file,
                    "Se cargo un programa en el proceso actual%s%s.\n",
                    p->ruta[0] ? ": " : "",
                    p->ruta);
                break;

            case CREACION:
                fprintf(log_file,
                    "Retorno=%lld. El tipo de tarea creada depende "
                    "de la syscall y sus flags.\n", retorno);
                break;

            case ESPERA:
                fprintf(log_file,
                    "La operacion de espera/consulta devolvio %lld.\n",
                    retorno);
                break;

            case RUTA:
                fprintf(log_file,
                    "Operacion completada%s%s.\n",
                    p->ruta[0] ? " sobre: " : "",
                    p->ruta);
                break;

            default:
                fprintf(log_file,
                    "Operacion completada. Retorno=%lld.\n",
                    retorno);
        }
    }

    fflush(log_file);
}

/*
 * PTRACE_GET_SYSCALL_INFO distingue:
 * - Entrada a una syscall: numero y argumentos.
 * - Salida de una syscall: resultado y error.
 */
static void observar_syscall(pid_t pid, Proceso *p)
{
    struct ptrace_syscall_info info;
    memset(&info, 0, sizeof(info));

    if (ptrace(PTRACE_GET_SYSCALL_INFO, pid,
               (void *)sizeof(info), &info) < 0)
        return;

    if (info.op == PTRACE_SYSCALL_INFO_ENTRY) {
        p->def = buscar_syscall((long)info.entry.nr);
        p->ruta[0] = '\0';

        memcpy(p->argumentos,
               info.entry.args,
               sizeof(p->argumentos));

        if (p->def && p->def->argumento_ruta >= 0) {
            int indice = p->def->argumento_ruta;

            leer_cadena(pid, p->argumentos[indice],
                         p->ruta, sizeof(p->ruta));
        }

    } else if (info.op == PTRACE_SYSCALL_INFO_EXIT) {
        if (p->def)
            registrar(p, info.exit.rval, info.exit.is_error);

        p->def = NULL;
    }
}

int main(int argc, char **argv)
{
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--io") != 0)) {
        fprintf(stderr, "Uso: %s [--io]\n", argv[0]);
        return 1;
    }

    incluir_io = argc == 2;

    const char *ruta_log = getenv("AUDIT_LOG");
    if (!ruta_log || !*ruta_log)
        ruta_log = "audit.log";

    log_file = fopen(ruta_log, "a");

    if (!log_file) {
        perror("audit.log");
        return 1;
    }

    fprintf(log_file, "\n=== INICIO DE AUDITORIA UNIDAD 1 ===\n");
    fflush(log_file);

    /*
     * El auditor crea su propia shell.
     * El hijo pide ser observado mediante PTRACE_TRACEME.
     */
    pid_t hijo = fork();

    if (hijo < 0) {
        perror("fork");
        return 1;
    }

    if (hijo == 0) {
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
            perror("PTRACE_TRACEME");
            _exit(1);
        }

        raise(SIGSTOP);

        setenv("PS1", "auditor-u1> ", 1);

        execlp("bash", "bash",
               "--noprofile", "--norc", "-i",
               (char *)NULL);

        perror("bash");
        _exit(127);
    }

    /* El auditor no debe cerrarse por Ctrl+C dirigido a la shell */
    signal(SIGINT, SIG_IGN);

    int estado;

    if (waitpid(hijo, &estado, 0) < 0 || !WIFSTOPPED(estado)) {
        fprintf(stderr, "No se pudo iniciar la shell observada\n");
        return 1;
    }

    /*
     * TRACESYSGOOD: distingue paradas por syscalls.
     * TRACEFORK/VFORK/CLONE: sigue nuevas tareas.
     * TRACEEXEC: observa reemplazos del programa.
     */
    long opciones =
        PTRACE_O_TRACESYSGOOD |
        PTRACE_O_TRACEFORK |
        PTRACE_O_TRACEVFORK |
        PTRACE_O_TRACECLONE |
        PTRACE_O_TRACEEXEC;

    if (ptrace(PTRACE_SETOPTIONS, hijo, NULL,
               (void *)opciones) < 0) {
        perror("PTRACE_SETOPTIONS");
        kill(hijo, SIGKILL);
        return 1;
    }

    buscar_proceso(hijo);

    if (ptrace(PTRACE_SYSCALL, hijo, NULL, NULL) < 0) {
        perror("PTRACE_SYSCALL");
        return 1;
    }

    printf("Auditor iniciado. Log: %s\n", ruta_log);
    printf("Ejecuta comandos y escribe exit para terminar.\n");
    fflush(stdout);

    /*
     * El proceso principal espera eventos de todos
     * los procesos e hilos observados.
     */
    while (1) {
        pid_t pid = waitpid(-1, &estado, __WALL);

        if (pid < 0) {
            if (errno == EINTR)
                continue;
            if (errno == ECHILD)
                break;

            perror("waitpid");
            break;
        }

        if (WIFEXITED(estado) || WIFSIGNALED(estado)) {
            eliminar_proceso(pid);
            continue;
        }

        if (!WIFSTOPPED(estado))
            continue;

        Proceso *p = buscar_proceso(pid);
        int sig = WSTOPSIG(estado);
        int entregar = 0;
        unsigned evento = (unsigned)estado >> 16;

        if (sig == (SIGTRAP | 0x80)) {
            observar_syscall(pid, p);

        } else if (sig == SIGTRAP && evento) {
            if (evento == PTRACE_EVENT_FORK ||
                evento == PTRACE_EVENT_VFORK ||
                evento == PTRACE_EVENT_CLONE) {

                unsigned long nuevo_pid = 0;

                if (ptrace(PTRACE_GETEVENTMSG, pid,
                           NULL, &nuevo_pid) == 0) {
                    buscar_proceso((pid_t)nuevo_pid)
                        ->parada_inicial = 1;
                }
            }

        } else if (p->parada_inicial && sig == SIGSTOP) {
            p->parada_inicial = 0;

        } else if (sig != SIGTRAP) {
            entregar = sig;
        }

        /* Reanuda el proceso hasta la siguiente syscall */
        if (ptrace(PTRACE_SYSCALL, pid, NULL,
                   (void *)(intptr_t)entregar) < 0 &&
            errno != ESRCH) {
            perror("PTRACE_SYSCALL");
        }
    }

    fprintf(log_file, "=== FIN DE AUDITORIA ===\n");
    fclose(log_file);

    return 0;
}
