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
#ifdef DEBUG
    // ---- só a thread 0
    if (!param->id) {
        printf("Initial:\n");
        print_board(param->prev, size);
        print_stats(stats_step);
    }
#endif
    cell_t** tmp;

    for (int i = 0; i < steps; i++) {
        // cada thread lidará com um slice do tabuleiro,
        // recebendo as próprias estatísticas
        param->stats_step = play(param->prev, param->next,
                                 size, param->beg, param->end);
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
            // -------caso se deseje ver o passo a passo,
            // -------a variável stats_step receberá os
            // -------valores de cada thread
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
            // só a thread 0
            if (!param->id) {
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
                       " <nome do arquivo do tabuleiro> "
                       "<Nthreads>!\n\n",
        argv[0]);
        return 1;
    }
    FILE *f;
    if ((f = fopen(argv[1], "r")) == NULL) {
        printf("ERRO! O arquivo de tabuleiro '%s' não existe!\n\n",
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
    int aux = size / Nthreads;
    if (Nthreads >= size) {
        // mais threads que linhas
        // é um desperdício
        // nesse caso cada thread
        // lida com uma coluna
        Nthreads = size;
        aux = 1;
    } else if (size % Nthreads) {
        // precisa arredondar
        // para cima
        aux++;
    }
    // elimina as threads desnecessárias
    while (aux * (Nthreads - 1) >= size) {
        Nthreads--;
    }

    // controle de concorrência
    pthread_mutex_init(&mutex0, NULL);
    FLAG_S = Nthreads;
    semaforo = (sem_t*)malloc(sizeof(sem_t)*Nthreads);
    // para evitar dupla inicialização

    pthread_t Th[Nthreads];
    slice param[Nthreads];
     
    for (int i = 0; i < Nthreads; ++i) {
        param[i].id = i;

        // divisão dos slices
        param[i].beg = aux*i;
        if (aux*(i+1) > size) {
            param[i].end = size;
        } else {
            param[i].end = aux * (i + 1);
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
    // usa outro for para garantir que
    // não interfira com a execução
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
