// The following macro creates a Matrix instance
// on the stack. This means matrices should not be
// used as return values as they are allocated in the
// function's stack frame.
//
// Matrices should be made by doing `Matrix(A, B)`
// to create a AxB matrix of zeros or `Matrix(A, B, ...)`
// where all elements are listed out explicitly;
// it should be noted that it's not checked if the amount
// of listed elements matches up with AxB, so if too few
// elements are given, the rest of the matrix's elements
// are initialized to zero (but too many and the compiler
// will complain, so that's okay).

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

struct Matrix
{
    i32 rows;
    i32 columns;
    f32 values[];
};



// Matrix elements are stored as a
// flat array in row-major order.
// The following macro makes it convenient
// to index an arbitrary matrix.

#define AT(MATRIX, ROW, COLUMN) \
    (MATRIX)->values[(ROW) * (MATRIX)->columns + (COLUMN)]



// To multiply two matrices, the destination matrix
// should be allocated beforehand to the correct dimensions.

static void
MATRIX_multiply(struct Matrix* dst, struct Matrix* lhs, struct Matrix* rhs)
{

    if (!dst)
        sorry

    if (!lhs)
        sorry

    if (!rhs)
        sorry

    if (dst == lhs)
        sorry

    if (dst == rhs)
        sorry

    if (dst->rows != lhs->rows)
        sorry

    if (dst->columns != rhs->columns)
        sorry

    if (lhs->columns != rhs->rows)
        sorry

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



// Matrices can be added/subtracted with
// the following multiply-add routine.
// Note that it's accumulator-based, so it
// modifies one of the given matrices.

static void
MATRIX_multiply_add(struct Matrix* accumulator, struct Matrix* addend, f32 factor)
{

    if (!accumulator)
        sorry

    if (!addend)
        sorry

    if (accumulator->rows != addend->rows)
        sorry

    if (accumulator->columns != addend->columns)
        sorry

    for (i32 i = 0; i < accumulator->rows; i += 1)
    {
        for (i32 j = 0; j < accumulator->columns; j += 1)
        {
            AT(accumulator, i, j) += AT(addend, i, j) * factor;
        }
    }

}



// Helper routine to dump the contents of the matrix
// for diagnostics. The rounding should be accounted for
// if the values are to be analyzed seriously.

static void
MATRIX_stlink_tx(struct Matrix* matrix)
{

    if (!matrix)
        sorry

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
