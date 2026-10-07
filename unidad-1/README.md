# Shell Syscall Auditor

Programa en C para Linux que utiliza `ptrace(2)` para observar las llamadas al sistema realizadas por una shell y por los procesos creados desde ella.

El proyecto está pensado originalmente para sistemas Red Hat Enterprise Linux y compatibles, pero utiliza interfaces estándar del kernel Linux y no APIs específicas de RHEL.

## Arquitectura

```text
shell-auditor
     |
     | descubre shells mediante /proc
     |
     +---- bash A
     |
     +---- bash B
     |
     +---- bash C
             |
             +-- ls
             +-- cat
             +-- mkdir
             +-- python
                   |
                   +-- otros procesos
```

El usuario selecciona una shell y el auditor utiliza `ptrace()` para observarla.

Los procesos creados mediante `fork`, `vfork` o `clone` también son seguidos automáticamente.

Las syscalls consideradas relevantes son escritas en:

```text
audit.log
```

---

# Ejecución directa en Red Hat Enterprise Linux

En RHEL no es necesario utilizar Docker.

El programa puede compilarse y ejecutarse directamente sobre el sistema operativo Linux.

Esta es además la forma más cercana al entorno objetivo real del proyecto.

## Requisitos

Se necesita:

- Red Hat Enterprise Linux 8, 9 o 10
- GCC
- GNU Make
- headers estándar de Linux/glibc
- permisos para utilizar `ptrace`

Comprobar si GCC y Make ya están instalados:

```bash
gcc --version
make --version
```

Si ambos comandos funcionan, puede pasarse directamente a la sección de compilación.

## Instalar herramientas de compilación

La opción recomendada en RHEL es instalar el grupo `Development Tools`:

```bash
sudo dnf group install "Development Tools"
```

Este grupo incluye GCC, Make, GDB y otras herramientas habituales de desarrollo.

Red Hat documenta este grupo como el entorno estándar para desarrollar aplicaciones C y C++ en RHEL.

También puede realizarse una instalación mínima si únicamente interesa compilar este proyecto:

```bash
sudo dnf install gcc make
```

Verificar después:

```bash
gcc --version
make --version
```

En instalaciones antiguas de RHEL 7 puede utilizarse `yum` en lugar de `dnf`:

```bash
sudo yum groupinstall "Development Tools"
```

---

# Obtener el proyecto en RHEL

Copiar o clonar el directorio del proyecto en el servidor.

Por ejemplo:

```bash
cd ~
git clone <URL_DEL_REPOSITORIO>
cd unidad-1
```

Si el proyecto fue copiado manualmente:

```bash
cd /ruta/al/proyecto
```

El directorio debería contener archivos similares a:

```text
Dockerfile
Makefile
README.md
logger.c
logger.h
main.c
procfs.c
procfs.h
procs.c
procs.h
shells.c
shells.h
syscalls.c
syscalls.h
tracer.c
tracer.h
```

---

# Compilar en RHEL

El proyecto incluye un `Makefile`, por lo que la forma recomendada de compilar es:

```bash
make
```

Después de compilar debería generarse el ejecutable:

```text
shell-auditor
```

Verificar:

```bash
ls -lh shell-auditor
```

Ejemplo:

```text
-rwxr-xr-x. 1 usuario usuario 35K Oct 7 18:20 shell-auditor
```

Para recompilar completamente:

```bash
make clean
make
```

---

# Compilación manual

Si por algún motivo no se desea utilizar el `Makefile`, también puede compilarse directamente con GCC.

Por ejemplo:

```bash
gcc \
    -O2 \
    -Wall \
    -Wextra \
    main.c \
    logger.c \
    procfs.c \
    procs.c \
    shells.c \
    syscalls.c \
    tracer.c \
    -o shell-auditor
```

Luego:

```bash
./shell-auditor
```

La compilación mediante `make` es preferible porque centraliza las opciones de compilación utilizadas por el proyecto.

---

# Ejecutar el auditor en RHEL

Primero abrir una terminal con una shell que vaya a ser auditada.

Por ejemplo:

```bash
bash
```

Dejar esa terminal abierta.

Abrir otra terminal o sesión SSH sobre el mismo servidor:

```bash
cd /ruta/al/proyecto
./shell-auditor
```

El programa mostrará algo similar a:

```text
=== Shell syscall auditor ===

Shell actual excluida: bash (PID 3521)

Shells disponibles:

[1] bash     PID=3412     PPID=3401     TTY=/dev/pts/1

Seleccione shell:
```

Seleccionar:

```text
1
```

El auditor comenzará a observar esa shell y los procesos descendientes creados desde ella.

---

# Generar actividad

Volver a la terminal seleccionada y ejecutar, por ejemplo:

```bash
mkdir prueba
touch prueba/hola.txt
cat /etc/hosts
rm prueba/hola.txt
rmdir prueba
```

También pueden ejecutarse otros programas:

```bash
ls -la
```

```bash
bash
```

Los procesos descendientes de la shell seleccionada también serán seguidos.

---

# Ver el log

El auditor escribe los eventos detectados en:

```text
audit.log
```

Para visualizarlo:

```bash
cat audit.log
```

También puede observarse en tiempo real:

```bash
tail -f audit.log
```

Ejemplo:

```text
--- START TRACE shell=bash pid=3412 ---

PID=3412 created child PID=3500
PID=3500 syscall=execve path="/usr/bin/mkdir" result=OK return=0
PID=3500 syscall=mkdir path="prueba" result=OK return=0

PID=3412 created child PID=3504
PID=3504 syscall=execve path="/usr/bin/cat" result=OK return=0
PID=3504 syscall=openat path="/etc/hosts" result=OK return=3

PID=3412 created child PID=3510
PID=3510 syscall=execve path="/usr/bin/rm" result=OK return=0
PID=3510 syscall=unlinkat path="prueba/hola.txt" result=OK return=0
```

---

# Permisos de ptrace en RHEL

`ptrace` permite que un proceso inspeccione y controle otros procesos.

Por razones de seguridad, Linux restringe qué procesos pueden ser observados.

La forma más sencilla de probar el laboratorio es ejecutar el auditor con privilegios de root:

```bash
sudo ./shell-auditor
```

Sin embargo, es importante tener en cuenta que el proceso que se desea auditar debe ser visible y accesible para el auditor.

Por ejemplo, puede abrirse una shell como root:

```bash
sudo bash
```

y luego, desde otra terminal:

```bash
cd /ruta/al/proyecto
sudo ./shell-auditor
```

## CAP_SYS_PTRACE

Como alternativa a ejecutar siempre el programa mediante `sudo`, Linux dispone de la capability:

```text
CAP_SYS_PTRACE
```

Puede asignarse al ejecutable con:

```bash
sudo setcap cap_sys_ptrace+ep ./shell-auditor
```

Verificar:

```bash
getcap ./shell-auditor
```

Debería aparecer algo similar a:

```text
./shell-auditor cap_sys_ptrace=ep
```

A partir de ese momento puede probarse:

```bash
./shell-auditor
```

Para quitar la capability:

```bash
sudo setcap -r ./shell-auditor
```

Si el comando `setcap` no está disponible, instalar el paquete correspondiente:

```bash
sudo dnf install libcap
```

El uso de capabilities permite conceder específicamente el permiso necesario para tracing sin otorgar al programa todos los privilegios de root.

---

# SELinux

RHEL utiliza SELinux de forma predeterminada.

El proyecto no requiere que SELinux sea deshabilitado para su funcionamiento normal.

No se recomienda ejecutar:

```bash
setenforce 0
```

ni desactivar SELinux permanentemente únicamente para utilizar este programa.

Si el sistema posee políticas de seguridad adicionales que impiden específicamente el tracing entre procesos, deben revisarse dichas políticas antes de modificar la configuración global de SELinux.

---

# Resumen para RHEL

En una instalación limpia de RHEL:

```bash
sudo dnf install gcc make
```

Abrir una shell objetivo:

```bash
sudo bash
```

Y desde otra terminal:

```bash
sudo ./shell-auditor
```

Después generar actividad desde la primera shell:

```bash
mkdir prueba
touch prueba/hola.txt
cat /etc/hosts
rm prueba/hola.txt
rmdir prueba
```

Y revisar:

```bash
cat audit.log
```

---

# Desarrollo desde macOS utilizando Docker

Debido a que `ptrace`, `/proc` y las estructuras utilizadas por el proyecto son específicas de Linux, el programa no puede ejecutarse directamente sobre el kernel de macOS.

Docker Desktop permite utilizar una máquina virtual Linux y ejecutar allí el auditor.

## Requisitos en macOS

- Docker Desktop
- Docker CLI

Verificar:

```bash
docker --version
```

---

# Construcción de la imagen Docker

Desde el directorio del proyecto:

```bash
docker build -t shell-auditor .
```

---

# Ejecutar el entorno Docker

El programa necesita permiso para utilizar `ptrace`.

Crear el contenedor con:

```bash
docker run \
    --name syscall-lab \
    --cap-add=SYS_PTRACE \
    --security-opt seccomp=unconfined \
    -v "$(pwd)":/workspace \
    -it \
    shell-auditor
```

La opción:

```text
--cap-add=SYS_PTRACE
```

permite al contenedor utilizar `ptrace` sobre otros procesos para los que tenga permiso.

La opción:

```text
--security-opt seccomp=unconfined
```

desactiva el perfil seccomp del contenedor durante este laboratorio para evitar restricciones sobre operaciones de tracing.

Este modo está pensado solamente para el entorno de desarrollo.

No utilizar:

```bash
--privileged
```

ya que otorgaría muchos más privilegios de los necesarios.

---

# Abrir una shell para auditar en Docker

Abrir una nueva terminal de macOS:

```bash
docker exec -it syscall-lab bash
```

Esta será la shell que utilizaremos como objetivo.

Por ejemplo:

```text
root@container:/app#
```

Dejar esa terminal abierta.

---

# Abrir el auditor en Docker

Abrir una segunda terminal de macOS:

```bash
docker exec -it syscall-lab bash
```

Dentro del contenedor:

```bash
cd /app
./shell-auditor
```

El programa mostrará algo similar a:

```text
=== Shell syscall auditor ===

Shell actual excluida: bash (PID 35)

Shells disponibles:

[1] bash     PID=22     PPID=0     TTY=/dev/pts/1

Seleccione shell:
```

Seleccionar:

```text
1
```

---

# Desarrollo desde macOS

El código fuente puede editarse directamente desde macOS.

El directorio del proyecto está montado como:

```text
/workspace
```

dentro del contenedor.

Después de modificar el código:

```bash
cd /workspace
make clean
make
```

y ejecutar:

```bash
./shell-auditor
```

Esto permite editar el proyecto desde VS Code, CLion u otro editor de macOS y compilarlo dentro de Linux.

---

# Detener el entorno Docker

Salir de las shells:

```bash
exit
```

Desde macOS:

```bash
docker stop syscall-lab
docker rm syscall-lab
```

---

# Consideraciones sobre macOS

El programa no está ejecutándose directamente sobre el kernel de macOS.

Docker Desktop ejecuta contenedores Linux dentro de una máquina virtual Linux:

```text
macOS
 |
 +-- Docker Desktop
       |
       +-- Linux VM
             |
             +-- container
                   |
                   +-- shell-auditor
                   +-- bash
                   +-- procesos Linux
```

El auditor puede observar los procesos Linux dentro de ese entorno.

No puede observar procesos nativos de macOS como:

```text
Terminal.app
Safari
Finder
zsh ejecutado directamente por macOS
```

Para probar el comportamiento que posteriormente tendrá en RHEL, esto es suficiente porque el programa utiliza las mismas interfaces Linux:

- `/proc`
- `ptrace(2)`
- `waitpid(2)`
- `fork`
- `vfork`
- `clone`
- `exec`
- Linux capabilities

---

# Seguridad

`ptrace` permite inspeccionar profundamente otros procesos.

Debe concederse únicamente el nivel de privilegio necesario para realizar el tracing.

En Docker se utiliza:

```text
CAP_SYS_PTRACE
```

En RHEL puede utilizarse:

```bash
sudo ./shell-auditor
```

o asignar específicamente:

```bash
sudo setcap cap_sys_ptrace+ep ./shell-auditor
```

No se recomienda otorgar permisos adicionales innecesarios.

---

# Compatibilidad

Diseñado para sistemas Linux, principalmente:

```text
Linux
├── Red Hat Enterprise Linux 8/9/10
├── Rocky Linux
├── AlmaLinux
├── Fedora
├── Ubuntu
└── Debian
```

En Red Hat Enterprise Linux puede compilarse y ejecutarse directamente.

En macOS debe utilizarse un entorno Linux, como Docker Desktop, ya que macOS no implementa las mismas interfaces de tracing y `/proc` utilizadas por este proyecto.
