//#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "gol.h"

// transformadas em variáveis globais
// para permitir acesso pelas threads
FILE *f;
int size, steps, Nthreads;
#ifdef DEBUG
    stats_t stats_step = {0, 0, 0, 0};
    pthread_mutex_t mutexDEBUG;
#endif
// controle de concorrência
int FLAG_S;
pthread_mutex_t mutex0;
sem_t sem0, sem1;

// Função que as threads executam,
// retirada da main
void* jogar(void *arg) {
    slice *param = (slice *)arg;
    // leitura do arquivo é paralelizada também
    read_file(f, param->prev, size,
              param->beg, param->end);
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
        printf("Thread %d jogou %d rodada(s)\n", param->id, i+1);
        // cada thread também tem suas próprias
        // estatísticas totais e por step
        param->stats_total.borns += param->stats_step.borns;
        param->stats_total.survivals += param->stats_step.survivals;
        param->stats_total.loneliness += param->stats_step.loneliness;
        param->stats_total.overcrowding += param->stats_step.overcrowding;

        #ifdef DEBUG
            // -------caso se deseje ver o passo a passo,
            // -------a variável stats_step receberão os valores
            // -------de cada thread
            pthread_mutex_lock(&mutexDEBUG);
            stats_step.borns += param->stats_step.borns;
            stats_step.survivals += param->stats_step.survivals;
            stats_step.loneliness += param->stats_step.loneliness;
            stats_step.overcrowding += param->stats_step.overcrowding;
            pthread_mutex_unlock(&mutexDEBUG);
        #endif
        
        // alterando variáveis globais
        // em uma região de exclusão mútua
        pthread_mutex_lock(&mutex0);
        printf("Thread %d entrou na região crítica\n", param->id);
        // serve como um wait, para que as threads
        // não prossigam para o próximo tabuleiro
        // até que todas tenham terminado
        FLAG_S--;
        if (!FLAG_S) {
          // a última thread a chegar é a única
          // que libera o próprio caminho
            printf("Thread %d chegou por último\n", param->id);
            sem_post(&(param->semI));
            FLAG_S = Nthreads;
        }
        pthread_mutex_unlock(&mutex0);

        // trava todas as threads até a última
        printf("Thread %d vai travar\n", param->id);
        sem_wait(&(param->semI));
        printf("Thread %d foi liberada\n", param->id);
        tmp = param->next;
        param->next = param->prev;
        param->prev = tmp;
        // cada thread libera a vizinha
        sem_post(&(param->semD));
        printf("Thread %d liberou %d\n", param->id, (param->id+1)%Nthreads);

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
    
    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    
    if (argc != 3) {
printf("ERRO! Você deve digitar %s <nome do arquivo do tabuleiro> <Nthreads>!\n\n",
        argv[0]);
        return 1;
    }

    if ((f = fopen(argv[1], "r")) == NULL) {
        printf("ERRO! O arquivo de tabuleiro '%s' não existe!\n\n",
               argv[1]);
        return 1;
    }

    Nthreads = atoi(argv[2]);

    // recebe os parâmetros size e steps do arquivo input
    fscanf(f, "%d %d", &size, &steps);
#ifdef DEBUG
    pthread_mutex_init(&mutexDEBUG, NULL);
#endif

    /* read the first new line (it will be ignored) */
    char *s = (char *)malloc(size + 10);
    fgets(s, size + 10, f);
    free(s);

    // aloca só uma vez
    cell_t **prev, **next;
    prev = allocate_board(size);
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
    //elimina as threads desnecessárias
    while (aux * (Nthreads - 1) >= size) {
        Nthreads--;
    }

    // controle de concorrência
    pthread_mutex_init(&mutex0, NULL);
    FLAG_S = Nthreads;
    sem_t semaforo[Nthreads];
    // para evitar dupla inicialização
    sem_init(&semaforo[0], 0, 0);

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

        param[i].semI = semaforo[i];
        // todas menos a última inicializam o semaforo
        // da vizinha, porque o semaforo 0 já o foi
        if (i + 1 < Nthreads) {
            sem_init(&semaforo[i + 1], 0, 0);
        }
        param[i].semD = semaforo[(i + 1) % Nthreads];

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
    for (int i = 0; i < Nthreads; ++i) {
        sem_destroy( &semaforo[i]);
    }
    // na versão paralela,  o arquivo
    // fica aberto durante o jogo todo
    fclose(f);
    
#ifdef RESULT
    printf("Final:\n");
    print_board(prev, size);
    print_stats(stats_total);
#endif
    
    free_board(prev, size);
    free_board(next, size);
    return 0;
}
