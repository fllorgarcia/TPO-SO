/*
 * simulador_planificacion.c
 *
 * Primera version simple de un simulador de planificacion de procesos.
 * Algoritmos incluidos:
 *   - FCFS
 *   - SJF no apropiativo
 *   - Round Robin
 *
 * El objetivo es comparar, de forma basica, el tiempo de espera
 * y el tiempo de retorno de distintos procesos.
 */

#include <stdio.h>

#define MAX_PROCESOS 10

typedef struct {
    int id;
    int rafaga;
    int espera;
    int retorno;
    int restante;
} Proceso;

void mostrarProcesos(Proceso procesos[], int cantidad) {
    int i;

    printf("\nProcesos cargados:\n");
    printf("PID\tRafaga\n");

    for (i = 0; i < cantidad; i++) {
        printf("P%d\t%d\n", procesos[i].id, procesos[i].rafaga);
    }
}

void mostrarResultados(Proceso procesos[], int cantidad) {
    int i;
    float promedioEspera = 0;
    float promedioRetorno = 0;

    printf("\nPID\tRafaga\tEspera\tRetorno\n");

    for (i = 0; i < cantidad; i++) {
        printf("P%d\t%d\t%d\t%d\n",
               procesos[i].id,
               procesos[i].rafaga,
               procesos[i].espera,
               procesos[i].retorno);

        promedioEspera += procesos[i].espera;
        promedioRetorno += procesos[i].retorno;
    }

    promedioEspera /= cantidad;
    promedioRetorno /= cantidad;

    printf("\nPromedio de espera: %.2f\n", promedioEspera);
    printf("Promedio de retorno: %.2f\n", promedioRetorno);
}

void fcfs(Proceso procesos[], int cantidad) {
    int i;
    int tiempo = 0;

    for (i = 0; i < cantidad; i++) {
        procesos[i].espera = tiempo;
        tiempo += procesos[i].rafaga;
        procesos[i].retorno = tiempo;
    }

    printf("\n=== FCFS ===\n");
    mostrarResultados(procesos, cantidad);
}

void sjf(Proceso procesos[], int cantidad) {
    Proceso copia[MAX_PROCESOS];
    Proceso aux;
    int i, j;
    int tiempo = 0;

    for (i = 0; i < cantidad; i++) {
        copia[i] = procesos[i];
    }

    for (i = 0; i < cantidad - 1; i++) {
        for (j = i + 1; j < cantidad; j++) {
            if (copia[j].rafaga < copia[i].rafaga) {
                aux = copia[i];
                copia[i] = copia[j];
                copia[j] = aux;
            }
        }
    }

    for (i = 0; i < cantidad; i++) {
        copia[i].espera = tiempo;
        tiempo += copia[i].rafaga;
        copia[i].retorno = tiempo;
    }

    printf("\n=== SJF ===\n");
    mostrarResultados(copia, cantidad);
}

void roundRobin(Proceso procesos[], int cantidad, int quantum) {
    Proceso copia[MAX_PROCESOS];
    int i;
    int tiempo = 0;
    int terminados = 0;

    for (i = 0; i < cantidad; i++) {
        copia[i] = procesos[i];
        copia[i].restante = copia[i].rafaga;
        copia[i].espera = 0;
        copia[i].retorno = 0;
    }

    while (terminados < cantidad) {
        for (i = 0; i < cantidad; i++) {
            if (copia[i].restante > 0) {

                if (copia[i].restante > quantum) {
                    tiempo += quantum;
                    copia[i].restante -= quantum;
                } else {
                    tiempo += copia[i].restante;
                    copia[i].restante = 0;
                    copia[i].retorno = tiempo;
                    copia[i].espera = copia[i].retorno - copia[i].rafaga;
                    terminados++;
                }
            }
        }
    }

    printf("\n=== ROUND ROBIN (quantum = %d) ===\n", quantum);
    mostrarResultados(copia, cantidad);
}

int main() {
    Proceso procesos[MAX_PROCESOS];
    int cantidad;
    int i;
    int opcion;
    int quantum;

    printf("SIMULADOR DE PLANIFICACION DE PROCESOS\n");
    printf("-------------------------------------\n");

    printf("Cantidad de procesos (maximo %d): ", MAX_PROCESOS);
    scanf("%d", &cantidad);

    if (cantidad <= 0 || cantidad > MAX_PROCESOS) {
        printf("Cantidad invalida.\n");
        return 1;
    }

    for (i = 0; i < cantidad; i++) {
        procesos[i].id = i + 1;

        printf("Rafaga de CPU para P%d: ", procesos[i].id);
        scanf("%d", &procesos[i].rafaga);

        procesos[i].espera = 0;
        procesos[i].retorno = 0;
        procesos[i].restante = procesos[i].rafaga;
    }

    mostrarProcesos(procesos, cantidad);

    do {
        printf("\nSeleccione un algoritmo:\n");
        printf("1. FCFS\n");
        printf("2. SJF\n");
        printf("3. Round Robin\n");
        printf("4. Salir\n");
        printf("Opcion: ");
        scanf("%d", &opcion);

        switch (opcion) {
            case 1:
                fcfs(procesos, cantidad);
                break;

            case 2:
                sjf(procesos, cantidad);
                break;

            case 3:
                printf("Ingrese quantum: ");
                scanf("%d", &quantum);

                if (quantum <= 0) {
                    printf("Quantum invalido.\n");
                } else {
                    roundRobin(procesos, cantidad, quantum);
                }
                break;

            case 4:
                printf("Fin del simulador.\n");
                break;

            default:
                printf("Opcion invalida.\n");
        }

    } while (opcion != 4);

    return 0;
}
