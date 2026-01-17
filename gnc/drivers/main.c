#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "matrix.c"

extern int
main(void)
{

    struct Matrix* gain =
        Matrix
        (
            3, 6,
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

    MATRIX_print(control_output);



    return 0;

}
