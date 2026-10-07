/*
 * logger.h - Escritura del archivo de auditoría (audit.log)
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <sys/types.h>

/* Abre el log en modo agregar. 0 si pudo, -1 si no. */
int log_open(const char *path);

/* Marca de inicio de una auditoría */
void log_start(pid_t pid);

/* Registra una syscall completa (con su resultado o error) */
void log_syscall(pid_t pid, const char *name, const char *path,
                 int is_error, long long rval);

/* Registra que un proceso creó un hijo */
void log_fork(pid_t parent, unsigned long child);

/* Registra que un proceso terminó */
void log_exit(pid_t pid);

/* Cierra el log */
void log_close(void);

#endif
