#!/bin/bash
# Uso: ./mmu_sim.sh        -> salida directa
#      ./mmu_sim.sh -t     -> modo TERMINAL

########################### TERMINAL ###########################

TERMINAL=0
[ "$1" = "-t" ] && TERMINAL=1

AMARILLO=$'\033[1;33m'; VERDE=$'\033[1;32m'; ROJO=$'\033[1;31m'; CELESTE=$'\033[1;36m'; NORMAL=$'\033[0m'

# Si se aprieta Enter mientras se escribe, se muestra el resto de la sección de una
SALTAR=0
ES_TTY=0
if [ $TERMINAL -eq 1 ] && [ -t 0 ]; then
    ES_TTY=1
    STTY_ORIGINAL=$(stty -g)
    stty -icanon -echo min 0 time 0      # leer teclas sin esperar ni mostrarlas
    trap 'stty "$STTY_ORIGINAL"' EXIT    # al salir, la terminal vuelve a la normalidad
fi

# Devuelve verdadero si se apretó una tecla (no se queda esperando).
# Se usa dd porque respeta "min 0 time 0" y funciona igual en Linux y en Mac.
t_tecla() {
    [ $ES_TTY -eq 1 ] || return 1
    local k
    k=$(dd bs=1 count=1 2>/dev/null; echo x)
    [ "$k" != "x" ]
}

# Espera los segundos indicados, salvo que se haya apretado Enter
t_esperar() {
    [ $SALTAR -eq 1 ] && return
    t_tecla && { SALTAR=1; return; }
    sleep "$1"
}

# Escribe el texto de a poco, como si alguien lo estuviera tipeando
t_escribir() {
    local texto="$1" i
    if [ $SALTAR -eq 0 ]; then
        for ((i = 0; i < ${#texto}; i += 3)); do
            printf '%s' "${texto:i:3}"
            if t_tecla; then
                SALTAR=1
                printf '%s' "${texto:i+3}"
                break
            fi
            sleep 0.01
        done
    else
        printf '%s' "$texto"
    fi
    echo
}

t_pausa() {
    echo
    printf "%s" "   [ Enter para continuar ] "
    if [ $ES_TTY -eq 1 ]; then
        while t_tecla; do :; done        # descarta teclas apretadas de más
        until t_tecla; do sleep 0.05; done
        echo
    else
        read -r _
    fi
    SALTAR=0                             # la próxima sección vuelve a escribirse de a poco
}

t_titulo() {
    echo
    echo "${CELESTE}==================================================${NORMAL}"
    echo "${CELESTE}  $1${NORMAL}"
    echo "${CELESTE}==================================================${NORMAL}"
}

t_mostrar_tabla() {
    local p
    echo "     Página | Marco"
    echo "     -------+-----------------"
    for p in "${!tabla[@]}"; do
        if [ "${tabla[$p]}" -eq -1 ]; then
            echo "       $p    | ${ROJO}-1 (está en disco)${NORMAL}"
        else
            echo "       $p    | ${tabla[$p]}"
        fi
    done
}

t_mostrar_tlb() {
    local p
    if [ ${#tlb[@]} -eq 0 ]; then
        echo "     TLB: (vacía)"
    else
        printf "%s" "     TLB: "
        for p in "${!tlb[@]}"; do printf "%s" "[página $p -> marco ${tlb[$p]}]  "; done
        echo
    fi
}

t_introduccion() {
    [ $TERMINAL -eq 1 ] || return
    clear
    t_titulo "SIMULADOR DE LA MMU (Unidad de Gestión de Memoria)"
    echo
    t_escribir "Un programa nunca ve la memoria RAM real. Ve direcciones VIRTUALES"
    t_escribir "(o lógicas), que empiezan en 0 como si tuviera toda la memoria para él."
    t_escribir "La MMU es el hardware que traduce cada dirección virtual a la"
    t_escribir "dirección FÍSICA, que es el lugar real dentro de la RAM."
    t_esperar 0.6
    echo
    t_escribir "Para hacerlo se usa PAGINACIÓN:"
    t_escribir "  - El programa se corta en pedazos iguales llamados PÁGINAS."
    t_escribir "  - La RAM se corta en pedazos del mismo tamaño llamados MARCOS."
    t_escribir "  - En este ejemplo cada página/marco mide $TAM_PAGINA bytes."
    t_pausa

    t_titulo "LA TABLA DE PÁGINAS"
    t_escribir "La tabla de páginas anota en qué marco de la RAM quedó cada página."
    t_escribir "Está guardada en la RAM, a partir de la dirección PTBR = $PTBR."
    echo
    t_mostrar_tabla
    echo
    t_escribir "Las páginas 2 y 3 todavía están en el disco: si se piden, habrá"
    t_escribir "un FALLO DE PÁGINA y el sistema operativo las tendrá que cargar en la RAM."
    t_pausa

    t_titulo "LA TLB"
    t_escribir "Ir a buscar a la tabla de páginas cuesta un acceso extra a la RAM."
    t_escribir "Para ahorrarlo, la MMU tiene una memoria chica y rápida, la TLB,"
    t_escribir "que recuerda las últimas traducciones. La nuestra guarda solo 2."
    echo
    t_mostrar_tlb
    echo
    t_escribir "El programa va a pedir estas direcciones: ${direcciones[*]}"
    t_escribir "Vamos a traducirlas una por una."
    t_pausa
}

t_paso1() {
    [ $TERMINAL -eq 1 ] || return
    clear
    t_titulo "DIRECCIÓN VIRTUAL $dir"
    echo
    t_escribir "PASO 1: ¿En qué página cae y en qué posición dentro de ella?"
    t_esperar 0.3
    t_escribir "   Página = $dir ÷ $TAM_PAGINA = $pagina    (cuántas páginas enteras entran)"
    t_esperar 0.3
    t_escribir "   Offset = resto de $dir ÷ $TAM_PAGINA = $offset    (cuántos bytes adentro de la página)"
    t_esperar 0.6
}

t_paso2() {
    [ $TERMINAL -eq 1 ] || return
    echo
    t_escribir "PASO 2: Primero se busca la página $pagina en la TLB (lo más rápido)."
    t_mostrar_tlb
    t_esperar 0.6
}

t_hit() {
    [ $TERMINAL -eq 1 ] || return
    echo "${VERDE}   ¡Está! Es un TLB HIT: no hace falta ir a la tabla de páginas.${NORMAL}"
    t_esperar 0.6
}

t_miss() {
    [ $TERMINAL -eq 1 ] || return
    echo "${AMARILLO}   No está. Es un TLB MISS: hay que leer la tabla de páginas en la RAM.${NORMAL}"
    t_escribir "   Su entrada está en PTBR + página = $PTBR + $pagina = $((PTBR + pagina))"
    t_esperar 0.6
    echo
    t_mostrar_tabla
    t_esperar 0.6
}

t_fallo() {
    [ $TERMINAL -eq 1 ] || return
    echo
    echo "${ROJO}   La página $pagina tiene -1: ¡está en el disco! FALLO DE PÁGINA.${NORMAL}"
    t_escribir "   La CPU solo puede usar lo que está en la RAM, así que el sistema"
    t_escribir "   operativo copia la página del disco al primer marco libre ($marco_libre)"
    t_escribir "   y actualiza la tabla de páginas."
    printf "%s" "   Cargando desde el disco"
    for _ in 1 2 3; do t_esperar 0.4; printf "%s" "."; done
    echo
}

t_tlb_llena() {
    [ $TERMINAL -eq 1 ] || return
    [ ${#tlb[@]} -ge 2 ] && t_escribir "   La TLB está llena (2 de 2), así que se vacía para hacer lugar."
    t_escribir "   Se guarda 'página $pagina -> marco $marco' en la TLB para la próxima vez."
    t_esperar 0.6
}

t_paso4() {
    [ $TERMINAL -eq 1 ] || return
    echo
    t_escribir "PASO 3: Calcular la dirección física"
    t_escribir "   Inicio del marco = marco × tamaño = $marco × $TAM_PAGINA = $((marco * TAM_PAGINA))"
    t_escribir "   Se le suma el offset: $((marco * TAM_PAGINA)) + $offset = $((marco * TAM_PAGINA + offset))"
    t_esperar 0.6
    echo "${VERDE}   Dirección virtual $dir  ==>  dirección física $((marco * TAM_PAGINA + offset))${NORMAL}"
}

t_fin_paso() {
    [ $TERMINAL -eq 1 ] || return
    t_pausa
}

t_resultados() {
    [ $TERMINAL -eq 1 ] || return
    clear
    t_titulo "RESULTADOS"
    echo
    echo "Hits: $hits   Misses: $misses   Fallos de página: $fallos"
    t_pausa
    t_titulo "¿CUÁNTO TIEMPO NOS AHORRA LA TLB?"
    t_escribir "Supongamos que ir a buscar el dato tarda 100 ns, leer la tabla de páginas otros 100 ns y leer la TLB tarda 1 ns."
    echo
    t_escribir "SIN TLB: ${#direcciones[@]} direcciones × 200 ns = $(( ${#direcciones[@]} * 200 )) ns"
    t_esperar 0.6
    echo
    t_escribir "CON TLB: un HIT  cuesta 1 (TLB) + 100 (dato)        = 101 ns"
    t_escribir "         un MISS cuesta 1 (TLB) + 100 (tabla) + 100 = 201 ns"
    t_escribir "         $hits × 101 + $misses × 201 = $(( hits * 101 + misses * 201 )) ns"
    t_esperar 0.6
    echo
    echo "${VERDE}   La TLB ahorró $(( ${#direcciones[@]} * 200 - (hits * 101 + misses * 201) )) ns.${NORMAL}"
}
######################### FIN TERMINAL #########################

TAM_PAGINA=256   # tamaño de cada página (y de cada marco)
PTBR=1000        # dónde empieza la tabla de páginas en la RAM

# Tabla de páginas: posición = página, valor = marco (-1 = está en disco)
tabla=(3 5 -1 -1)
marco_libre=6    # próximo marco libre para cargar páginas

# TLB: guarda las traducciones recientes (máximo 2)
tlb=()

hits=0
misses=0
fallos=0

# Direcciones que pide el programa
direcciones=(10 20 300 600 610 1000 15)

t_introduccion

for dir in "${direcciones[@]}"; do

    # Paso 1: dividir en página y offset
    pagina=$((dir / TAM_PAGINA))
    offset=$((dir % TAM_PAGINA))
    t_paso1                                          # TERMINAL
    echo
    echo "Dirección $dir -> página $pagina, offset $offset"

    # Paso 2: buscar en la TLB
    t_paso2                                          # TERMINAL
    if [ -n "${tlb[$pagina]}" ]; then
        marco=${tlb[$pagina]}
        t_hit                                        # TERMINAL
        echo "  TLB HIT -> marco $marco"
        ((hits++))
    else
        t_miss                                       # TERMINAL
        echo "  TLB MISS -> se busca en la tabla de páginas (PTBR + $pagina = $((PTBR + pagina)))"
        ((misses++))

        # Paso 3: si la página está en disco, es un fallo de página
        if [ "${tabla[$pagina]}" -eq -1 ]; then
            t_fallo                                  # TERMINAL
            echo "   Se carga la página $pagina en el marco $marco_libre"
            tabla[$pagina]=$((marco_libre++))        # asigna el marco libre y avanza
            ((fallos++))
        fi

        marco=${tabla[$pagina]}

        # Guardar en la TLB (si está llena, se vacía)
        t_tlb_llena                                  # TERMINAL
        [ ${#tlb[@]} -ge 2 ] && tlb=()
        tlb[$pagina]=$marco
    fi

    # Paso 4: calcular la dirección física
    t_paso4                                          # TERMINAL
    echo "  Dirección física ==> $((marco * TAM_PAGINA + offset))"
    t_fin_paso                                       # TERMINAL
done

t_resultados                                         # TERMINAL