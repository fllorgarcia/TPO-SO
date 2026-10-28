#!/bin/bash
# TPO Unidad 5 - Spooler de impresora
# Tres programas mandan trabajos a la vez a una impresora que imprime de a uno.
# cola/   : orden de llegada de los trabajos (FIFO)
# buffer/ : copia del contenido de cada trabajo (el programa no espera a la impresora)
# impresora (en segundo plano con &) : atiende la cola de a un trabajo

COLA=cola
BUFFER=buffer

# ---- Colores y dibujos (solo para que la salida se entienda) ----
VIOLETA=$'\033[1;35m'; VERDE=$'\033[1;32m'; AMARILLO=$'\033[1;33m'; GRIS=$'\033[0;37m'; NORMAL=$'\033[0m'
color() { case $1 in Word) echo $'\033[1;34m';; Excel) echo $'\033[1;32m';; *) echo $'\033[1;31m';; esac; }
titulo() { echo; echo "${VIOLETA}════════ $1 ════════${NORMAL}"; }
mostrar_cola() {     # dibuja los trabajos que esperan: [Word][Excel]...
    lista=""
    for t in $(ls $COLA | tail -n +$1); do lista+="$(color ${t#_})[${t#_}]${NORMAL}"; done
    echo "   COLA esperando: ${lista:-${GRIS}(vacía)${NORMAL}}"
}

rm -rf $COLA $BUFFER fin
mkdir $COLA $BUFFER

# Un programa "imprime": guarda el texto en el buffer y lo anota al final de la cola
enviar_trabajo() {
    id=$(date +%s%N)_$1
    echo -e "$2" > $BUFFER/$id
    touch $COLA/$id
    echo "$(color $1)► $1${NORMAL} dejó un trabajo de $(wc -l < $BUFFER/$id) hoja(s) en el buffer y siguió con lo suyo"
}

# Proceso en segundo plano: el dispositivo lento
impresora() {
    sleep 2                                      # la impresora "calienta" antes de arrancar
    while true; do
        trabajo=$(ls $COLA | head -1)            # el más viejo primero (FIFO)
        if [ -z "$trabajo" ]; then
            [ -f fin ] && break                  # no llegan más trabajos
            sleep 0.2; continue                  # cola vacía: espera
        fi
        prog=${trabajo#*_}
        echo; echo "${AMARILLO}🖨️  IMPRESORA:${NORMAL} atiende el trabajo de $(color $prog)$prog${NORMAL}"
        mostrar_cola 2                           # los demás esperan su turno
        while read linea; do
            printf "   imprimiendo \"%s\" " "$linea"
            for i in 1 2 3 4 5 6 7 8 9 10; do printf "█"; sleep 0.1; done
            echo " ${VERDE}✔️${NORMAL}"
        done < $BUFFER/$trabajo
        rm $COLA/$trabajo $BUFFER/$trabajo       # libera la cola y el buffer
    done
    echo; echo "${AMARILLO}🖨️  IMPRESORA:${NORMAL} la cola quedó vacía, se apaga"
}

# Cada programa manda 2 trabajos
programa() {
    enviar_trabajo $1 "$1 - hoja 1\n$1 - hoja 2"
    sleep 0.5
    enviar_trabajo $1 "$1 - informe final"
}

titulo "SPOOLER DE IMPRESORA"
echo "Tres programas (Word, Excel y Navegador) quieren imprimir AL MISMO TIEMPO,"
echo "pero la impresora es LENTA y solo puede imprimir UN trabajo a la vez."
echo "Solución del sistema operativo (spooling):"
echo "  1. Cada programa deja su trabajo en un BUFFER (una copia en disco) y sigue trabajando."
echo "  2. El trabajo se anota en una COLA, por orden de llegada."
echo "  3. Un proceso en SEGUNDO PLANO (la impresora) los imprime de a uno."
read -p "Presioná Enter para empezar..."

titulo "1) LOS PROGRAMAS MANDAN A IMPRIMIR"
SECONDS=0
impresora &                 # el demonio arranca en segundo plano
PID_IMP=$!
echo "${GRIS}(la impresora arrancó en segundo plano con PID $PID_IMP y se está calentando)${NORMAL}"
programa Word & programa Excel & programa Navegador &   # los 3 a la vez
wait %2 %3 %4               # espera solo a los programas

FIN_PROGRAMAS=$SECONDS
titulo "2) LOS PROGRAMAS YA TERMINARON (en $FIN_PROGRAMAS s)"
echo "No tuvieron que esperar a la impresora. Sus trabajos quedaron en la cola:"
mostrar_cola 1
titulo "3) LA IMPRESORA ATIENDE LA COLA DE A UNO"
touch fin                   # avisa a la impresora que no llegan más trabajos
wait $PID_IMP

titulo "RESUMEN"
echo "Los programas terminaron en $FIN_PROGRAMAS s y la impresora en $SECONDS s."
echo "Gracias al buffer y la cola, ningún programa quedó trabado esperando al dispositivo lento."
rm -rf $COLA $BUFFER fin