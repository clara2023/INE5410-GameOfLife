//#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "gol.h"

int main(int argc, char **argv) {
    
    if (argc != 3) {
printf("ERRO! Você deve digitar %s <nome do arquivo do tabuleiro> <Nthreads>!\n\n",
        argv[0]);
        return 1;
    }
    FILE *f;
    if ((f = fopen(argv[1], "r")) == NULL) {
        printf("ERRO! O arquivo de tabuleiro '%s' não existe!\n\n",
               argv[1]);
        return 1;
    }
    int size, steps, Nthreads;

    // recebe os parâmetros size e steps do arquivo input
    fscanf(f, "%d %d", &size, &steps);

    Nthreads = atoi(argv[2]);

    /* read the first new line (it will be ignored) */
    char *s = (char *)malloc(size + 10);
    fgets(s, size + 10, f);
    free(s);

    // aloca só uma vez
    cell_t **prev;
    prev = allocate_board(size);

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

    //----------- leitura paralela do arquivo
    
    pthread_t T_read[Nthreads];
    leitura argu[Nthreads];
    slice param[Nthreads];

    for (int i = 0; i < Nthreads; ++i) {
        argu[i].size = param[i].size = size;
        // divisão dos slices
        argu[i].begin = param[i].beg = aux*i;
        if (aux*(i+1) > size) {
            argu[i].end = param[i].end = size;
        } else {
            argu[i].end = param[i].end = aux * (i + 1);
        }

        // o quadros para onde lerão
        argu[i].board = prev;

        // arquivo
        argu[i].file = f;

        pthread_create(&T_read[i], NULL,
                       read_file,
                       (void *)&argu[i]);
    }
    // impedindo a thread main de continuar
    // até as trabalhadoras terminarem de ler
    for (int i = 0; i < Nthreads; ++i) {
        pthread_join(T_read[i], NULL);
    }
    fclose(f);

    //--------------------- JOGO paralelo ---------------------
    cell_t **next, **tmp;
    next = allocate_board(size);
    // variável para o resultado final
    stats_t stats_total = {
      0,
      0,
      0,
      0
      };
#ifdef DEBUG
    stats_t stats_step = stats_total;
    printf("Initial:\n");
    print_board(prev, size);
    print_stats(stats_step);
    
#endif
    pthread_t Th[Nthreads];

    for (int j = 0; j < steps; ++j) {
        for (int i = 0; i < Nthreads; ++i) {
            // os quadros que manipularão
            param[i].prev = prev;
            param[i].next = next;
            // recebendo estatísticas zeradas
            param[i].stats.borns = 0;
            param[i].stats.survivals = 0;
            param[i].stats.loneliness = 0;
            param[i].stats.overcrowding = 0;

            pthread_create(&Th[i], NULL,
                        play,
                        (void *)&param[i]);
        }
        // impedindo o jogo de continuar
        // até as trabalhadoras terminarem
        for (int i = 0; i < Nthreads; ++i) {
            pthread_join(Th[i], NULL);
            // acumulando os resultados na variável
            stats_total.borns += param[i].stats.borns;
            stats_total.loneliness += param[i].stats.loneliness;
            stats_total.overcrowding += param[i].stats.overcrowding;
            stats_total.survivals += param[i].stats.survivals;
            #ifdef DEBUG
                // -------caso se deseje ver o passo a passo,
                // -------a variável stats_step receberão os valores
                // -------de cada thread
                stats_step.borns += param[i].stats.borns;
                stats_step.loneliness += param[i].stats.loneliness;
                stats_step.overcrowding += param[i].stats.overcrowding;
                stats_step.survivals += param[i].stats.survivals;
            #endif
        }
        #ifdef DEBUG
            printf("Step %d ----------\n", j + 1);
            print_board(prev, size);
            print_stats(stats_step);
        #endif
        
        tmp = next;
        next = prev;
        prev = tmp;
    }
    
#ifdef RESULT
    printf("Final:\n");
    print_board(prev, size);
    print_stats(stats_total);
#endif
    
    free_board(prev, size);
    free_board(next, size);
    return 0;
}
