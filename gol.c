/*
 * The Game of Life
 *
 * RULES:
 *  1. A cell is born, if it has exactly three neighbours.
 *  2. A cell dies of loneliness,
 *     if it has less than two neighbours.
 *  3. A cell dies of overcrowding,
 *     if it has more than three neighbours.
 *  4. A cell survives to the next generation,
 *     if it does not die of lonelines or overcrowding.
 *
 * In this version, a 2D array of ints is used.
 * A 1 cell is on, a 0 cell is off.
 * The game plays a number of steps (given by the input),
 * printing to the screen each time.
 * A 'x' printed means on, space means off.
 *
 */

#include <stdlib.h>
#include "gol.h"

cell_t **allocate_board(int size) {
    cell_t **board = (cell_t **)malloc(sizeof(cell_t *) * size);
    for (int i = 0; i < size; i++) {
        board[i] = (cell_t *)malloc(sizeof(cell_t) * size);
    }

    return board;
}

void free_board(cell_t **board, int size) {
    for (int i = 0; i < size; i++) {
        free(board[i]);
    }
    free(board);
}

int adjacent_to(cell_t **board, int size, int i, int j) {
    int k, l, count = 0;

    int sk = (i > 0) ? i - 1 : i;
    int ek = (i + 1 < size) ? i + 1 : i;
    int sl = (j > 0) ? j - 1 : j;
    int el = (j + 1 < size) ? j + 1 : j;

    for (k = sk; k <= ek; k++) {
        for (l = sl; l <= el; l++) {
            count += board[k][l];
        }
    }
    count -= board[i][j];

    return count;
}

// alterada para receber o slice que cada thread vai usar
void play(cell_t **board, cell_t **newboard,
             int size, int linhaI, int linhaF,
             int colunaI, int colunaF, stats_t *stats) {
    int i = linhaI, j = colunaI, a, b, end;
    int vet[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    /* for each cell, apply the rules of Life */
    for (; i < linhaF; i++) {
        end = (i == (linhaF - 1))? colunaF : size;
        for (; j < end; j++) {
            a = adjacent_to(board, size, i, j);
            // se a célula estiver viva,
            // os índices de 2 para cima
            // são incrementados
            b = board[i][j]*(a+2);
            // caso contrário, b é 0;
            // o índice 1 deve ser
            // incrementado se a == 3
            // se não, será índice 0
            // (que não será usado, 
            // mas representa célula que
            // continua morta)
            b -= (board[i][j] - 1)*(a==3);
            vet[b]++;
            newboard[i][j] = (b == 1) + (b == 4) + (b == 5);
        }
        j = 0;
    }
    vet[6] += vet[7] + vet[8] + vet[9] + vet[10];
    stats->overcrowding = vet[6];
    stats->loneliness = vet[2] + vet[3];
    stats->survivals = vet[4] + vet[5];
    stats->borns = vet[1];
}

void print_board(cell_t **board, int size) {
    /* for each row */
    for (int j = 0; j < size; j++) {
        // print each column position...
        for (int i = 0; i < size; i++) {
            printf("%c", board[i][j] ? 'x' : ' ');
        }
        // followed by a carriage return
        printf("\n");
    }
}

void print_stats(stats_t stats) {
    // print final statistics
    printf("Statistics:\n\tBorns..............: "
            "%u\n\tSurvivals..........: "
            "%u\n\tLoneliness deaths..: "
            "%u\n\tOvercrowding deaths: %u\n\n",
            stats.borns, stats.survivals,
            stats.loneliness, stats.overcrowding);
}

void read_file(FILE *f, cell_t **board, int size) {
    char *s = (char *) malloc(size + 10);

    // read the first new line (it will be ignored)
    fgets(s, size + 10, f);

    /* read the life board */
    for (int j = 0; j < size; j++) {
        // get a string
        fgets(s, size + 10, f);

        // copy the string to the life board
        for (int i = 0; i < size; i++) {
            board[i][j] = (s[i] == 'x');
        }
    }

    free(s);
}
