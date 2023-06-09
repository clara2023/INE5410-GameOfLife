#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "gol.h"

// controle de concorrência
int FLAG_S;
pthread_mutex_t mutex0;
sem_t *semaforo;
#ifdef DEBUG
    stats_t stats_step = {0,0,0,0};
#endif

// Função que as threads executam,
// retirada da main
void* jogar(void *arg) {
    slice *param = (slice *)arg;
    // cada thread inicializando
    // o próprio semáforo
    sem_init(&semaforo[param->id], 0, 0);

    int colunaI = (param->colunas_por_thread)*(param->id) + param->resto_cel;
    int colunaF = colunaI + (param->colunas_por_thread);
    if (param->resto_cel && param->id < param->resto_cel) {
        colunaI += (param->id - param->resto_cel);
        colunaF += (param->id - param->resto_cel) + 1;
    }
    int linhaI = (param->linhas_por_thread - 1)*(param->id); 
    int linhaF = linhaI + param->linhas_por_thread + colunaF/(param->size);
    linhaI += colunaI/(param->size);
    colunaI %= param->size;
    colunaF %= param->size;
    if (!colunaF) {
        colunaF = param->size;
        linhaF--;
    }

    cell_t** tmp;

    for (int step = 1; step <= param->steps; step++) {
        // cada thread lidará com um slice do tabuleiro,
        // recebendo as próprias estatísticas
        play(param->prev, param->next, param->size,
             linhaI, linhaF,
             colunaI, colunaF,
             &(param->stats_step));
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
        
        // serve como um wait, para que as threads
        // não prossigam para o próximo tabuleiro
        // até que todas tenham terminado
        FLAG_S--;
        #ifdef DEBUG
            stats_step.borns += param->stats_step.borns;
            stats_step.survivals += param->stats_step.survivals;
            stats_step.loneliness += param->stats_step.loneliness;
            stats_step.overcrowding += param->stats_step.overcrowding;
        #endif

        if (!FLAG_S) {
            // a última thread a chegar
            // libera as outras
            FLAG_S = param->Nthreads;
            #ifdef DEBUG
                printf("Step %d ----------\n", step);
                print_board(param->prev, param->size);
                print_stats(stats_step);
                stats_step.borns = 0;
                stats_step.survivals = 0;
                stats_step.loneliness = 0;
                stats_step.overcrowding = 0;
            #endif
            for (int i = 0; i < param->Nthreads; i++) {
                sem_post(&semaforo[i]);
            }
        }
        pthread_mutex_unlock(&mutex0);
        // trava todas as threads até a última
        sem_wait(&(semaforo[param->id]));
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
    int Nthreads, size, steps;

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
    if (Nthreads > size*size) {
        // mais threads que células
        // é um desperdício, nesse
        // caso, cada thread lida
        // com uma célula
        Nthreads = size*size;
    }
    int cel_por_thread = (size * size) / Nthreads;
    int resto_cel = (size * size) % Nthreads;
    int linhas_por_thread = cel_por_thread / size;
    int colunas_por_thread = cel_por_thread % size;
    if (!colunas_por_thread) {
        colunas_por_thread = size;
    } else {
        linhas_por_thread++;
    }

    // controle de concorrência
    pthread_mutex_init(&mutex0, NULL);
    FLAG_S = Nthreads;
    semaforo = (sem_t*)malloc(sizeof(sem_t)*Nthreads);

    pthread_t Th[Nthreads];
    slice param[Nthreads];

    for (int i = 0; i < Nthreads; ++i) {
        param[i].id = i;

        param[i].size = size;
        param[i].steps = steps;
        param[i].Nthreads = Nthreads;
        param[i].resto_cel = resto_cel;
        param[i].linhas_por_thread = linhas_por_thread;
        param[i].colunas_por_thread = colunas_por_thread;
        
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
    //}
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

