////////////////////////////////////////////////////////////////////////////////
//
// Math stuff.
//



#define Matrix_AxB(ROWS, COLUMNS)    \
    struct Matrix_##ROWS##x##COLUMNS \
    {                                \
        f32 rows[(ROWS)][(COLUMNS)]; \
    }

Matrix_AxB(1, 1); Matrix_AxB(2, 1); Matrix_AxB(3, 1); Matrix_AxB(4, 1); Matrix_AxB(5, 1); Matrix_AxB(6, 1);
Matrix_AxB(1, 2); Matrix_AxB(2, 2); Matrix_AxB(3, 2); Matrix_AxB(4, 2); Matrix_AxB(5, 2); Matrix_AxB(6, 2);
Matrix_AxB(1, 3); Matrix_AxB(2, 3); Matrix_AxB(3, 3); Matrix_AxB(4, 3); Matrix_AxB(5, 3); Matrix_AxB(6, 3);
Matrix_AxB(1, 4); Matrix_AxB(2, 4); Matrix_AxB(3, 4); Matrix_AxB(4, 4); Matrix_AxB(5, 4); Matrix_AxB(6, 4);
Matrix_AxB(1, 5); Matrix_AxB(2, 5); Matrix_AxB(3, 5); Matrix_AxB(4, 5); Matrix_AxB(5, 5); Matrix_AxB(6, 5);
Matrix_AxB(1, 6); Matrix_AxB(2, 6); Matrix_AxB(3, 6); Matrix_AxB(4, 6); Matrix_AxB(5, 6); Matrix_AxB(6, 6);

#undef Matrix_AxB

struct Quaternion
{
    f32 s;
    f32 i;
    f32 j;
    f32 k;
};

struct EulerZYX
{
    f32 yaw;   // Z-axis.
    f32 pitch; // Y-axis.
    f32 roll;  // X-axis.
};



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#include "CMSIS-DSP/Include/arm_common_tables.h"
#include "CMSIS-DSP/Source/CommonTables/CommonTables.c"
#include "CMSIS-DSP/Source/FastMathFunctions/FastMathFunctions.c"
#pragma GCC diagnostic pop

static useret f32
atan2_f32(f32 y, f32 x)
{

    f32        result = {0};
    arm_status status = arm_atan2_f32(y, x, &result);

    if (status != ARM_MATH_SUCCESS)
        sus;

    return result;

}

static useret f32
asin_f32(f32 x)
{

    f32        y      = {0};
    arm_status status = arm_sqrt_f32(1 - x * x, &y);

    if (status != ARM_MATH_SUCCESS)
        sus;

    f32 result = atan2_f32(x, y);

    return result;

}



#define MATRIX_multiply(DST, LHS, RHS)                                     \
    do                                                                     \
    {                                                                      \
        static_assert(countof((DST)->rows   ) == countof((LHS)->rows   )); \
        static_assert(countof((DST)->rows[0]) == countof((RHS)->rows[0])); \
        static_assert(countof((LHS)->rows[0]) == countof((RHS)->rows   )); \
        MATRIX_multiply_                                                   \
        (                                                                  \
            &(DST)->rows[0][0],                                            \
            &(LHS)->rows[0][0],                                            \
            &(RHS)->rows[0][0],                                            \
            countof((DST)->rows   ),                                       \
            countof((DST)->rows[0]),                                       \
            countof((LHS)->rows[0])                                        \
        );                                                                 \
    }                                                                      \
    while (false)
static void
MATRIX_multiply_(f32* dst, f32* lhs, f32* rhs, i32 rows, i32 columns, i32 common)
{

    if (dst == lhs || dst == rhs)
        sus; // No destination aliasing (e.g. `A = A * B;` is disallowed).

    for (i32 i = 0; i < rows; i += 1)
    {
        for (i32 j = 0; j < columns; j += 1)
        {

            dst[i * columns + j] = 0;

            for (i32 k = 0; k < common; k += 1)
            {
                dst[i * columns + j] += lhs[i * common + k] * rhs[k * columns + j];
            }

        }
    }

}



#define MATRIX_scale_add(DST, A, LHS, B, RHS)                              \
    do                                                                     \
    {                                                                      \
        static_assert(countof((DST)->rows   ) == countof((LHS)->rows   )); \
        static_assert(countof((DST)->rows[0]) == countof((LHS)->rows[0])); \
        static_assert(countof((DST)->rows   ) == countof((RHS)->rows   )); \
        static_assert(countof((DST)->rows[0]) == countof((RHS)->rows[0])); \
        MATRIX_scale_add_                                                  \
        (                                                                  \
            &(DST)->rows[0][0],                                            \
            (A),                                                           \
            &(LHS)->rows[0][0],                                            \
            (B),                                                           \
            &(RHS)->rows[0][0],                                            \
            countof((DST)->rows   ),                                       \
            countof((DST)->rows[0])                                        \
        );                                                                 \
    }                                                                      \
    while (false)
static void
MATRIX_scale_add_(f32* dst, f32 a, f32* lhs, f32 b, f32* rhs, i32 rows, i32 columns)
{
    for (i32 i = 0; i < rows; i += 1)
    {
        for (i32 j = 0; j < columns; j += 1)
        {
            dst[i * columns + j] = (a * lhs[i * columns + j]) + (b * rhs[i * columns + j]);
        }
    }
}



#define stlink_tx_Matrix(MATRIX)    \
    stlink_tx_Matrix_               \
    (                               \
        &(MATRIX)->rows[0][0],      \
        countof((MATRIX)->rows   ), \
        countof((MATRIX)->rows[0])  \
    )
static void
stlink_tx_Matrix_(f32* matrix, i32 rows, i32 columns)
{

    for (i32 i = 0; i < rows; i += 1)
    {

        stlink_tx
        (
            "%c ",
            rows == 1     ? '<'  :
            i == 0        ? '/'  :
            i == rows - 1 ? '\\' : '|'
        );

        for (i32 j = 0; j < columns; j += 1)
        {
            stlink_tx("%9.5g ", matrix[i * columns + j]);
        }

        stlink_tx
        (
            "%c\n",
            rows == 1     ? '>'  :
            i == 0        ? '\\' :
            i == rows - 1 ? '/'  : '|'
        );

    }

}



static useret struct Quaternion
QUATERNION_multiply(struct Quaternion lhs, struct Quaternion rhs)
{

    struct Quaternion result =
        {
            .s = lhs.s * rhs.s - lhs.i * rhs.i - lhs.j * rhs.j - lhs.k * rhs.k,
            .i = lhs.s * rhs.i + lhs.i * rhs.s + lhs.j * rhs.k - lhs.k * rhs.j,
            .j = lhs.s * rhs.j - lhs.i * rhs.k + lhs.j * rhs.s + lhs.k * rhs.i,
            .k = lhs.s * rhs.k + lhs.i * rhs.j - lhs.j * rhs.i + lhs.k * rhs.s,
        };

    return result;

}



static useret struct Quaternion
QUATERNION_conjugate(struct Quaternion q)
{

    struct Quaternion result =
        {
            .s = +q.s,
            .i = -q.i,
            .j = -q.j,
            .k = -q.k,
        };

    return result;

}



static useret struct Quaternion
QUATERNION_from_euler_zyx(struct EulerZYX angles)
{

    f32 cy = arm_cos_f32(angles.yaw   * 0.5f);
    f32 sy = arm_sin_f32(angles.yaw   * 0.5f);
    f32 cp = arm_cos_f32(angles.pitch * 0.5f);
    f32 sp = arm_sin_f32(angles.pitch * 0.5f);
    f32 cr = arm_cos_f32(angles.roll  * 0.5f);
    f32 sr = arm_sin_f32(angles.roll  * 0.5f);

    struct Quaternion result =
        {
            .s = cr * cp*cy + sr * sp * sy,
            .i = sr * cp*cy - cr * sp * sy,
            .j = cr * sp*cy + sr * cp * sy,
            .k = cr * cp*sy - sr * sp * cy,
        };

    return result;

}



static useret struct EulerZYX
QUATERNION_to_euler_zyx(struct Quaternion q)
{

    f32 sinr_cosp = 2.0f * (q.s * q.i + q.j * q.k);
    f32 cosr_cosp = 1.0f - 2.0f * (q.i*q.i + q.j * q.j);
    f32 sinp      = 2.0f * (q.s * q.j - q.k * q.i);
    f32 siny_cosp = 2.0f * (q.s * q.k + q.i * q.j);
    f32 cosy_cosp = 1.0f - 2.0f * (q.j * q.j + q.k * q.k);

    struct EulerZYX result =
        {
            .yaw   = atan2_f32(siny_cosp, cosy_cosp),
            .pitch = (fabsf(sinp) >= 1.0f) ? copysignf(PI / 2.0f, sinp) : asin_f32(sinp),
            .roll  = atan2_f32(sinr_cosp, cosr_cosp),
        };

    return result;

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
                u16 computer_vision_processing_time_ms; // Do not use in `GNC_update`! Use `.most_recent_openmv_reading_timestamp_us` instead.
                u8  computer_vision_confidence;
                u8  padding[47];
            } gnc;

            struct OpenMVPacketImage
            {
                u16 sequence_number;
                u8  bytes[62];
            } image;

        };
    };

    static_assert(sizeof(struct OpenMVPacketGNC  ) == sizeof(struct OpenMVPacket));
    static_assert(sizeof(struct OpenMVPacketImage) == sizeof(struct OpenMVPacket));

pack_pop



enum GNCOperationMode : u32
{
    GNCOperationMode_obtaining_heading_estimate,
    GNCOperationMode_stabilizing,
    GNCOperationMode_searching,
    GNCOperationMode_aligning,
};

struct GNCInput
{

    // Data in here is stuff the caller of `GNC_update` will provide us,
    // and thus shouldn't be modified (can't anyways because it's marked as `const`).
    //
    // All of the fields (and that field's subfields) below should have its own column
    // in the spreadsheet `./misc/GNC_MOCK_SIMULATION.csv`. For example, there should
    // be a column for `current_timestamp_us`, `most_recent_imu.QuatX`, `most_recent_imu.QuatY`, etc.

    u32                    current_timestamp_us;
    u32                    ejection_timestamp_us;
    struct VN100Packet     most_recent_imu;
    struct OpenMVPacketGNC most_recent_openmv_reading;
    u32                    most_recent_openmv_reading_timestamp_us;

};

struct GNCContext
{
    b32                   initialized;
    enum GNCOperationMode operation_mode;
    struct Matrix_3x1     control_accelerations;
    b32                   target_found;
    i32                   target_conflict_count;
    u32                   target_lost_timestamp_us;
    u32                   target_found_timestamp_us;
    struct Quaternion     desired_orientation;
};

static void
GNC_update(const struct GNCInput input, struct GNCContext* context)
{

    #define SEARCH_AMPLITUDE       0.3491f
    #define SEARCH_FREQUENCY       3.0f
    #define SEARCH_RATE            0.1745f
    #define REACTION_WHEEL_INERTIA 0.01f // TODO: Get actual value for this.

    if (!context)
        sus;



    ////////////////////////////////////////
    //
    // TODO.
    //

    if (!context->initialized)
    {
        *context =
            (struct GNCContext) // TODO.
            {
                .initialized              = true,
                .target_lost_timestamp_us = input.ejection_timestamp_us,
                .operation_mode           = GNCOperationMode_obtaining_heading_estimate,
            };
    }



    ////////////////////////////////////////
    //
    // TODO.
    //

    struct Quaternion quaternion_current =
        (struct Quaternion)
        {
            .s = input.most_recent_imu.QuatS,
            .i = input.most_recent_imu.QuatX,
            .j = input.most_recent_imu.QuatY,
            .k = input.most_recent_imu.QuatZ,
        };

    struct Quaternion quaternion_target = {0};
    struct Matrix_3x1 rates_target      = {0};

    for (b32 yield = false; !yield;)
    {
        switch (context->operation_mode)
        {



            // TODO.

            case GNCOperationMode_obtaining_heading_estimate:
            {

                if (input.current_timestamp_us - input.ejection_timestamp_us < 10'000'000)
                {
                    // TODO: Motors Disabled
                    // TODO: Send VNKMD OFF
                    yield = true;
                }
                else
                {
                    context->operation_mode = GNCOperationMode_stabilizing;
                }

            } break;



            // TODO.

            case GNCOperationMode_stabilizing:
            {
                if (input.current_timestamp_us - input.ejection_timestamp_us < 30'000'000)
                {

                    // TODO: Align to intial target
                    // TODO: Send VMKMD ON

                    quaternion_target =
                        QUATERNION_from_euler_zyx
                        (
                            (struct EulerZYX)
                            {
                                .yaw   = QUATERNION_to_euler_zyx(quaternion_current).yaw,
                                .pitch = 0.0f,
                                .roll  = 0.0f,
                            }
                        );

                    yield = true;

                }
                else
                {
                    context->operation_mode = GNCOperationMode_searching;
                }
            } break;



            // TODO.

            case GNCOperationMode_searching:
            {
                if (context->target_found)
                {
                    context->operation_mode = GNCOperationMode_aligning;
                }
                else
                {

                    quaternion_target =
                        QUATERNION_from_euler_zyx
                        (
                            (struct EulerZYX)
                            {
                                .yaw   = QUATERNION_to_euler_zyx(quaternion_current).yaw,
                                .pitch = SEARCH_AMPLITUDE *arm_sin_f32(SEARCH_FREQUENCY * (f32) input.current_timestamp_us / 1'000'000.0f),
                                .roll  = 0.0f,
                            }
                        );

                    rates_target =
                        (struct Matrix_3x1)
                        {
                            .rows =
                                {
                                    { 0.0f        },
                                    { 0.0f        },
                                    { SEARCH_RATE },
                                },
                        };

                    yield = true;

                }
            } break;



            // TODO.

            case GNCOperationMode_aligning:
            {

                if (context->target_found)
                {

                    quaternion_target =
                        QUATERNION_from_euler_zyx
                        (
                            (struct EulerZYX)
                            {
                                .yaw   = input.most_recent_openmv_reading.attitude_yaw,
                                .pitch = input.most_recent_openmv_reading.attitude_pitch,
                                .roll  = input.most_recent_openmv_reading.attitude_roll,
                            }
                        );

                    yield = true;

                }
                else if (input.current_timestamp_us - context->target_lost_timestamp_us < 10'000'000)
                {

                    quaternion_target =
                        (struct Quaternion)
                        {
                            // TODO: get this from buffer
                        };

                    yield = true;

                }
                else
                {
                    context->operation_mode = GNCOperationMode_searching;
                }

            } break;



            default: sus;

        }
    }



    ////////////////////////////////////////
    //
    // Apply hesterisis to CVT's target confidence.
    //

    if (context->target_found)
    {
        if (input.most_recent_openmv_reading.computer_vision_confidence)
        {
            context->target_conflict_count = 0; // We still see the target!
        }
        else if (context->target_conflict_count < 3)
        {
            context->target_conflict_count += 1; // Hmm, we're losing the target..?
        }
        else
        {
            context->target_found             = false; // Target definitely lost!
            context->target_conflict_count    = 0;
            context->target_lost_timestamp_us = input.current_timestamp_us;
        }
    }
    else
    {
        if (!input.most_recent_openmv_reading.computer_vision_confidence)
        {
            context->target_conflict_count = 0; // Target still missing...
        }
        else if (context->target_conflict_count < 3)
        {
            context->target_conflict_count += 1; // Oh, we're starting to see the target..?
        }
        else
        {
            context->target_found          = true; // Confident we now see the target!
            context->target_conflict_count = 0;
        }
    }



    ////////////////////////////////////////
    //
    // Compute errors for control law.
    //

    struct Matrix_3x1 rates_current_body =
        {
            .rows =
                {
                    { input.most_recent_imu.GyroX },
                    { input.most_recent_imu.GyroY },
                    { input.most_recent_imu.GyroZ },
                },
        };

    struct Matrix_3x1 rates_error = {0};

    MATRIX_scale_add
    (
        &rates_error,
        +1.0f, &rates_target,
        -1.0f, &rates_current_body
    );



    ////////////////////////////////////////
    //
    // Select appropriate gain matrix based on the current state and operation mode.
    // TODO: Implement the gain selection logic based on the current state and operation mode.
    //

    struct Quaternion quaternion_error = QUATERNION_multiply(quaternion_current, QUATERNION_conjugate(quaternion_target));

    // VERY IMPORTANT NOTE:
    //  - to avoid an additional subtraction operation, gains should be negated
    //  - for (u = -Kx) -> (gain = -K)
    struct Matrix_3x6 gain = {0};

    {

        #define ANGLE_THRESHOLD_SMALL  0.924f
        #define ANGLE_THRESHOLD_MEDIUM 0.707f

        switch (context->operation_mode)
        {

            case GNCOperationMode_obtaining_heading_estimate:
            {
                // Don't care.
            } break;



            // Control enabled, align to target
            //  - Linearize about [~, ~, ~, 15, 15, 15]
            //  - Set Q = diag(0, a, a, b, b, b) where a and b are user-defined parameters

            case GNCOperationMode_aligning:
            case GNCOperationMode_stabilizing:
            {
                if (quaternion_error.s > ANGLE_THRESHOLD_SMALL)
                {
                    //TODO: select small angle gain matrix
                }
                else if (quaternion_error.s > ANGLE_THRESHOLD_MEDIUM)
                {
                    //TODO: select medium angle gain matrix
                }
                else
                {
                    //TODO: select large angle gain matrix
                }
            } break;



            // Control enabled, search for target
            //  - Linearize about [~, ~, ~, 15, 15, SEARCH_RATE]
            //  - Set Q = diag(0, a, a, b, b, c>b) where a, b, c are user-defined parameters

            case GNCOperationMode_searching:
            {
                if (quaternion_error.s > ANGLE_THRESHOLD_SMALL)
                {
                    //TODO: select small angle gain matrix
                }
                else if (quaternion_error.s > ANGLE_THRESHOLD_MEDIUM)
                {
                    //TODO: select medium angle gain matrix
                }
                else
                {
                    //TODO: select large angle gain matrix
                }
            } break;



            default : sus;

        }

    }



    ////////////////////////////////////////
    //
    // Compute control torques and convert
    // control torques to accelerations.
    //

    struct Matrix_6x1 state =
        {
            .rows =
                {
                    { quaternion_error.i     },
                    { quaternion_error.j     },
                    { quaternion_error.k     },
                    { rates_error.rows[0][0] },
                    { rates_error.rows[1][0] },
                    { rates_error.rows[2][0] },
                },
        };

    struct Matrix_3x1 control_torques = {0};
    MATRIX_multiply(&control_torques, &gain, &state); // TODO: Implement the control law to compute the angular accelerations based on the gain and the state.

    context->control_accelerations =
        (struct Matrix_3x1)
        {
            .rows =
                {
                    { control_torques.rows[0][0] / REACTION_WHEEL_INERTIA },
                    { control_torques.rows[1][0] / REACTION_WHEEL_INERTIA },
                    { control_torques.rows[2][0] / REACTION_WHEEL_INERTIA },
                },
        };

}
