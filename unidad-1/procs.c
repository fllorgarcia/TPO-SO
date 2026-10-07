/*
 * procs.c - Estado de cada proceso que estamos trazando
 */
#include <stdio.h>
#include <stdlib.h>

#include "procs.h"

/* Lista enlazada con un nodo por proceso trazado */
static Proc *procs = NULL;


Proc *get_proc(pid_t pid)
{
    for (Proc *p = procs; p; p = p->next)
        if (p->pid == pid)
            return p;

    /* No estaba: lo agregamos al principio de la lista */
    Proc *p = calloc(1, sizeof(Proc));
    if (!p) {
        perror("calloc");
        exit(1);
    }

    p->pid = pid;
    p->syscall = -1;
    p->next = procs;
    procs = p;
    return p;
}
