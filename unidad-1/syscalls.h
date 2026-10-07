/*
 * syscalls.h - Qué llamadas al sistema se auditan y cómo leerlas
 */
#ifndef SYSCALLS_H
#define SYSCALLS_H

/* 1 = no registrar los openat de bibliotecas y archivos del sistema */
#define FILTRAR_RUIDO 1

/* Nombre de la syscall si se audita, NULL si se ignora */
const char *sys_name(long nr);

/* Índice del argumento que contiene la ruta (-1 = no tiene) */
int path_arg(long nr);

/* 1 si es un openat "de ruido" que no conviene registrar */
int is_noise(long nr, const char *path);

#endif
