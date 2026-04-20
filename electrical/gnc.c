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

static useret f32
acos_f32(f32 x)
{
    f32        y      = {0};
    arm_status status = arm_sqrt_f32(1 - x * x, &y);

    if (status != ARM_MATH_SUCCESS)
        sus;

    f32 result = atan2_f32(y, x);

    return result;
}

static useret f32
sqrt_f32(f32 x)
{

    f32        result = {0};
    arm_status status = arm_sqrt_f32(x, &result);

    if (status != ARM_MATH_SUCCESS)
    {
        sus;
        result = 0.0f;
    }

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
                u16 computer_vision_processing_time_ms;
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
    GNCOperationMode_ejecting,
    GNCOperationMode_rate_damping,
    GNCOperationMode_searching,
    GNCOperationMode_aligning,
};

struct GNCInput
{
    u32                    current_timestamp_us;
    u32                    ejection_timestamp_us;
    struct VN100Packet     most_recent_imu;
    struct OpenMVPacketGNC most_recent_openmv_reading;
};

struct GNCContext
{
    b32                   initialized;
    enum GNCOperationMode operation_mode;
    struct Matrix_3x1     control_accelerations;
    b32                   target_found;
    b32                   prev_target_found;
    i32                   target_conflict_count;
    u32                   target_lost_timestamp_us;
    u32                   search_start_timestamp_us;
};

static void
GNC_update(const struct GNCInput input, struct GNCContext* context)
{

    if (!context)
        sus;



    ////////////////////////////////////////
    //
    // If needed, initialize our working
    // variables to known values.
    //

    if (!context->initialized)
    {
        *context =
            (struct GNCContext)
            {
                .initialized              = true,
                .operation_mode           = GNCOperationMode_ejecting,
                .control_accelerations    = {},
                .target_found             = false,
                .prev_target_found        = false,
                .target_conflict_count    = 0,
                .target_lost_timestamp_us = input.ejection_timestamp_us,
            };
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
        else if (context->target_conflict_count < 3) // TODO Determine hystersis threshold.
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
        else if (context->target_conflict_count < 3) // TODO Determine hystersis threshold.
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
    // Determine the desired vehicle
    // orientation and angular rates.
    //

    struct Quaternion desired_orientation =
        (struct Quaternion)
        {
            .s = 1.0f,
            .i = 0.0f,
            .j = 0.0f,
            .k = 0.0f,
        };

    struct Matrix_3x1 desired_rates = {0};

    struct Matrix_3x6 gain = {0};

    switch (context->operation_mode)
    {



        ////////////////////////////////////////
        //
        // Right after the vehicle has ejected,
        // the motors need to be kept disabled
        // so that the VN-100 can obtain an
        // accurate heading estimate without any
        // magnetic interference.
        //
        ////////////////////////////////////////

        case GNCOperationMode_ejecting:
        {
            if (input.current_timestamp_us - input.ejection_timestamp_us < 10'000'000)
            {

                // We're still letting the VN-100 get a heading estimate.
                // It'll be the responsibility of the caller to ensure
                // that the motors are disabled and that the VNKMD-OFF
                // command has been sent to the VN-100.

                // TODO No `desired_orientation` and `desired_rates`?

            }
            else
            {

                // We can now move onto actually using the stepper motors.
                // It'll be the responsibility of the caller to ensure
                // that the VNKMD-ON command gets sent to the VN-100
                // before enabling the motors.

                context->operation_mode = GNCOperationMode_rate_damping;

                // TODO No `desired_orientation` and `desired_rates`?

            }
        } break;



        ////////////////////////////////////////
        //
        // The vehicle might be tumbling a bit
        // after it has ejected, so we're going
        // so spend some time here undoing that.
        //
        ////////////////////////////////////////

        case GNCOperationMode_rate_damping:
        {
            if (input.current_timestamp_us - input.ejection_timestamp_us < 30'000'000)
            {

                // Drive rates to zero before searching

                gain =
                    (struct Matrix_3x6) // TODO Select large angle gain matrix.
                    {
                        .rows =
                            {
                                { 0.0f, 0.0f, 0.0f, 0.0120f, 0.0f   , 0.0f    },
                                { 0.0f, 0.0f, 0.0f, 0.0f   , 0.0116f, 0.0f    },
                                { 0.0f, 0.0f, 0.0f, 0.0f   , 0.0f   , 0.0111f },
                            },
                    };

            }
            else // We spent enough time stabilizing; move onto finding the horizon.
            {

                context->operation_mode = GNCOperationMode_searching;

                // TODO No `desired_orientation` and `desired_rates`?

            }
        } break;



        ////////////////////////////////////////
        //
        // We're currently not confident we have
        // found the horizon yet, so we're going
        // to do some motions in hopes that the
        // OpenMV will eventually pick something up.
        //
        ////////////////////////////////////////

        case GNCOperationMode_searching:
        {
            if (context->target_found)
            {

                // Got something! Move onto aligning the vehicle
                // towards the target that's hopefully the horizon.

                context->operation_mode = GNCOperationMode_aligning;

            }
            else
            {

                if (context->prev_target_found != context->target_found)
                {

                    // Just lost the target; try searching around the last known location.

                    context->search_start_timestamp_us = input.current_timestamp_us;

                }



                #define SEARCH_DURATION 30.0f
                #define SEARCH_GAIN      0.3f
                #define OMEGA_PHI        2.399963229728653f

                f32 search_time = (f32) (input.current_timestamp_us - context->search_start_timestamp_us) / 1'000'000.0f;
                f32 u           = search_time / SEARCH_DURATION;

                if (u < 0.0f)
                {
                    u = 0.0f;
                }
                else if (u > 1.0f)
                {
                    u = 1.0f;
                }

                f32 phi    = acos_f32(1.0f - 2.0f * u);
                f32 theta  = OMEGA_PHI * search_time; // TODO Not used?
                f32 dtheta = SEARCH_GAIN * OMEGA_PHI;
                f32 dphi   = 2.0f * SEARCH_GAIN / (SEARCH_DURATION * sqrt_f32(1 - ((1 - 2*u) * (1 - 2*u))));

                desired_rates =
                    (struct Matrix_3x1)
                    {
                        .rows =
                            {
                                { dtheta * arm_cos_f32(phi)  },
                                { dphi                       },
                                { -dtheta * arm_sin_f32(phi) },
                            },
                    };

            }
        } break;



        ////////////////////////////////////////
        //
        // We've found the target is our goal is
        // now to align the vehicle towards it.
        //
        ////////////////////////////////////////

        case GNCOperationMode_aligning:
        {
            if (context->target_found)
            {

                // Keep aligning towards the horizon according
                // to whatever the OpenMV is saying.

                desired_orientation =
                    QUATERNION_from_euler_zyx
                    (
                        (struct EulerZYX)
                        {
                            .yaw   = input.most_recent_openmv_reading.attitude_yaw,
                            .pitch = input.most_recent_openmv_reading.attitude_pitch,
                            .roll  = input.most_recent_openmv_reading.attitude_roll,
                        }
                    );

            }
            else // Alright, we've lost the horizon target for a while now.
            {

                context->operation_mode = GNCOperationMode_searching;

                // TODO No `desired_orientation` and `desired_rates`?

            }
        } break;



        default: sus;

    }



    ////////////////////////////////////////
    //
    // Compute errors.
    //

    struct Quaternion orientation_error =
        QUATERNION_multiply
        (
            (struct Quaternion) {0}, // TODO?
            QUATERNION_conjugate(desired_orientation)
        );

    struct Matrix_3x1 rates_error =
        {
            .rows =
                {
                    { desired_rates.rows[0][0] - input.most_recent_imu.GyroX },
                    { desired_rates.rows[1][0] - input.most_recent_imu.GyroY },
                    { desired_rates.rows[2][0] - input.most_recent_imu.GyroZ },
                },
        };



    ////////////////////////////////////////
    //
    // TODO Get gain matrix.
    //

    #define ANGLE_THRESHOLD_SMALL  0.924f
    #define ANGLE_THRESHOLD_MEDIUM 0.707f

    switch (context->operation_mode)
    {

        case GNCOperationMode_ejecting:
        {
            // Don't care.
        } break;



        // Linearize about [~, ~, ~, 15, 15, 15].
        // Set Q = diag(0, a, a, b, b, b) where a and b are user-defined parameters.

        case GNCOperationMode_aligning:
        case GNCOperationMode_rate_damping:
        {
            // TODO.
        } break;



        // Linearize about [~, ~, ~, 15, 15, search_rate].
        // Set Q = diag(0, a, a, b, b, c > b) where a, b, c are user-defined parameters.

        case GNCOperationMode_searching:
        {
            // TODO.
        } break;



        default : sus;

    }



    ////////////////////////////////////////
    //
    // Finally compute the stepper
    // motors' angular accelerations.
    //

    #define REACTION_WHEEL_INERTIA 0.000017469f // (kg-m^2)

    struct Matrix_6x1 state =
        {
            .rows =
                {
                    { orientation_error.i    },
                    { orientation_error.j    },
                    { orientation_error.k    },
                    { rates_error.rows[0][0] },
                    { rates_error.rows[1][0] },
                    { rates_error.rows[2][0] },
                },
        };

    struct Matrix_3x1 control_torques = {0};
    MATRIX_multiply(&control_torques, &gain, &state); // TODO Implement the control law to compute the angular accelerations based on the gain and the state.

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

    context->prev_target_found = context->target_found; // Update previous target found status for next iteration.

}
