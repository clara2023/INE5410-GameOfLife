//#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "gol.h"

FILE *f;
int size, steps;

void* jogar(void *arg) {
    slice param = *(slice *)arg;
    
    read_file(f, param.prev, size, param.beg, param.end);
    fclose(f);    

#ifdef DEBUG
    printf("Initial:\n");
    print_board(param.prev, size);
    print_stats(param.stats_step);
#endif

    cell_t** tmp;

    for (int i = 0; i < steps; i++) {
        param.stats_step = play(param.prev, param.next, size, param.beg, param.end);
        
        param.stats_total.borns += param.stats_step.borns;
        param.stats_total.survivals += param.stats_step.survivals;
        param.stats_total.loneliness += param.stats_step.loneliness;
        param.stats_total.overcrowding += param.stats_step.overcrowding;

        tmp = param.next;
        param.next = param.prev;
        param.prev = tmp;
#ifdef DEBUG
        printf("Step %d ----------\n", i + 1);
        print_board(tmp, size);
        print_stats(param.stats_step);
#endif
    }
    
    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    
    if (argc != 3) {
        printf("ERRO! Você deve digitar %s <nome do arquivo do tabuleiro> <Nthreads>!\n\n", argv[0]);
        return 0;
    }

    if ((f = fopen(argv[1], "r")) == NULL) {
        printf("ERRO! O arquivo de tabuleiro '%s' não existe!\n\n", argv[1]);
        return 0;
    }

    int Nthreads = atoi(argv[2]);

    fscanf(f, "%d %d", &size, &steps);

    /* read the first new line (it will be ignored) */
    char *s = (char *)malloc(size + 10);
    fgets(s, size + 10, f);
    free(s);

    cell_t **prev, **next;
    prev = allocate_board(size);
    next = allocate_board(size);
    
    stats_t stats_step = {0, 0, 0, 0};
    stats_t stats_total = {0, 0, 0, 0};

    pthread_t Th[Nthreads];
    slice param[Nthreads];
    
    int aux = size / Nthreads;
    if (size % Nthreads) {
        aux++;
    }
     
    for (int i = 0; i < Nthreads; ++i) {
        param[i].id = i;

        param[i].beg = aux*i;
        param[i].end = aux*(i + 1);
        if (param[i].beg >= size) {
            param[i].beg = size;
            param[i].end = size;
        } else if (param[i].end > size) {
            param[i].end = size;
        }

        param[i].stats_step = stats_step;
        param[i].stats_total = stats_total;

        param[i].prev = prev;
        param[i].next = next;

        pthread_create(&Th[i], NULL, jogar, (void *)&param[i]);
    }
    for (int i = 0; i < Nthreads; ++i) {
        pthread_join(Th[i], NULL);
        stats_total.borns += param[i].stats_total.borns;
        stats_total.loneliness += param[i].stats_total.loneliness;
        stats_total.overcrowding += param[i].stats_total.overcrowding;
        stats_total.survivals += param[i].stats_total.survivals;
    }
    
    free_board(prev, size);
    free_board(next, size);
    
#ifdef RESULT
    printf("Final:\n");
    print_board(prev, size);
    print_stats(param.stats_total);
#endif
    return 0;
}
