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

    if (!dst || !lhs || !rhs)
        sus; // Missing arguments.

    if (dst == lhs || dst == rhs)
        sus; // No destination aliasing (e.g. `A = A * B;` is disallowed).

    if (dst->rows != lhs->rows)
        sus; // Incorrect dimensions.

    if (dst->columns != rhs->columns)
        sus; // Incorrect dimensions.

    if (lhs->columns != rhs->rows)
        sus; // Incorrect dimensions.

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

    if (!accumulator || !addend)
        sus; // Missing arguments.

    if (accumulator->rows != addend->rows)
        sus; // Incorrect dimensions.

    if (accumulator->columns != addend->columns)
        sus; // Incorrect dimensions.

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

    if (!matrix)
        sus; // Missing argument.

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
// Quaternion stuff.
//



struct Quaternion
{
    f32 s;
    f32 i;
    f32 j;
    f32 k;
};

struct EulerAngles
{
    f32 yaw;
    f32 pitch;
    f32 roll;
};

static useret struct Quaternion
QUATERNION_multiply(struct Quaternion lhs, struct Quaternion rhs)
{
    return
        (struct Quaternion)
        {
            .s = lhs.s*rhs.s - lhs.i*rhs.i - lhs.j*rhs.j - lhs.k*rhs.k,
            .i = lhs.s*rhs.i + lhs.i*rhs.s + lhs.j*rhs.k - lhs.k*rhs.j,
            .j = lhs.s*rhs.j - lhs.i*rhs.k + lhs.j*rhs.s + lhs.k*rhs.i,
            .k = lhs.s*rhs.k + lhs.i*rhs.j - lhs.j*rhs.i + lhs.k*rhs.s,
        };
}

static useret struct Quaternion
QUATERNION_conjugate(struct Quaternion quaternion)
{
    return
        (struct Quaternion)
        {
            .s = +quaternion.s,
            .i = -quaternion.i,
            .j = -quaternion.j,
            .k = -quaternion.k,
        };
}

static useret struct Quaternion
QUATERNION_error(struct Quaternion quaternion_s, struct Quaternion quaternion_r)
{
    return QUATERNION_multiply(QUATERNION_conjugate(quaternion_s), quaternion_r);
}

static useret struct Quaternion
QUATERNION_from_euler_angles(struct EulerAngles angles)
{

    f32 cy = arm_cos_f32(angles.yaw   * 0.5f);
    f32 sy = arm_sin_f32(angles.yaw   * 0.5f);
    f32 cp = arm_cos_f32(angles.pitch * 0.5f);
    f32 sp = arm_sin_f32(angles.pitch * 0.5f);
    f32 cr = arm_cos_f32(angles.roll  * 0.5f);
    f32 sr = arm_sin_f32(angles.roll  * 0.5f);

    return
        (struct Quaternion)
        {
            .s = cr*cp*cy + sr*sp*sy,
            .i = sr*cp*cy - cr*sp*sy,
            .j = cr*sp*cy + sr*cp*sy,
            .k = cr*cp*sy - sr*sp*cy,
        };

}

static useret struct EulerAngles
EULER_from_quaternion(struct Quaternion q)
{

    f32 sinr_cosp = 2.0f * (q.s*q.i + q.j*q.k);
    f32 cosr_cosp = 1.0f - 2.0f * (q.i*q.i + q.j*q.j);

    f32 sinp = 2.0f * (q.s*q.j - q.k*q.i);

    f32 siny_cosp = 2.0f * (q.s*q.k + q.i*q.j);
    f32 cosy_cosp = 1.0f - 2.0f * (q.j*q.j + q.k*q.k);

    return
        (struct EulerAngles)
        {
            .yaw   = atan2f(siny_cosp, cosy_cosp),
            .pitch = (fabsf(sinp) >= 1.0f) ? copysignf(PI / 2.0f, sinp) : acosf(sinp),
            .roll  = atan2f(sinr_cosp, cosr_cosp),
        };

}



////////////////////////////////////////////////////////////////////////////////
//
// Actual GNC stuff.
//



pack_push

    struct VN100Packet
    {
        f32 QuatX;
        f32 QuatY;
        f32 QuatZ;
        f32 QuatS;
        f32 MagX;
        f32 MagY;
        f32 MagZ;
        f32 AccelX;
        f32 AccelY;
        f32 AccelZ;
        f32 GyroX;
        f32 GyroY;
        f32 GyroZ;
    };

    // TODO Finalize structure.
    // TODO We may have two packet variations: one for IMU + image data and one for just image data.


    struct OpenMVPacket // @/`OpenMV Packet Format`: Coupled.
    {
        union
        {

            // @/`OpenMV Sequence Number`:
            // First image chunk begins at `1`; zero is reserved for `OpenMVPacketGNC`.

            u16 sequence_number;

            struct OpenMVPacketGNC
            {
                u16 zero;
                f32 attitude_yaw;
                f32 attitude_pitch;
                f32 attitude_roll;
                f32 unused;
                u16 computer_vision_processing_time_ms;
                u8  computer_vision_confidence;
                u8  padding[43];
            } gnc;

            struct OpenMVPacketImage
            {
                u16 sequence_number;
                u8  bytes[62];
            } image;

        };
    };

pack_pop



struct GNCParameters
{
    struct VN100Packet     most_recent_imu;
    struct OpenMVPacketGNC most_recent_openmv_reading;
    u32                    most_recent_openmv_reading_timestamp_us;
    u32                    ejection_timestamp_us;
    u32                    current_timestamp_us;
    b32                    target_found;
    i32                    target_conflict_count;
    u32                    target_lost_timestamp_us;
    struct Quaternion      desired_orientation;
};

static void
GNC_update(struct GNCParameters* parameters)
{

     u32 time_since_ejection_us = parameters->current_timestamp_us - parameters->ejection_timestamp_us;



    // Apply hesterisis to CVT's target confidence.

    if (parameters->target_found)
    {
        if (parameters->most_recent_openmv_reading.computer_vision_confidence)
        {
            parameters->target_conflict_count = 0; // We still see the target!
        }
        else if (parameters->target_conflict_count < 3)
        {
            parameters->target_conflict_count += 1; // Hmm, we're losing the target..?
        }
        else
        {
            parameters->target_found             = false; // Target definitely lost!
            parameters->target_conflict_count    = 0;
            parameters->target_lost_timestamp_us = parameters->current_timestamp_us;
        }
    }
    else
    {
        if (!parameters->most_recent_openmv_reading.computer_vision_confidence)
        {
            parameters->target_conflict_count = 0; // Target still missing...
        }
        else if (parameters->target_conflict_count < 3)
        {
            parameters->target_conflict_count += 1; // Oh, we're starting to see the target..?
        }
        else
        {
            parameters->target_found          = true; // Confident we now see the target!
            parameters->target_conflict_count = 0;
        }
    }



    // Determine the orientation the vehicle should have.

    struct Matrix* gain = {0};

    if (parameters->target_found)
    {
        if (parameters->most_recent_openmv_reading.computer_vision_confidence)
        {

            parameters->desired_orientation =
                QUATERNION_from_euler_angles
                (
                    (struct EulerAngles)
                    {
                        .yaw   = parameters->most_recent_openmv_reading.attitude_yaw,
                        .pitch = parameters->most_recent_openmv_reading.attitude_pitch,
                        .roll  = parameters->most_recent_openmv_reading.attitude_roll,
                    }
                );

            gain = nullptr; // TODO Determine the gain matrix for this situation.

        }
        else
        {
            gain = nullptr;
        }
    }
    else if (time_since_ejection_us < 20'000'000)
    {

        parameters->desired_orientation =
            QUATERNION_from_euler_angles
            (
                (struct EulerAngles)
                {
                    .yaw   = 0.0f, // TODO: Yaw value?
                    .pitch = 0.0f, // Reset pitch so we can do the search process.,
                    .roll  = 0.0f, // TODO: Roll value?
                }
            );

        gain = nullptr; // TODO Determine the gain matrix for this situation.

    }
    else // Do the process of searching.
    {

        f32 t = 0.0f;

        parameters->desired_orientation =
            QUATERNION_from_euler_angles
            (
                (struct EulerAngles)
                {
                    .yaw   = t, // TODO: Function of t?
                    .pitch = arm_sin_f32(t), // TODO: Function of t?
                    .roll  = 0, // TODO: Function of t?
                }
            );

        gain = nullptr; // TODO Determine the gain matrix for this situation.

    }



    // TODO.

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
