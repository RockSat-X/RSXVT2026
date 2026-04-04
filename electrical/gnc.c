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
    b32               initialized;
    struct Matrix_3x1 control_accelerations;
    b32               target_found;
    i32               target_conflict_count;
    u32               target_lost_timestamp_us;
    u32               target_found_timestamp_us;
    struct Quaternion desired_orientation;
};

static void
GNC_update(const struct GNCInput input, struct GNCContext* context)
{

    if (!context)
        sus;



    ////////////////////////////////////////
    //
    // See if we need to set some default
    // values for the GNC context.
    //

    if (!context->initialized)
    {

        // Let's make sure all values are
        // zero instead of potentially
        // any left-over garbage.

        memzero(context);



        // We'll start off as if we lost
        // the target from the moment we
        // have ejected.

        context->target_found             = false;
        context->target_conflict_count    = 0;
        context->target_lost_timestamp_us = input.ejection_timestamp_us;



        // An actual value will be computed
        // for these fields later on.

        context->control_accelerations = (struct Matrix_3x1) {0};
        context->desired_orientation   = (struct Quaternion) {0};



        // We're done setting the initial values
        // for the rest of the GNC algorithm to work on.

        context->initialized = true;

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
    // TODO.
    //

    u32     time_to_start_us = 10'000'000;  // time after ejection to enable motors
    u32     time_to_align_us = 20'000'000;  // time to align to target estimate before starting search pattern

    u32     time_since_ejection_us = input.current_timestamp_us - input.ejection_timestamp_us;              // time since ejection
    u32     time_since_target_lost_us = input.current_timestamp_us - context->target_lost_timestamp_us;     // time since target lost
    u32     time_since_target_found_us = input.current_timestamp_us - context->target_found_timestamp_us;   // time since target found

    f32     search_rate = 0.1745f; // search rate in radians per second (10 degrees per second)
    f32     reaction_wheel_inertia = 0.01f;     // reaction wheel inertia (kg*m^2)
    // TODO: Get actual value for this.

    f32 a = 0.3491f; // search pattern amplitude in radians (20 degrees)
    f32 b = 3.0f;  // search pattern frequency (3 pitch oscillations per yaw revolution)


    // Operation modes:
    // (0) Ejection: Control disabled, tracking only, obtain heading estimate (VNKMD OFF)
    // (1) Alignment: Control enabled, align to target
    //      - Target Rates set to zero
    //      - Drive pitch and roll error to zero, no yaw error control
    // (2) Search: Control enabled, search for target
    //      - Target yaw rate set to constant
    //      - Drive pitch and roll erros to zero, no yaw error control
    i32     operation_mode = 0;

    struct Matrix_6x1 state = {0};              // initialize state vector
    struct Matrix_3x1 rates_target = {0};       // initialize target rates, default rates set to zero
    struct Quaternion quaternion_target = {0};  // initialize target quaternion
    struct Matrix_3x1 rates_error = {0};        // initialize error rates

    // Convert to quaternion
    struct Quaternion quaternion_current =
        (struct Quaternion)
        {
            .s = input.most_recent_imu.QuatS,
            .i = input.most_recent_imu.QuatX,
            .j = input.most_recent_imu.QuatY,
            .k = input.most_recent_imu.QuatZ,
        };

    struct Quaternion quaternion_from_last_target =
        (struct Quaternion)
        {
            // TODO: get this from buffer
        };

    struct EulerZYX euler_current = QUATERNION_to_euler_zyx(quaternion_current);

    struct Matrix_3x1 rates_current_body = {0};
    rates_current_body.rows[0][0] = input.most_recent_imu.GyroX;
    rates_current_body.rows[1][0] = input.most_recent_imu.GyroY;
    rates_current_body.rows[2][0] = input.most_recent_imu.GyroZ;



    ////////////////////////////////////////
    //
    // TODO.
    //

    if (time_since_ejection_us < time_to_start_us)
    {
        // Motors Disabled
        // TODO: Send VNKMD OFF

        operation_mode = 0;
    }
    else if (time_since_ejection_us < (time_to_start_us + time_to_align_us))
    {
        // Align to intial target
        // TODO: Send VMKMD ON

        operation_mode = 1;

        // Define intial target quaternion
        {

            quaternion_target =
                QUATERNION_from_euler_zyx
                (
                    (struct EulerZYX)
                    {
                        .yaw   = euler_current.yaw, // use current yaw angle to avoid unnecessary rotation
                        .pitch = 0.0f, // set pitch to zero
                        .roll  = 0.0f, // set roll to zero
                    }
                );
        }
    }
    else
    {
        if (!context->target_found)
        {
            if (time_since_target_lost_us < time_to_align_us)
            {
                // Align to intial target
                operation_mode = 1;

                quaternion_target = quaternion_from_last_target;
            }
            else
            {
                // Search for Target
                operation_mode = 2;

                quaternion_target =
                    QUATERNION_from_euler_zyx
                    (
                        (struct EulerZYX)
                        {
                            .yaw   = euler_current.yaw, // yaw doesn't matter since we're just spinning about D axis
                            .pitch = a*arm_sin_f32(b * (f32) input.current_timestamp_us / 1'000'000.0f), // oscillate pitch to create search pattern
                            .roll  = 0.0f,
                        }
                    );

                rates_target.rows[2][0] = search_rate;  // search rate
            }
        }
        else
        {
            // Track Target
            if (time_since_target_found_us < time_to_align_us)
            {
                // Align to target
                operation_mode = 1;

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
            }
            else
            {
                // Rotate about down (D) axis
                operation_mode = 2;

                rates_target.rows[2][0] = search_rate; // search rate

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
            }
        }
    }



    ////////////////////////////////////////
    //
    // Compute errors for control law.
    //

    struct Quaternion quaternion_error = QUATERNION_multiply(quaternion_current, QUATERNION_conjugate(quaternion_target));
    rates_error.rows[1][0] = rates_target.rows[0][0] - rates_current_body.rows[0][0];
    rates_error.rows[2][0] = rates_target.rows[1][0] - rates_current_body.rows[1][0];
    rates_error.rows[3][0] = rates_target.rows[2][0] - rates_current_body.rows[2][0];



    ////////////////////////////////////////
    //
    // Define state vector for control law
    //

    state.rows[0][0] = quaternion_error.i;
    state.rows[1][0] = quaternion_error.j;
    state.rows[2][0] = quaternion_error.k;
    state.rows[3][0] = rates_error.rows[0][0];
    state.rows[4][0] = rates_error.rows[1][0];
    state.rows[5][0] = rates_error.rows[2][0];



    ////////////////////////////////////////
    //
    // Select appropriate gain matrix based on the current state and operation mode.
    // TODO: Implement the gain selection logic based on the current state and operation mode.
    //

    struct Matrix_3x6 gain = {0};

    {

        // VERY IMPORTANT NOTE:
        //  - to avoid an additional subtraction operation, gains should be negated
        //  - for (u = -Kx) -> (gain = -K)

        f32 quaternion_threshold_1 = 0.924f; // small angle
        f32 quaternion_threshold_2 = 0.707f;  // medium angle

        // (1) Alignment: Control enabled, align to target
        //  - Linearize about [~, ~, ~, 15, 15, 15]
        //  - Set Q = diag(0, a, a, b, b, b) where a and b are user-defined parameters
        if (operation_mode == 1)
        {
            if (quaternion_error.s > quaternion_threshold_1)
            {
                //TODO: select small angle gain matrix
            }
            else if (quaternion_error.s > quaternion_threshold_2)
            {
                //TODO: select medium angle gain matrix
            }
            else
            {
                //TODO: select large angle gain matrix
            }
        }

        // (2) Search: Control enabled, search for target
        //  - Linearize about [~, ~, ~, 15, 15, search_rate]
        //  - Set Q = diag(0, a, a, b, b, c>b) where a, b, c are user-defined parameters
        else if (operation_mode == 2)
        {
            if (quaternion_error.s > quaternion_threshold_1)
            {
                //TODO: select small angle gain matrix
            }
            else if (quaternion_error.s > quaternion_threshold_2)
            {
                //TODO: select medium angle gain matrix
            }
            else
            {
                //TODO: select large angle gain matrix
            }
        }

    }



    ////////////////////////////////////////
    //
    // Compute control torques and convert
    // control torques to accelerations.
    //

    struct Matrix_3x1 control_torques = {0};
    MATRIX_multiply(&control_torques, &gain, &state); // TODO: Implement the control law to compute the angular accelerations based on the gain and the state.

    context->control_accelerations.rows[0][0] = control_torques.rows[0][0] / reaction_wheel_inertia;
    context->control_accelerations.rows[1][0] = control_torques.rows[1][0] / reaction_wheel_inertia;
    context->control_accelerations.rows[2][0] = control_torques.rows[2][0] / reaction_wheel_inertia;



    ////////////////////////////////////////
    //
    // TODO.
    //

    if (operation_mode == 0)
    {
        // TODO: return context->control_accelerations = (struct Matrix_3x1) {0};
    }
    else
    {
        // TODO: context->control_accelerations;
    }

}
