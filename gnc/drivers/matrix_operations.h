#ifndef MATRIX_OPERATIONS_H
#define MATRIX_OPERATIONS_H

// matrix dimensions so that we dont have to pass them as
// parametersmat1[R1][C1] and mat2[R2][C2]
#define R1 3 // number of rows in matrix 1
#define C1 6 // number of columns in matrix 1
#define R2 6 // number of rows in matrix 2
#define C2 1 // number of columns in matrix 2



// Function prototype
void matrix_multiply(int m1[][C1], int m2[][C2], int result[][C2] );
void matrix_add(int m1[][C2], int m2[][C2], int result[][C2] );
void matrix_subtract(int m1[][C2], int m2[][C2], int result[][C2] );

#endif