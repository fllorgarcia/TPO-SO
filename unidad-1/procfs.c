/*
 * procfs.c - Lectura de información de procesos desde /proc
 */
#include <stdio.h>
#include <string.h>

#include "procfs.h"


int get_comm(pid_t pid, char *buf)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);

    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    if (!fgets(buf, 32, f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    buf[strcspn(buf, "\n")] = 0;   /* sacamos el salto de línea */
    return 0;
}


int get_ppid_uid(pid_t pid, pid_t *ppid, uid_t *uid)
{
    char path[64], line[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    /* status tiene una línea por campo: "PPid:  123", "Uid:  1000 ..." */
    while (fgets(line, sizeof(line), f)) {
        if (!strncmp(line, "PPid:", 5))
            sscanf(line + 5, "%d", ppid);
        if (!strncmp(line, "Uid:", 4))
            sscanf(line + 4, "%u", uid);
    }

    fclose(f);
    return 0;
}
