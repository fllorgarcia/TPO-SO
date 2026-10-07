/*
 * syscalls.c - Qué llamadas al sistema se auditan y cómo leerlas
 *
 * Cada syscall se identifica con un número (SYS_xxx). El kernel lo usa
 * como índice en su tabla de manejadores de llamadas al sistema.
 */
#include <string.h>
#include <sys/syscall.h>

#include "syscalls.h"


const char *sys_name(long nr)
{
    switch (nr) {
        /* administración de procesos */
        case SYS_execve:    return "execve";
        /* administración de archivos */
        case SYS_openat:    return "openat";
        /* administración de directorios */
        case SYS_unlinkat:  return "unlinkat";
        case SYS_mkdirat:   return "mkdirat";
        case SYS_renameat:  return "renameat";
#ifdef SYS_renameat2
        case SYS_renameat2: return "renameat2";   /* la que usa mv */
#endif
        /* permisos y dueños */
        case SYS_fchmodat:  return "fchmodat";
        case SYS_fchownat:  return "fchownat";
        /* red */
        case SYS_connect:   return "connect";
        /* syscalls "clásicas" (sin *at), aún usadas en x86_64 */
#ifdef SYS_mkdir
        case SYS_mkdir:     return "mkdir";
#endif
#ifdef SYS_rmdir
        case SYS_rmdir:     return "rmdir";
#endif
#ifdef SYS_unlink
        case SYS_unlink:    return "unlink";
#endif
#ifdef SYS_rename
        case SYS_rename:    return "rename";
#endif
#ifdef SYS_chmod
        case SYS_chmod:     return "chmod";
#endif
        default:            return NULL;
    }
}


int path_arg(long nr)
{
    switch (nr) {
        /* la ruta es el 1er argumento: execve("/usr/bin/ls", ...) */
        case SYS_execve:
#ifdef SYS_mkdir
        case SYS_mkdir:
#endif
#ifdef SYS_rmdir
        case SYS_rmdir:
#endif
#ifdef SYS_unlink
        case SYS_unlink:
#endif
#ifdef SYS_rename
        case SYS_rename:
#endif
#ifdef SYS_chmod
        case SYS_chmod:
#endif
            return 0;

        /* las *at reciben primero un directorio (dirfd) y después
           la ruta: openat(dirfd, "archivo", ...) */
        case SYS_openat:
        case SYS_unlinkat:
        case SYS_mkdirat:
        case SYS_renameat:
#ifdef SYS_renameat2
        case SYS_renameat2:
#endif
        case SYS_fchmodat:
        case SYS_fchownat:
            return 1;

        default:
            return -1;     /* connect: no recibe una ruta */
    }
}


int is_noise(long nr, const char *path)
{
    if (!FILTRAR_RUIDO || nr != SYS_openat)
        return 0;

    /* Cada comando abre bibliotecas y archivos internos al arrancar */
    const char *prefixes[] = {
        "/lib", "/usr/lib", "/etc/ld.so", "/usr/share/locale",
        "/proc", "/sys", "/dev/tty", NULL
    };

    for (int i = 0; prefixes[i]; i++)
        if (!strncmp(path, prefixes[i], strlen(prefixes[i])))
            return 1;
    return 0;
}
