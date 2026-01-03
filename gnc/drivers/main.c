// C program to multiply two matrices
#include <stdio.h>
#include <stdlib.h>
#include "matrix_operations.c"

// Driver code
int main()
{
    // R1 = 4, C1 = 4 and R2 = 4, C2 = 4 (Update these
    // values in MACROs)

    int gain[R1][C1] = { { 1, 0, 0, 1, 0, 0 }, { 0, 1, 0, 0, 1, 0 }, { 0, 0, 1, 0, 0, 1 } };
    int state[R2][C2] = { { 1 }, { 1 }, { 1 }, { 1 }, { 1 }, { 1 } };
    int zeros [R1][C2] = { { 0 }, { 0 }, { 0 } };
    int control_output[R1][C2];

    // if coloumn of m1 not equal to rows of m2
    if (C1 != R2) {
        printf("The number of columns in Matrix-1 must be "
               "equal to the number of rows in "
               "Matrix-2\n");
        printf("Please update MACROs value according to "
               "your array dimension in "
               "#define section\n");

        exit(EXIT_FAILURE);
    }

    // Function call
    matrix_multiply(gain, state, control_output);
    matrix_subtract(zeros,control_output,control_output);

    printf("Resultant Matrix is:\n");
    for (int i = 0; i < R1; i++) {
        for (int j = 0; j < C2; j++) {
            printf("%d\t", control_output[i][j]);
        }
        printf("\n");
    }

    return 0;
}
