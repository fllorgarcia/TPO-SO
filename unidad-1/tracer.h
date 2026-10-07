/*
 * tracer.h - Enganche y trazado de procesos con ptrace
 */
#ifndef TRACER_H
#define TRACER_H

#include <sys/types.h>

/* Se engancha a la shell y la deja detenida lista para trazar.
   Devuelve 0 si pudo, -1 si no (ej. falta de permisos). */
int attach_shell(pid_t pid);

/* Bucle principal: atiende todas las paradas de la shell y sus
   descendientes hasta que no quede ningún proceso trazado. */
void trace(pid_t first);

#endif
