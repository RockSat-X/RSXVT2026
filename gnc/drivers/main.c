#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "matrix_operations.c"

int main()
{

    struct Matrix* gain =
        Matrix
        (
            1, 6,
            1, 0, 0, 1, 0, 0,
            0, 1, 0, 0, 1, 0,
            0, 0, 1, 0, 0, 1,
        );

    struct Matrix* state =
        Matrix
        (
            6, 1,
            1,
            1,
            1,
            1,
            1,
            1,
        );

    struct Matrix* control_output = Matrix(3, 1);



    MATRIX_multiply(control_output, gain, state);

    MATRIX_multiply_add(control_output, control_output, -2);



    printf("Resultant Matrix is:\n");

    for (int i = 0; i < control_output->rows; i += 1)
    {
        for (int j = 0; j < control_output->columns; j += 1)
        {
            printf("%d\t", control_output->values[i * control_output->columns + j]);
        }
        printf("\n");
    }



    return 0;

}
