////////////////////////////////////////////////////////////////////////////////
//
// Matrix stuff. @/`Using Matrices`.
//



#define Matrix(ROWS, COLUMNS, VALUES...)  \
    (                                     \
        (struct Matrix*)                  \
        (f32[2 + (ROWS) * (COLUMNS)])     \
        {                                 \
            *(f32*) &(i32) { (ROWS   ) }, \
            *(f32*) &(i32) { (COLUMNS) }, \
            VALUES                        \
        }                                 \
    )

#define AT(MATRIX, ROW, COLUMN) \
    (MATRIX)->values[(ROW) * (MATRIX)->columns + (COLUMN)]

struct Matrix
{
    i32 rows;
    i32 columns;
    f32 values[];
};




static void
MATRIX_multiply(struct Matrix* dst, struct Matrix* lhs, struct Matrix* rhs)
{

    sorry_if(!dst || !lhs || !rhs);         // Missing arguments.
    sorry_if(dst == lhs || dst == rhs);     // No destination aliasing (e.g. `A = A * B;` is disallowed).
    sorry_if(dst->rows    != lhs->rows   ); // Incorrect dimensions.
    sorry_if(dst->columns != rhs->columns); // "
    sorry_if(lhs->columns != rhs->rows   ); // "

    for (i32 i = 0; i < lhs->rows; i += 1)
    {
        for (i32 j = 0; j < rhs->columns; j += 1)
        {

            dst->values[i * dst->columns + j] = 0;

            for (i32 k = 0; k < rhs->rows; k += 1)
            {
                AT(dst, i, j) += AT(lhs, i, k) * AT(rhs, k, j);
            }

        }
    }

}



static void
MATRIX_multiply_add(struct Matrix* accumulator, struct Matrix* addend, f32 factor)
{

    sorry_if(!accumulator || !addend);                 // Missing arguments.
    sorry_if(accumulator->rows    != addend->rows);    // Incorrect dimensions.
    sorry_if(accumulator->columns != addend->columns); // Incorrect dimensions.

    for (i32 i = 0; i < accumulator->rows; i += 1)
    {
        for (i32 j = 0; j < accumulator->columns; j += 1)
        {
            AT(accumulator, i, j) += AT(addend, i, j) * factor;
        }
    }

}



static void
MATRIX_stlink_tx(struct Matrix* matrix)
{

    sorry_if(!matrix); // Missing argument.

    for (i32 i = 0; i < matrix->rows; i += 1)
    {

        stlink_tx
        (
            "%c ",
            matrix->rows == 1     ? '<'  :
            i == 0                ? '/'  :
            i == matrix->rows - 1 ? '\\' : '|'
        );

        for (i32 j = 0; j < matrix->columns; j += 1)
        {
            stlink_tx("%9.5g ", AT(matrix, i, j));
        }

        stlink_tx
        (
            "%c\n",
            matrix->rows == 1     ? '>'  :
            i == 0                ? '\\' :
            i == matrix->rows - 1 ? '/'  : '|'
        );

    }

}



////////////////////////////////////////////////////////////////////////////////
//
// Actual GNC stuff.
//



static useret enum GNCUpdateResult : u32
{
    GNCUpdateResult_okay,
    GNCUpdateResult_bug = BUG_CODE,
}
GNC_update
(
    struct Matrix* resulting_angular_velocities
)
{

    sorry_if(!resulting_angular_velocities);



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

    MATRIX_multiply(resulting_angular_velocities, gain, state);

    MATRIX_multiply_add(resulting_angular_velocities, resulting_angular_velocities, -2);



    return GNCUpdateResult_okay;

}



////////////////////////////////////////////////////////////////////////////////
//
// Notes.
//



// @/`Using Matrices`:
//
// Matrices should be made by doing `Matrix(A, B)` to create
// an AxB matrix of zeros, or `Matrix(A, B, ...)` where all
// elements are listed out explicitly; it should be noted that
// it's not checked if the amount of listed elements matches up
// with AxB, so if too few elements are given, the rest of the
// matrix's elements are initialized to zero (but too many and
// the compiler will complain, so that's okay).
//
// The `Matrix` macro creates a `Matrix` instance on the stack.
// This means matrices should not be used as return values as
// they are allocated in the called function's stack frame.
//
// Matrix elements are stored as a flat array in row-major order.
// To access a specific element of the matrix, use the `AT` macro,
// like: `AT(my_matrix, 5, 6)` to get the element in the 6th row,
// 7th column (because zero-indexing).
