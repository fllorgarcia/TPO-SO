#!/bin/bash
# TPO Unidad 5 - Spooler de impresora
# Tres programas mandan trabajos a la vez a una impresora que imprime de a uno.
# cola/   : orden de llegada de los trabajos (FIFO)
# buffer/ : copia del contenido de cada trabajo (el programa no espera a la impresora)
# impresora (en segundo plano con &) : atiende la cola de a un trabajo

COLA=cola
BUFFER=buffer
VELOCIDAD=1          # segundos que tarda la impresora en imprimir una linea

rm -rf $COLA $BUFFER fin
mkdir $COLA $BUFFER

# Un programa "imprime": guarda el texto en el buffer y lo anota en la cola
enviar_trabajo() {
    id=$(date +%s%N)_$1
    echo -e "$2" > $BUFFER/$id
    touch $COLA/$id
    echo "[$(date +%T)] $1 envio un trabajo y sigue trabajando"
}

# Proceso en segundo plano: el dispositivo lento
impresora() {
    while true; do
        trabajo=$(ls $COLA | head -1)            # el mas viejo primero
        if [ -z "$trabajo" ]; then
            [ -f fin ] && break                  # no hay mas trabajos
            sleep 0.2; continue                  # cola vacia: espera
        fi
        echo "[$(date +%T)] IMPRESORA: empieza ${trabajo#*_} (trabajos en cola: $(ls $COLA | wc -l))"
        while read linea; do
            sleep $VELOCIDAD
            echo "              | $linea"
        done < $BUFFER/$trabajo
        rm $COLA/$trabajo $BUFFER/$trabajo       # libera la cola y el buffer
        echo "[$(date +%T)] IMPRESORA: termino ${trabajo#*_}"
    done
    echo "[$(date +%T)] IMPRESORA: cola vacia, se apaga"
}

# Cada programa manda 2 trabajos
programa() {
    enviar_trabajo $1 "$1 - hoja 1\n$1 - hoja 2"
    sleep 0.5
    enviar_trabajo $1 "$1 - informe final"
    echo "[$(date +%T)] $1 TERMINO (no espero a la impresora)"
}

impresora & 
PID_IMP=$!
echo "Impresora en segundo plano con PID $PID_IMP"

programa Word & programa Excel & programa Navegador &   # los 3 a la vez
wait %2 %3 %4               # espera solo a los programas

echo ">>> Los 3 programas terminaron. Trabajos en cola: $(ls $COLA | wc -l)"
touch fin                   # avisa a la impresora que no llegan mas
wait $PID_IMP
rm -rf $COLA $BUFFER fin