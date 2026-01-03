#define Matrix(ROWS, COLUMNS, VALUES...) \
    ((struct Matrix*) (int[2 + (ROWS) * (COLUMNS)]) { (ROWS), (COLUMNS), VALUES })

struct Matrix
{
    int rows;
    int columns;
    int values[];
};



static void
MATRIX_multiply(struct Matrix* dst, struct Matrix* lhs, struct Matrix* rhs)
{

    for (int i = 0; i < lhs->rows; i += 1)
    {
        for (int j = 0; j < rhs->columns; j += 1)
        {

            dst->values[i * dst->columns + j] = 0;

            for (int k = 0; k < rhs->rows; k += 1)
            {
                dst->values[i * dst->columns + j] +=
                    lhs->values[i * lhs->columns + k] *
                    rhs->values[k * rhs->columns + j];
            }

        }
    }

}



static void
MATRIX_multiply_add(struct Matrix* accumulator, struct Matrix* addend, int factor)
{
    for (int i = 0; i < accumulator->rows; i += 1)
    {
        for (int j = 0; j < accumulator->columns; j += 1)
        {
            accumulator->values[i * accumulator->columns + j] += addend->values[i * addend->columns + j] * factor;
        }
    }
}
