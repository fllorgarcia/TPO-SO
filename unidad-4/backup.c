#include <stdio.h>
#include <string.h>
#include <fcntl.h>      // open
#include <unistd.h>     // read, write, close
#include <dirent.h>     // opendir, readdir, closedir
#include <sys/stat.h>   // stat, mkdir

void copiar(char *origen, char *destino) {
    char buffer[4096];
    int n;
    int fd_in = open(origen, O_RDONLY);
    int fd_out = open(destino, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    while ((n = read(fd_in, buffer, sizeof(buffer))) > 0)
        write(fd_out, buffer, n);

    close(fd_in);
    close(fd_out);
}

int main() {
    char dir_origen[256], dir_destino[256];
    char ruta_origen[512], ruta_destino[512];
    struct dirent *entrada;
    struct stat info;
    int examinados = 0, respaldados = 0;

    printf("Directorio de origen: ");
    scanf("%255s", dir_origen);
    printf("Directorio de destino: ");
    scanf("%255s", dir_destino);

    DIR *dir = opendir(dir_origen);
    if (dir == NULL) {
        perror("opendir");
        return 1;
    }

    mkdir(dir_destino, 0755);   // si ya existe, no pasa nada

    while ((entrada = readdir(dir)) != NULL) {
        sprintf(ruta_origen, "%s/%s", dir_origen, entrada->d_name);
        stat(ruta_origen, &info);

        // Solo archivos regulares (se saltean directorios, ".", ".." y archivos especiales)
        if (!S_ISREG(info.st_mode))
            continue;

        examinados++;
        sprintf(ruta_destino, "%s/%s", dir_destino, entrada->d_name);
        copiar(ruta_origen, ruta_destino);
        respaldados++;
        printf("  %-15s - copiado (%ld bytes)\n", entrada->d_name, (long) info.st_size);
    }

    closedir(dir);
    printf("\nArchivos examinados: %d\n", examinados);
    printf("Respaldados: %d\n", respaldados);
    return 0;
}