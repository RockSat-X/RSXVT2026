#include <stdio.h>
#include "matrix_operations.h"

void matrix_multiply(int m1[R1][C1], int m2[R1][C2], int result[R1][C2] )
{
    for (int i = 0; i < R1; i++) {
        for (int j = 0; j < C2; j++) {
            result[i][j] = 0;

            for (int k = 0; k < R2; k++) {
                result[i][j] += m1[i][k] * m2[k][j];
            }
        }
    }
}

void matrix_add(int m1[][C2], int m2[][C2], int result[][C2] )
{
    for (int i = 0; i < R1; i++) {
        for (int j = 0; j < C2; j++) {
            result[i][j] = m1[i][j] + m2[i][j];
        }
    }
}


void matrix_subtract(int m1[][C2], int m2[][C2], int result[][C2] )
{
    for (int i = 0; i < R1; i++) {
        for (int j = 0; j < C2; j++) {
            result[i][j] = m1[i][j] - m2[i][j];
        }
    }
}