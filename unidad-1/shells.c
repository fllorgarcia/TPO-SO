/*
 * shells.c - Búsqueda de las shells del usuario que se pueden auditar
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>

#include "shells.h"
#include "procfs.h"


int is_shell(const char *name)
{
    const char *names[] = { "bash", "sh", "zsh", "fish", "dash", NULL };

    for (int i = 0; names[i]; i++)
        if (!strcmp(name, names[i]))
            return 1;
    return 0;
}


pid_t current_shell(void)
{
    /* Subimos por la jerarquía de procesos (padre, abuelo, ...)
       hasta encontrar una shell o llegar a init (PID 1) */
    pid_t pid = getppid();

    while (pid > 1) {
        char name[32];
        pid_t ppid;
        uid_t uid;

        if (!get_comm(pid, name) && is_shell(name))
            return pid;
        if (get_ppid_uid(pid, &ppid, &uid))
            break;
        pid = ppid;
    }
    return -1;
}


int find_shells(Shell *shells, pid_t exclude, uid_t uid)
{
    DIR *dir = opendir("/proc");
    if (!dir) {
        perror("/proc");
        return -1;
    }

    struct dirent *e;
    int count = 0;

    /* Cada directorio numérico de /proc es un proceso vivo */
    while ((e = readdir(dir)) && count < MAX_SHELLS) {

        if (!isdigit((unsigned char)e->d_name[0]))
            continue;

        pid_t pid = atoi(e->d_name);
        if (pid == exclude)
            continue;

        char name[32];
        pid_t ppid;
        uid_t process_uid;

        if (get_comm(pid, name) || !is_shell(name))
            continue;
        if (get_ppid_uid(pid, &ppid, &process_uid))
            continue;
        if (process_uid != uid)          /* solo shells del mismo usuario */
            continue;

        shells[count].pid = pid;
        snprintf(shells[count].name, sizeof(shells[count].name), "%s", name);

        /* fd/0 es la entrada estándar: apunta a la terminal de la shell */
        char fd[64];
        snprintf(fd, sizeof(fd), "/proc/%d/fd/0", pid);

        ssize_t len = readlink(fd, shells[count].tty,
                               sizeof(shells[count].tty) - 1);
        if (len >= 0)
            shells[count].tty[len] = 0;
        else
            strcpy(shells[count].tty, "?");

        count++;
    }

    closedir(dir);
    return count;
}
