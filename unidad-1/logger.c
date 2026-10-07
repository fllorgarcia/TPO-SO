/*
 * logger.c - Escritura del archivo de auditoría (audit.log)
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "logger.h"
#include "procfs.h"

/* static: solo este módulo toca el archivo */
static FILE *log_file = NULL;


int log_open(const char *path)
{
    log_file = fopen(path, "a");     /* "a": agrega sin borrar lo anterior */
    if (!log_file) {
        perror(path);
        return -1;
    }
    return 0;
}


/* Escribe "[HH:MM:SS] PID=1234 (comando) " al principio de cada línea */
static void log_prefix(pid_t pid)
{
    char hora[16], comm[32];
    time_t t = time(NULL);

    strftime(hora, sizeof(hora), "%H:%M:%S", localtime(&t));

    if (get_comm(pid, comm))
        strcpy(comm, "?");

    fprintf(log_file, "[%s] PID=%d (%s) ", hora, pid, comm);
}


void log_start(pid_t pid)
{
    fprintf(log_file, "=== Inicio de auditoría de PID=%d ===\n", pid);
    fflush(log_file);
}


void log_syscall(pid_t pid, const char *name, const char *path,
                 int is_error, long long rval)
{
    log_prefix(pid);
    fprintf(log_file, "syscall=%s", name);

    if (path && *path)
        fprintf(log_file, " path=\"%s\"", path);

    /* El kernel devuelve -errno cuando la syscall falla */
    if (is_error)
        fprintf(log_file, " ERROR errno=%lld (%s)",
                -rval, strerror((int)-rval));
    else
        fprintf(log_file, " OK return=%lld", rval);

    fprintf(log_file, "\n");
    fflush(log_file);   /* se escribe ya, por si el auditor se corta */
}


void log_fork(pid_t parent, unsigned long child)
{
    log_prefix(parent);
    fprintf(log_file, "fork -> hijo PID=%lu\n", child);
    fflush(log_file);
}


void log_exit(pid_t pid)
{
    fprintf(log_file, "PID=%d terminó\n", pid);
    fflush(log_file);
}


void log_close(void)
{
    if (log_file)
        fclose(log_file);
    log_file = NULL;
}
