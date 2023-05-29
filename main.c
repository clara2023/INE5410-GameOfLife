#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "gol.h"

// transformadas em variáveis globais
// para permitir acesso pelas threads
int size, steps, Nthreads;
#ifdef DEBUG
    stats_t stats_step = {0, 0, 0, 0};
#endif
// controle de concorrência
int FLAG_S;
pthread_mutex_t mutex0;
sem_t *semaforo;

// Função que as threads executam,
// retirada da main
void* jogar(void *arg) {
    slice *param = (slice *)arg;
    // cada thread inicializando
    // o próprio semáforo
    sem_init(&semaforo[param->id], 0, 0);

    cell_t** tmp;
    int linhaI = param->beg / size;
    int colunaI = param->beg % size;
    int linhaF = param->end / size;
    int colunaF = param->end % size;
    if (!colunaF) {
        colunaF = size;
    }
    printf("Thread %d\n", param->id);
    printf("begin %d\n", param->beg);
    printf("end %d\n\n", param->end);
    printf("linhaI = %d\n", linhaI);
    printf("ColunaI = %d\n", colunaI);
    printf("linhaF = %d\n", linhaF);
    printf("colunaF = %d\n\n\n", colunaF);

    for (int i = 0; i < steps; i++) {
        // cada thread lidará com um slice do tabuleiro,
        // recebendo as próprias estatísticas
        play(param->prev, param->next,
             size, linhaI, linhaF, colunaI,
             colunaF, &(param->stats_step));
        // cada thread também tem suas próprias
        // estatísticas totais e por step
        param->stats_total.borns += param->stats_step.borns;
        param->stats_total.survivals += param->stats_step.survivals;
        param->stats_total.loneliness += param->stats_step.loneliness;
        param->stats_total.overcrowding += param->stats_step.overcrowding;

        tmp = param->next;
        param->next = param->prev;
        param->prev = tmp;
        
        // alterando variáveis globais
        // em uma região de exclusão mútua
        pthread_mutex_lock(&mutex0);
        #ifdef DEBUG
            // -------a variável stats_step recebera os valores
            // -------de cada thread
            stats_step.borns += param->stats_step.borns;
            stats_step.survivals += param->stats_step.survivals;
            stats_step.loneliness += param->stats_step.loneliness;
            stats_step.overcrowding += param->stats_step.overcrowding;
        #endif
        
        // serve como um wait, para que as threads
        // não prossigam para o próximo tabuleiro
        // até que todas tenham terminado
        FLAG_S--;

        if (!FLAG_S) {
            // a última thread a chegar
            // libera as outras
            FLAG_S = Nthreads;
            for (int t = 0; t < Nthreads; t++) {
                sem_post(&semaforo[t]);
            }
        }
        pthread_mutex_unlock(&mutex0);
        // trava todas as threads até a última
        sem_wait(&(semaforo[param->id]));

        #ifdef DEBUG
            // só a thread Nthreads-1
            if (param->id == Nthreads - 1) {
                printf("Step %d ----------\n", i + 1);
                print_board(param->prev, size);
                print_stats(stats_step);
                stats_step.borns = 0;
                stats_step.survivals = 0;
                stats_step.loneliness = 0;
                stats_step.overcrowding = 0;
                }
        #endif
    }
    sem_destroy(&semaforo[param->id]);
    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    
    if (argc != 3) {
                printf("ERRO! Você deve digitar %s"
                       " <nome do arquivo do tabuleiro>"
                       " <Nthreads>!\n\n",
        argv[0]);
        return 1;
    }
    FILE *f;
    if ((f = fopen(argv[1], "r")) == NULL) {
        printf("ERRO! O arquivo de tabuleiro '%s'"
               " não existe!\n\n",
               argv[1]);
        return 1;
    }

    Nthreads = atoi(argv[2]);

    // recebe os parâmetros size e steps do arquivo input
    fscanf(f, "%d %d", &size, &steps);

    // aloca só uma vez
    cell_t **prev, **next;
    prev = allocate_board(size);
    read_file(f, prev, size);
    fclose(f);

    next = allocate_board(size);
    // variável para o resultado final
    stats_t stats_total = {
      0,
      0,
      0,
      0};

    // para dividir o tabuleiro
    // entre as threads
    int aux = (size*size) / Nthreads,
        aux2 = 0, aux3 = (size*size) % Nthreads;
    printf("aux antes = %d\n", aux);
    printf("aux2 antes = %d\n", aux2);
    printf("aux3 antes = %d\n\n\n", aux3);
    if (!aux) {
        // mais threads que células
        // é um desperdício, nesse
        // caso, cada thread lida
        // com uma célula
        Nthreads = size*size;
        aux = 1; aux3 = 0;
    } else if (aux3) {
        // precisa arredondar
        // para cima
        aux2 = 1;
    }
    printf("aux = %d\n", aux);
    printf("aux2 = %d\n", aux2);
    printf("aux3 = %d\n\n\n", aux3);

    // controle de concorrência
    pthread_mutex_init(&mutex0, NULL);
    FLAG_S = Nthreads;
    semaforo = (sem_t*)malloc(sizeof(sem_t)*Nthreads);

    pthread_t Th[Nthreads];
    slice param[Nthreads];
#ifdef DEBUG
    printf("Initial:\n");
    print_board(prev, size);
    print_stats(stats_step);
#endif
     
    for (int i = 0; i < Nthreads; ++i) {
        param[i].id = i;

        // divisão dos slices
        if (i == aux3 && aux2) {

            aux2 = 0;
        } if (!i) {
            param[i].beg = 0;
            param[i].end = aux + aux2;
        } else {
            param[i].beg = param[i-1].end;
            param[i].end = param[i].beg + aux + aux2;
        }
        // recebendo estatísticas zeradas
        param[i].stats_step = stats_total;
        param[i].stats_total = stats_total;

        // os quadros que manipularão
        param[i].prev = prev;
        param[i].next = next;


        pthread_create(&Th[i], NULL,
                       jogar,
                       (void *)&param[i]);
    }
    // impedindo a thread main de continuar
    // até as trabalhadoras terminares
    for (int i = 0; i < Nthreads; ++i) {
        pthread_join(Th[i], NULL);
        // acumulando os resultados na variável
        stats_total.borns += param[i].stats_total.borns;
        stats_total.loneliness += param[i].stats_total.loneliness;
        stats_total.overcrowding += param[i].stats_total.overcrowding;
        stats_total.survivals += param[i].stats_total.survivals;
    }
    pthread_mutex_destroy(&mutex0);
    free(semaforo);
    
#ifdef RESULT
    printf("Final:\n");
    print_board(prev, size);
    print_stats(stats_total);
#endif
    
    free_board(prev, size);
    free_board(next, size);
    return 0;
}
