/*
 * procs.h - Estado de cada proceso que estamos trazando
 *
 * Una syscall se ve en DOS paradas (entrada y salida). En la entrada
 * guardamos qué syscall es y su ruta; en la salida leemos el resultado.
 * Esta estructura recuerda los datos entre una parada y la otra.
 */
#ifndef PROCS_H
#define PROCS_H

#include <sys/types.h>

#define MAX_PATH 512

typedef struct Proc {
    pid_t pid;
    long syscall;          /* número de syscall en curso (-1 = ninguna) */
    int started;           /* 0 = todavía no tuvo su primera parada */
    char name[32];         /* nombre de la syscall en curso */
    char path[MAX_PATH];   /* ruta que recibió la syscall */
    struct Proc *next;
} Proc;

/* Busca el Proc de un PID; si no existe lo crea */
Proc *get_proc(pid_t pid);

#endif
