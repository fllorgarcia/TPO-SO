/*
 * main.c - Auditor de syscalls de una shell
 *
 * Lista las shells del usuario, deja elegir una, se engancha con
 * ptrace y registra en audit.log las syscalls que hacen esa shell
 * y todos los comandos que se ejecuten en ella.
 *
 * Compilar:  make
 * Ejecutar:  sudo ./auditor
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "shells.h"
#include "tracer.h"
#include "logger.h"


int main(void)
{
    /* 1. ¿De qué usuario son las shells que buscamos? */
    uid_t uid = getuid();
    if (getenv("SUDO_UID"))            /* con sudo, el usuario real */
        uid = atoi(getenv("SUDO_UID"));

    /* 2. Buscamos las shells (salvo la nuestra) */
    pid_t own = current_shell();

    Shell shells[MAX_SHELLS];
    int count = find_shells(shells, own, uid);

    if (count <= 0) {
        printf("No se encontraron otras shells.\n");
        return 0;
    }

    /* 3. El usuario elige cuál auditar */
    printf("Shells disponibles:\n\n");
    for (int i = 0; i < count; i++)
        printf("[%d] %s PID=%d TTY=%s\n",
               i + 1, shells[i].name, shells[i].pid, shells[i].tty);

    printf("\nSeleccione shell: ");

    int option;
    if (scanf("%d", &option) != 1 || option < 1 || option > count) {
        printf("Opción inválida.\n");
        return 1;
    }

    pid_t pid = shells[option - 1].pid;

    /* 4. Abrimos el log y nos enganchamos */
    if (log_open("audit.log") < 0)
        return 1;

    if (attach_shell(pid) < 0) {
        log_close();
        return 1;
    }

    log_start(pid);
    printf("\nTraceando PID=%d -> audit.log (Ctrl+C para terminar)\n", pid);

    /* 5. Bucle de trazado hasta que la shell termine */
    trace(pid);

    log_close();
    return 0;
}
