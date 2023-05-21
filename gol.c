/*
 * The Game of Life
 *
 * RULES:
 *  1. A cell is born, if it has exactly three neighbours.
 *  2. A cell dies of loneliness, if it has less than two neighbours.
 *  3. A cell dies of overcrowding, if it has more than three neighbours.
 *  4. A cell survives to the next generation, if it does not die of lonelines or overcrowding.
 *
 * In this version, a 2D array of ints is used.  A 1 cell is on, a 0 cell is off.
 * The game plays a number of steps (given by the input), printing to the screen each time.
 * A 'x' printed means on, space means off.
 *
 */

#include <pthread.h>
#include <stdlib.h>
#include "gol.h"

cell_t **allocate_board(int size)
{
    cell_t **board = (cell_t **)malloc(sizeof(cell_t *) * size);
    int i;
    for (i = 0; i < size; i++)
        board[i] = (cell_t *)malloc(sizeof(cell_t) * size);

    return board;
}

void free_board(cell_t **board, int size)
{
    int i;
    for (i = 0; i < size; i++)
        free(board[i]);
    free(board);
}

int adjacent_to(cell_t **board, int size, int i, int j)
{
    int k, l, count = 0;

    int sk = (i > 0) ? i - 1 : i;
    int ek = (i + 1 < size) ? i + 1 : i;
    int sl = (j > 0) ? j - 1 : j;
    int el = (j + 1 < size) ? j + 1 : j;

    for (k = sk; k <= ek; k++)
        for (l = sl; l <= el; l++)
            count += board[k][l];
    count -= board[i][j];

    return count;
}

// alterada para receber o slice que cada thread vai usar
void* play(void* arg) {
    slice *param = (slice *)arg;
    int i, j, a;

    /* for each cell, apply the rules of Life */
    for (i = 0; i < param->size; i++) {
        // só um dos for's é dividido,
        // caso contrário haveriam
        // regiões não varridas
        for (j = param->beg; j < param->end; j++) {
            a = adjacent_to(param->prev, param->size, i, j);

            /* if cell is alive */
            if(param->prev[i][j]) {
                /* death: loneliness */
                if(a < 2) {
                    param->next[i][j] = 0;
                    param->stats.loneliness++;
                } else if(a > 3) { /* death: overcrowding */
                        param->next[i][j] = 0;
                        param->stats.overcrowding++;
                } else { /* survival */
                    param->next[i][j] = param->prev[i][j];
                    param->stats.survivals++;
                }
            } else { /* if cell is dead */
                if(a == 3) { /* new born */
                    param->next[i][j] = 1;
                    param->stats.borns++;
                } else { /* stay unchanged */
                    param->next[i][j] = param->prev[i][j];
                }
            }
        }
    }

    pthread_exit(NULL);
}

void print_board(cell_t **board, int size)
{
    int i, j;
    /* for each row */
    for (j = 0; j < size; j++)
    {
        /* print each column position... */
        for (i = 0; i < size; i++)
            printf("%c", board[i][j] ? 'x' : ' ');
        /* followed by a carriage return */
        printf("\n");
    }
}

void print_stats(stats_t stats)
{
    /* print final statistics */
    printf("Statistics:\n\tBorns..............: %u\n\tSurvivals..........: %u\n\tLoneliness deaths..: %u\n\tOvercrowding deaths: %u\n\n",
        stats.borns, stats.survivals, stats.loneliness, stats.overcrowding);
}
// também dividida em slices
void *read_file(void *arg) {
    leitura *param = (leitura*)arg;
    
    // alterada para não ler a primeira linha
    char *s = (char *)malloc(param->size + 10);
    
    /* read the life board */
    for (int j = param->begin; j < param->end; j++) {
        /* get a string */
        fgets(s, param->size + 10, param->file);

        /* copy the string to the life board */
        // não é dividida em slices
        for (int i = 0; i < param->size; i++)
            param->board[i][j] = (s[i] == 'x');
    }

    free(s);
    pthread_exit(NULL);
}
