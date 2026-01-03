// matrix dimensions so that we dont have to pass them as
// parametersmat1[R1][C1] and mat2[R2][C2]
#define R1 3 // number of rows in matrix 1
#define C1 6 // number of columns in matrix 1
#define R2 6 // number of rows in matrix 2
#define C2 1 // number of columns in matrix 2

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
