/*
 * auditor.c - Auditor de syscalls (Primera entrega)
 *
 * Compilar:  gcc -Wall auditor.c -o auditor
 * Ejecutar:  ./auditor
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>

FILE *log_file;

/* Escribe una linea en el log con la hora y el PID */
void registrar(const char *texto)
{
    char hora[16];
    time_t ahora = time(NULL);
    strftime(hora, sizeof(hora), "%H:%M:%S", localtime(&ahora));

    fprintf(log_file, "[%s] PID=%d %s\n", hora, getpid(), texto);
}

/* Comando ls: lista los archivos de la carpeta actual */
void comando_ls(void)
{
    DIR *carpeta;
    struct dirent *archivo;

    registrar("Comando: ls");

    carpeta = opendir(".");
    registrar("   syscall openat(\".\")  -> abre la carpeta actual");

    if (carpeta == NULL) {
        printf("No se pudo abrir la carpeta\n");
        registrar("   ERROR: no se pudo abrir la carpeta");
        return;
    }

    registrar("   syscall getdents64()  -> lee la lista de archivos");
    while ((archivo = readdir(carpeta)) != NULL) {
        if (archivo->d_name[0] != '.') {        /* saltear ocultos, . y .. */
            printf("%s  ", archivo->d_name);
        }
    }
    printf("\n");
    registrar("   syscall write(1)      -> muestra los nombres en pantalla");

    closedir(carpeta);
    registrar("   syscall close()       -> cierra la carpeta");
}

/* Comando echo: muestra el texto en pantalla */
void comando_echo(const char *texto)
{
    char mensaje[300];

    snprintf(mensaje, sizeof(mensaje), "Comando: echo %s", texto);
    registrar(mensaje);

    write(1, texto, strlen(texto));   /* 1 = la pantalla (salida estandar) */
    write(1, "\n", 1);

    snprintf(mensaje, sizeof(mensaje), "   syscall write(1, \"%s\") -> escribe en pantalla", texto);
    registrar(mensaje);
}

int main(void)
{
    char linea[256];
    char mensaje[300];

    log_file = fopen("audit.log", "a");   /* "a" = agregar al final */
    if (log_file == NULL) {
        printf("No se pudo abrir audit.log\n");
        return 1;
    }

    printf("=== Auditoria iniciada ===\n");
    printf("Comandos disponibles: ls, echo <texto>, exit\n\n");
    registrar("=== Inicio de auditoria ===");

    while (1) {
        printf("mini-shell> ");
        fflush(stdout);                        /* mostrar el prompt ya */

        if (fgets(linea, sizeof(linea), stdin) == NULL)
            break;                             /* Ctrl+D */

        linea[strcspn(linea, "\n")] = '\0';     /* sacar el Enter */

        if (strcmp(linea, "exit") == 0) {
            break;
        } else if (strcmp(linea, "ls") == 0) {
            comando_ls();
        } else if (strncmp(linea, "echo ", 5) == 0) {
            comando_echo(linea + 5);           /* lo que viene despues de "echo " */
        } else if (linea[0] != '\0') {
            printf("Comando no reconocido: %s\n", linea);
            snprintf(mensaje, sizeof(mensaje), "Comando no reconocido: %s", linea);
            registrar(mensaje);
        }
    }

    registrar("=== Fin de auditoria ===");
    fclose(log_file);

    printf("\n=== Auditoria finalizada ===\n");
    printf("Ver audit.log\n");
    return 0;
}