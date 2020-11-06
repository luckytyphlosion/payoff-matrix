#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

typedef unsigned long u64;
typedef unsigned int uint;

#define MAX_ROWS 20
#define MAX_COLS 20
#define TOTAL_TRIALS 100

static uint64_t rng_state[4];

struct splitmix64_state {
	uint64_t s;
};

uint64_t splitmix64(struct splitmix64_state *state) {
	uint64_t result = state->s;

	state->s = result + 0x9E3779B97f4A7C15;
	result = (result ^ (result >> 30)) * 0xBF58476D1CE4E5B9;
	result = (result ^ (result >> 27)) * 0x94D049BB133111EB;
	return result ^ (result >> 31);
}

// as an example; one could do this same thing for any of the other generators
void init_rng(uint64_t seed) {
	struct splitmix64_state smstate = {seed};

	uint64_t tmp = splitmix64(&smstate);
	rng_state[0] = (uint32_t)tmp;
	rng_state[1] = (uint32_t)(tmp >> 32);

	tmp = splitmix64(&smstate);
	rng_state[2] = (uint32_t)tmp;
	rng_state[3] = (uint32_t)(tmp >> 32);
}

static inline uint64_t rotl(const uint64_t x, int k) {
	return (x << k) | (x >> (64 - k));
}

uint64_t next(void) {
	const uint64_t result = rotl(rng_state[1] * 5, 7) * 9;

	const uint64_t t = rng_state[1] << 17;

	rng_state[2] ^= rng_state[0];
	rng_state[3] ^= rng_state[1];
	rng_state[1] ^= rng_state[2];
	rng_state[0] ^= rng_state[3];

	rng_state[2] ^= t;

	rng_state[3] = rotl(rng_state[3], 45);

    //printf("result: %u, result %% 3: %d\n", result, result % 3);
	return result & 0xffffffff;
}

int weighted_random(int len, int * weights) {
    int sum = 0;
    int random_value;

    for (int i = 0; i < len; i++) {
        sum += weights[i];
    }

    random_value = (next() % sum) + 1;
    for (int i = 0; i < len; i++) {
        if (weights[i] < random_value) {
            random_value -= weights[i];
        } else {
            return i;
        }
    }
}

typedef struct Matrix_s Matrix;

struct Matrix_s {
    int rows; // number of p1 choices
    int cols; // number of p2 choices
    int * p1_matrix;
    int * p2_matrix;
};

int p1_matrix[3][3] = {
    {1, -3, 7},
    {0, 2, 5},
    {5, 5, 3}
};

int p2_strategy_weights[] = {5, 0, 7};

/*int p2_matrix[3][3] = {
    {32767, 5, 5},
    {32767, 8, 0},
    {32767, 1, 7}
};*/

/*
int p1_matrix[3][3] = {
    {-3, -6, -6},
    {-1, -3, -7},
    {-2, 9, 1}
};

int p2_matrix[3][3] = {
    {3, 1, 2},
    {100, 3, -9},
    {6, 7, -1}
};*/

/*
int p1_matrix[6][4] = {
    {6, 8, -8, 6},
    {9, 10, 1, 9},
    {-10, 10, -2, 2},
    {3, 9, -9, -5},
    {2, -5, -7, -8},
    {-5, -6, 4, 1}
};

int p2_matrix[4][6] = {
    {-6, -9, 10, -3, -2, 5},
    {-10, -10, -10, -10, -10, -10},
    {-10, -10, -10, -10, -10, -10},
    {-10, -10, -10, -10, -10, -10}
};*/

Matrix matrix = {
    .rows = 3,
    .cols = 3,
    .p1_matrix = &p1_matrix[0][0],
    .p2_matrix = NULL,
};

void transpose_and_negate_p1_matrix_into_p2_matrix(Matrix * matrix) {
    int num_rows = matrix->rows;
    int num_cols = matrix->cols;
    int i, j;

    for (i = 0; i < num_rows; i++) {
        for (j = 0; j < num_cols; j++) {
            //printf("% 2d, % 2d\n", j * num_rows + i, i * num_cols + j);
            *(matrix->p2_matrix + j * num_rows + i) = -*(matrix->p1_matrix + i * num_cols + j);
        }
    }
    
    /*for (i = 0; i < num_cols; i++) {
        for (j = 0; j < num_rows; j++) {
            printf("%d, ", *(matrix->p2_matrix + i * num_rows + j));
        }
        printf("\n");
    }*/
}

// returns a random choice
void print_estimate_payoff_matrix(Matrix * matrix) {
    int num_rows = matrix->rows;
    int num_cols = matrix->cols;
    int p1_strategy = next() % num_rows;
    int p1_initial_strategy = p1_strategy;
    int p2_strategy;
    int p2_initial_strategy;
    int p1_strategies[MAX_ROWS] = {0};
    int p2_strategies[MAX_COLS] = {0};
    int p1_value_numerator = -32768;
    int p1_value_denominator = 1;
    int p2_value_numerator = -32768;
    int p2_value_denominator = 1;
    int p1_best_play_strategy_counts[MAX_ROWS];
    int p2_best_play_strategy_counts[MAX_COLS];
    int p1_weights[MAX_COLS] = {0};
    int p2_weights[MAX_ROWS] = {0};
    int cur_trial, i;
    int * row_or_col;
    int p1_payoff, p2_payoff;

    for (cur_trial = 1; cur_trial < (TOTAL_TRIALS + 1); cur_trial++) {
        p1_strategies[p1_strategy] += 1;
        row_or_col = matrix->p1_matrix + p1_strategy * num_cols;
        p2_payoff = 0x7fffffff;

        for (i = 0; i < num_cols; i++) {
            p1_weights[i] += row_or_col[i];
            if (p1_weights[i] < p2_payoff) {
                p2_payoff = p1_weights[i];
                p2_strategy = i;
            }
        }

        if (cur_trial == 1) {
            p2_initial_strategy = p2_strategy;
        }

        if (p2_payoff == 0x7fffffff) {
            abort();
        }

        // if ((p2_payoff / cur_trial) > (p1_value.numerator / p1_value.denominator))
        //printf("\np2_payoff: %d, p1_value_denominator: %d, p1_value_numerator: %d, cur_trial: %d\n", p2_payoff, p1_value_denominator, p1_value_numerator, cur_trial);
        if (cur_trial != 1 && ((p2_payoff * p1_value_denominator) >= (p1_value_numerator * cur_trial))) {
            p1_value_numerator = p2_payoff;
            p1_value_denominator = cur_trial;
            memcpy(p1_best_play_strategy_counts, p1_strategies, num_rows * sizeof(int));
        }

        printf("% 3d: % 4d |", cur_trial, p1_strategy + 1);
        for (i = 0; i < num_cols; i++) {
            printf(" % 4d |", p1_weights[i]);
        }
        printf(" % .4f || ", p2_payoff * 1.0 / cur_trial);

        p2_strategies[p2_strategy] += 1;
        row_or_col = matrix->p2_matrix + p2_strategy * num_rows;
        p1_payoff = 0x7fffffff;

        for (i = 0; i < num_rows; i++) {
            //printf("p2_weights[i] before: %d, ", p2_weights[i]);
            p2_weights[i] += row_or_col[i];
            //printf("p2_weights[i] after: %d\n", p2_weights[i]);
            if (p2_weights[i] < p1_payoff) {
                p1_payoff = p2_weights[i];
                p1_strategy = i;
            }
        }

        if (p1_payoff == 0x7fffffff) {
            abort();
        }

        //printf("\np1_payoff: %d, p2_value_denominator: %d, p2_value_numerator: %d, cur_trial: %d\n", p1_payoff, p2_value_denominator, p2_value_numerator, cur_trial);
        if (cur_trial != 1 && ((p1_payoff * p2_value_denominator) >= (p2_value_numerator * cur_trial))) {
            p2_value_numerator = p1_payoff;
            p2_value_denominator = cur_trial;
            memcpy(p2_best_play_strategy_counts, p2_strategies, num_cols * sizeof(int));
        }

        printf("% 4d |", p2_strategy + 1);
        for (i = 0; i < num_rows; i++) {
            printf(" % 4d |", p2_weights[i]);
        }
        printf(" % .4f\n", p1_payoff * 1.0 / cur_trial);
    }

    // remove p1's initial strategy since it could be faulty
    p1_best_play_strategy_counts[p1_initial_strategy]--;
    p1_value_denominator--;

    p2_best_play_strategy_counts[p2_initial_strategy]--;
    p2_value_denominator--;

    for (i = 0; i < num_rows; i++) {
        printf("%d/%d, ", p1_best_play_strategy_counts[i], p1_value_denominator);
    }
    printf("\n");

    for (i = 0; i < num_cols; i++) {
        printf("%d/%d, ", p2_best_play_strategy_counts[i], p2_value_denominator);
    }
    printf("\n");
}

// returns a random choice
void estimate_payoff_matrix_p2_weights(Matrix * matrix) {
    int num_rows = matrix->rows;
    int num_cols = matrix->cols;
    int p1_strategy = next() % num_rows;
    int p1_initial_strategy = p1_strategy;
    int p2_strategy;
    int p2_initial_strategy;
    int p1_strategies[MAX_ROWS] = {0};
    int p2_strategies[MAX_COLS] = {0};
    int p1_value_numerator = -32768;
    int p1_value_denominator = 1;
    int p2_value_numerator = -32768;
    int p2_value_denominator = 1;
    int p1_best_play_strategy_counts[MAX_ROWS];
    int p2_best_play_strategy_counts[MAX_COLS];
    int p1_weights[MAX_COLS] = {0};
    int p2_weights[MAX_ROWS] = {0};
    int cur_trial, i;
    int * row_or_col;
    int p1_payoff, p2_payoff;

    for (cur_trial = 1; cur_trial < (TOTAL_TRIALS + 1); cur_trial++) {
        p1_strategies[p1_strategy] += 1;
        row_or_col = matrix->p1_matrix + p1_strategy * num_cols;
        //p2_payoff = 0x7fffffff;

        for (i = 0; i < num_cols; i++) {
            p1_weights[i] += row_or_col[i];
            /*if (p1_weights[i] < p2_payoff) {
                p2_payoff = p1_weights[i];
                p2_strategy = i;
            }*/
        }

        p2_strategy = weighted_random(num_rows, p2_strategy_weights);
        p2_payoff = p1_weights[p2_strategy];

        if (cur_trial == 1) {
            p2_initial_strategy = p2_strategy;
        }

        if (p2_payoff == 0x7fffffff) {
            abort();
        }

        // if ((p2_payoff / cur_trial) > (p1_value.numerator / p1_value.denominator))
        if (cur_trial != 1 && ((p2_payoff * p1_value_denominator) >= (p1_value_numerator * cur_trial))) {
            p1_value_numerator = p2_payoff;
            p1_value_denominator = cur_trial;
            memcpy(p1_best_play_strategy_counts, p1_strategies, num_rows * sizeof(int));
        }

        printf("% 3d: % 4d |", cur_trial, p1_strategy + 1);
        for (i = 0; i < num_cols; i++) {
            printf(" % 4d |", p1_weights[i]);
        }
        printf(" % .4f || ", p2_payoff * 1.0 / cur_trial);

        p2_strategies[p2_strategy] += 1;
        row_or_col = matrix->p2_matrix + p2_strategy * num_rows;
        p1_payoff = 0x7fffffff;

        for (i = 0; i < num_rows; i++) {
            p2_weights[i] += row_or_col[i];
            if (p2_weights[i] < p1_payoff) {
                p1_payoff = p2_weights[i];
                p1_strategy = i;
            }
        }

        if (p1_payoff == 0x7fffffff) {
            abort();
        }

        //printf("\np1_payoff: %d, p2_value_denominator: %d, p2_value_numerator: %d, cur_trial: %d\n", p1_payoff, p2_value_denominator, p2_value_numerator, cur_trial);
        if (cur_trial != 1 && ((p1_payoff * p2_value_denominator) >= (p2_value_numerator * cur_trial))) {
            p2_value_numerator = p1_payoff;
            p2_value_denominator = cur_trial;
            memcpy(p2_best_play_strategy_counts, p2_strategies, num_cols * sizeof(int));
        }

        printf("% 4d |", p2_strategy + 1);
        for (i = 0; i < num_rows; i++) {
            printf(" % 4d |", p2_weights[i]);
        }
        printf(" % .4f\n", p1_payoff * 1.0 / cur_trial);
    }

    // remove p1's initial strategy since it could be faulty
    p1_best_play_strategy_counts[p1_initial_strategy]--;
    p1_value_denominator--;

    p2_best_play_strategy_counts[p2_initial_strategy]--;
    p2_value_denominator--;

    for (i = 0; i < num_rows; i++) {
        printf("%d/%d, ", p1_best_play_strategy_counts[i], p1_value_denominator);
    }
    printf("\n");

    for (i = 0; i < num_cols; i++) {
        printf("%d/%d, ", p2_best_play_strategy_counts[i], p2_value_denominator);
    }
    printf("\n");
}

/*
how does this work?
when deciding which move to use, the strategy is to come up with a table of all enemy actions vs all player actions:

         | move 1 | move 2 | switch 2 | switch 3
==============================================
move 1   |        |        |          |
move 2   |        |        |          |
switch 2 |        |        |          |
switch 3 |        |        |          |

left side corresponds to enemy's actions, top side corresponds to player's actions

what we do is for each cell, come up with a value that ranks how good of an outcome it is for the enemy, for the actions that are chosen.

for example, we could say that enemy move 1 vs player move 1 is very good, so we give it a rating of 10. note that ratings are relative.

once the entire table is filled, we pass the data to the function, which will output the action to take. this accounts for simultaneous turn decisions where there is no strictly best option (e.g. counter/mirror coat, can't always spam a physical move because of counter, can't always spam a special move because of mirror coat)
*/

int estimate_payoff_matrix(Matrix * matrix) {
    int num_rows = matrix->rows;
    int num_cols = matrix->cols;
    int p1_strategy = next() % num_rows;
    int p1_initial_strategy = p1_strategy;
    int p2_strategy;
    int p1_strategies[MAX_ROWS] = {0};
    int p1_value_numerator = -32768;
    int p1_value_denominator = 1;
    int p1_best_play_strategy_counts[MAX_ROWS];
    int p1_weights[MAX_COLS] = {0};
    int p2_weights[MAX_ROWS] = {0};
    int cur_trial, i;
    int * row_or_col;
    int p1_payoff, p2_payoff;
    int random_value;

    for (cur_trial = 1; cur_trial < (TOTAL_TRIALS + 1); cur_trial++) {
        p1_strategies[p1_strategy] += 1;
        row_or_col = matrix->p1_matrix + p1_strategy * num_cols;
        p2_payoff = 0x7fffffff;

        for (i = 0; i < num_cols; i++) {
            p1_weights[i] += row_or_col[i];
            if (p1_weights[i] < p2_payoff) {
                p2_payoff = p1_weights[i];
                p2_strategy = i;
            }
        }

        if (p2_payoff == 0x7fffffff) {
            abort();
        }

        //printf("p2_payoff: %d, p1_value_denominator: %d, p1_value_numerator: %d, cur_trial: %d\n", p2_payoff, p1_value_denominator, p1_value_numerator, cur_trial);
        // if ((p2_payoff / cur_trial) > (p1_value.numerator / p1_value.denominator))
        if ((p2_payoff * p1_value_denominator) >= (p1_value_numerator * cur_trial)) {
            p1_value_numerator = p2_payoff;
            p1_value_denominator = cur_trial;
            memcpy(p1_best_play_strategy_counts, p1_strategies, num_rows * sizeof(int));
        }

        printf("% 3d: % 4d |", cur_trial, p1_strategy + 1);
        for (i = 0; i < num_cols; i++) {
            printf(" % 4d |", p1_weights[i]);
        }
        printf(" % .4f\n", p2_payoff * 1.0 / cur_trial);

        row_or_col = matrix->p2_matrix + p2_strategy * num_rows;
        p1_payoff = 0x7fffffff;

        for (i = 0; i < num_rows; i++) {
            p2_weights[i] += row_or_col[i];
            if (p2_weights[i] < p1_payoff) {
                p1_payoff = p2_weights[i];
                p1_strategy = i;
            }
        }
    }

    // remove p1's initial strategy since it could be faulty
    p1_best_play_strategy_counts[p1_initial_strategy]--;
    p1_value_denominator--;

    random_value = (next() % p1_value_denominator) + 1;

    for (i = 0; i < num_rows; i++) {
        printf("%d/%d, ", p1_best_play_strategy_counts[i], p1_value_denominator);
    }
    printf("\n");

    for (i = 0; i < num_rows; i++) {
        if (p1_best_play_strategy_counts[i] < random_value) {
            random_value -= p1_best_play_strategy_counts[i];
        } else {
            break;
        }
    }

    printf("chose: %d\n", i);

    return i;
}

int main(void) {
    init_rng(time(NULL));
    if (matrix.p2_matrix == NULL) {
        matrix.p2_matrix = malloc(matrix.rows * matrix.cols * sizeof(int));
        transpose_and_negate_p1_matrix_into_p2_matrix(&matrix);
    }
    estimate_payoff_matrix_p2_weights(&matrix);
    //print_estimate_payoff_matrix(&matrix);
    free(matrix.p2_matrix);
}
