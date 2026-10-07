# Simulador de Planificación de Procesos

Primera versión de un simulador simple de planificación de procesos en C.

Incluye:

- FCFS
- SJF no apropiativo
- Round Robin
- Cálculo de tiempo de espera
- Cálculo de tiempo de retorno

## Requisitos en Red Hat / RHEL

En Red Hat Enterprise Linux se puede utilizar `gcc` para compilar programas en C.

Primero instalar el compilador:

    sudo dnf install gcc

En versiones más antiguas de Red Hat también puede utilizarse:

    sudo yum install gcc

Para comprobar que quedó instalado correctamente:

    gcc --version

## Compilar en Red Hat

Ubicarse desde la terminal en la carpeta donde se encuentra el archivo:

    simulador_planificacion.c

Luego ejecutar:

    gcc simulador_planificacion.c -o simulador

Esto genera un ejecutable llamado:

    simulador

## Ejecutar en Red Hat

    ./simulador

## Compilar y ejecutar en un solo comando

    gcc simulador_planificacion.c -o simulador && ./simulador

## Requisitos en macOS

Para compilar programas en C en macOS se pueden utilizar las Command Line Tools de Xcode, que incluyen el compilador `clang`.

Abrir una terminal y ejecutar:

```bash
xcode-select --install
```

Comprobar que el compilador quedó instalado:

```bash
clang --version
```

## Compilar

Ubicarse desde la terminal en la carpeta donde se encuentra:

```text
simulador_planificacion.c
```

Luego ejecutar:

```bash
clang simulador_planificacion.c -o simulador
```

Esto genera un archivo ejecutable llamado:

```text
simulador
```

## Ejecutar

Para iniciar el programa:

```bash
./simulador
```

## Compilar y ejecutar en un solo comando

También se puede hacer todo seguido:

```bash
clang simulador_planificacion.c -o simulador && ./simulador
```

## Ejemplo

Si los archivos están en Descargas:

```bash
cd ~/Downloads
clang simulador_planificacion.c -o simulador
./simulador
```

## Archivos del proyecto

```text
simulador_planificacion.c
README.md
```
