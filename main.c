//#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "gol.h"

FILE *f;
int size, steps, flag0 = 1, flag1 = 1;
stats_t stats_step = {0, 0, 0, 0};
stats_t stats_total = {0, 0, 0, 0};

void* jogar(void *arg) {
    slice param = *(slice *)arg;

    char *s = (char *)malloc(size + 10);
    cell_t **prev, **next, **tmp;

    /* read the first new line (it will be ignored) */
    /* evita que várias alocações aconteçam*/
    if (flag0) {
        flag0 = 0;
        fgets(s, size + 10, f);
        prev = allocate_board(size);
        next = allocate_board(size);
    }
    read_file(f, prev, size, s, param.beg, param.end);
    fclose(f);
    

#ifdef DEBUG
    printf("Initial:\n");
    print_board(prev, size);
    print_stats(stats_step);
#endif

    for (int i = 0; i < steps; i++) {
        param.stats_step = play(prev, next, size, param.beg, param.end);
        
        param.stats_total.borns += param.stats_step.borns;
        param.stats_total.survivals += param.stats_step.survivals;
        param.stats_total.loneliness += param.stats_step.loneliness;
        param.stats_total.overcrowding += param.stats_step.overcrowding;

#ifdef DEBUG
        printf("Step %d ----------\n", i + 1);
        print_board(next, size);
        print_stats(param.stats_step);
#endif
        tmp = next;
        next = prev;
        prev = tmp;
    }

    
    
#ifdef RESULT
    printf("Final:\n");
    print_board(prev, size);
    print_stats(param.stats_total);
#endif

    free_board(prev, size);
    free_board(next, size);

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
        pthread_create(&Th[i], NULL, jogar, (void *)&param[i]);
    }
    for (int i = 0; i < Nthreads; ++i) {
        pthread_join(Th[i], NULL);
    }
    return 0;
}
