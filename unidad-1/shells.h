/*
 * shells.h - Búsqueda de las shells del usuario que se pueden auditar
 */
#ifndef SHELLS_H
#define SHELLS_H

#include <sys/types.h>

#define MAX_SHELLS 64

typedef struct {
    pid_t pid;
    char name[32];   /* bash, zsh, ... */
    char tty[64];    /* terminal donde corre (ej. /dev/pts/1) */
} Shell;

/* 1 si el nombre corresponde a una shell conocida */
int is_shell(const char *name);

/* PID de la shell desde la que se ejecutó el auditor (-1 si no hay) */
pid_t current_shell(void);

/* Llena el vector con las shells del usuario uid, salvo exclude.
   Devuelve la cantidad encontrada, o -1 si falla. */
int find_shells(Shell *shells, pid_t exclude, uid_t uid);

#endif
