/*
 * procfs.h - Lectura de información de procesos desde /proc
 *
 * /proc es un sistema de archivos virtual: el kernel expone ahí
 * información de cada proceso como si fueran archivos de texto.
 */
#ifndef PROCFS_H
#define PROCFS_H

#include <sys/types.h>

/* Lee el nombre del comando de un proceso (/proc/<pid>/comm).
   Devuelve 0 si pudo, -1 si el proceso no existe. */
int get_comm(pid_t pid, char *buf);

/* Lee el PID del padre y el UID del dueño (/proc/<pid>/status).
   Devuelve 0 si pudo, -1 si no. */
int get_ppid_uid(pid_t pid, pid_t *ppid, uid_t *uid);

#endif
