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

struct state



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

// Convert a vector from body frame to NED frame using the quaternion attitude.
// This function is used to convert the angular rates from body frame to NED frame.
// This may not be needed.
vector_from_body_to_NED(struct Quaternion q, struct Matrix_3x1 vector_body)
{
    vector_NED.rows[0][0] = vector_body.rows[1][0]*(2*q.i*q.j - 2*q.k*q.s) - vector_body.rows[0][0]*(2*q.j^2 + 2*q.k^2 - 1) + vector_body.rows[2][0]*(2*q.i*q.k + 2*q.j*q.s);
    vector_NED.rows[1][0] = vector_body.rows[0][0]*(2*q.i*q.j + 2*q.k*q.s) - vector_body.rows[1][0]*(2*q.i^2 + 2*q.k^2 - 1) + vector_body.rows[2][0]*(2*q.j*q.k - 2*q.i*q.s);
    vector_NED.rows[2][0] = vector_body.rows[0][0]*(2*q.i*q.k - 2*q.j*q.s) - vector_body.rows[2][0]*(2*q.i^2 + 2*q.j^2 - 1) + vector_body.rows[1][0]*(2*q.j*q.k + 2*q.i*q.s);
}


// Select appropriate gain matrix based on the current state and operation mode.
static useret struct Matrix_3x6
gain_matrix_select(struct Quaternion quaternion_error, struct Matrix_3x1 rate_error, i32 operation_mode)
{

    // VERY IMPORTANT NOTE:
    //  - to avoid an additional subtraction operation, gains should be negated
    //  - for (u = -Kx) -> (gain = -K)

    quaternion_threshold_1 = 0.924f; // small angle
    quaternion_threshold_2 = 0.707f;  // medium angle

    struct Matrix_3x6 gain = {0};

    // (1) Alignment: Control enabled, align to target
    //  - Linearize about [~, ~, ~, 15, 15, 15]
    //  - Set Q = diag(0, a, a, b, b, b) where a and b are user-defined parameters
    if (operation_mode == 1)
    {
        if quaternion_error.s > quaternion_threshold_1
        {
            //TODO: select small angle gain matrix
        }
        else if quaternion_error.s > quaternion_threshold_2
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
        if quaternion_error.s > quaternion_threshold_1
        {
            //TODO: select small angle gain matrix
        }
        else if quaternion_error.s > quaternion_threshold_2
        {
            //TODO: select medium angle gain matrix
        }
        else
        {
            //TODO: select large angle gain matrix
        }
    }

    return gain;
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

#include "GNCContext.meta"
/* #meta

    import deps.stpy.pxd.pxd as pxd
    import builtins

    # This structure contains stuff that `GNC_update` will use to figure out what to do,
    # and ultimately produce an updated value of `angular_accelerations` that the caller
    # will use to update the stepper motors.
    #
    # The only value the caller cares about is `angular_accelerations`; everything else
    # is up to us to modify and use at our discretion.

    FIELDS = pxd.SimpleNamespaceTable(
        ('name'                     , 'type'             , 'format'),
        ('initialized'              , 'b32'              , None    ),
        ('control_accelerations'    , 'struct Matrix_3x1', ...     ),
        ('target_found'             , 'b32'              , ...     ),
        ('target_conflict_count'    , 'i32'              , ...     ),
        ('target_lost_timestamp_us' , 'u32'              , "%u us" ),
        ('target_found_timestamp_us','u32'               , "%u us" ),
        ('desired_orientation'      , 'struct Quaternion', ...     ),
    )

    with Meta.enter('struct GNCContext'):
        for field in FIELDS:
            Meta.line(f'''
                {field.type} {field.name};
            ''')



    # Everything here below is just so we can print out the data in the `GNCContext`
    # structure in a nice looking output; see `DemoGNC` for its usage.

    with Meta.enter('''
        static void
        stlink_tx_GNCContext(struct GNCContext context)
    '''):

        format_string_argument_pairs = []

        for field in FIELDS:

            format_string = field.format
            argument      = f'context.{field.name}'

            if format_string is ...:

                match field.type:

                    case None:
                        format_string = None
                        argument      = None

                    case 'i8' | 'i16' | 'i32' | 'i64':
                        format_string = '%d'
                        argument      = f'context.{field.name}'

                    case 'u8' | 'u16' | 'u32' | 'u64':
                        format_string = '%u'
                        argument      = f'context.{field.name}'

                    case 'b8' | 'b16' | 'b32' | 'b64':
                        format_string = '%s'
                        argument      = f'context.{field.name} ? "true " : "false"'

                    case 'f32' | 'f64':
                        format_string = '%f'
                        argument      = f'context.{field.name}'

                    case 'struct Matrix_3x1':
                        format_string = '< (%f) (%f) (%f) >'
                        argument      = (
                            f'context.{field.name}.rows[0][0], '
                            f'context.{field.name}.rows[1][0], '
                            f'context.{field.name}.rows[2][0]'
                        )

                    case 'struct Quaternion':
                        format_string = '(%f) + (%f) i + (%f) j + (%f) k'
                        argument      = (
                            f'context.{field.name}.s, '
                            f'context.{field.name}.i, '
                            f'context.{field.name}.j, '
                            f'context.{field.name}.k'
                        )

                    case idk:
                        raise NotImplementedError(f"Unhandled formatting for field's type: {repr(field)}.")

            format_string_argument_pairs += [(format_string, argument)]

        whole_format_string = ' | '.join(
            f'.{field.name} = {format_string}'
            for field, (format_string, argument) in zip(FIELDS, format_string_argument_pairs)
            if format_string is not None
        )

        whole_argument = ', '.join(
            argument
            for format_string, argument in format_string_argument_pairs
            if format_string is not None
        )

        Meta.line(f'''
            stlink_tx("{whole_format_string}\\n", {whole_argument});
        ''')

*/

// static void
// GNC_update(const struct GNCInput input, struct GNCContext* context)
// {

//     if (!context)
//         sus;



//     // See if we need to set some default values for the GNC context.

//     if (!context->initialized)
//     {

//         // Let's make sure all values are zero instead of potentially any left-over garbage.

//         memzero(context);



//         // We'll start off as if we lost the target from the moment we have ejected.

//         context->target_found             = false;
//         context->target_conflict_count    = 0;
//         context->target_lost_timestamp_us = input.ejection_timestamp_us;



//         // An actual value will be computed for these fields later on.

//         context->angular_accelerations = (struct Matrix_3x1) {0};
//         context->desired_orientation   = (struct Quaternion) {0};



//         // We're done setting the initial values for the rest of the GNC algorithm to work on.

//         context->initialized = true;

//     }



//     // Apply hesterisis to CVT's target confidence.

//     if (context->target_found)
//     {
//         if (input.most_recent_openmv_reading.computer_vision_confidence)
//         {
//             context->target_conflict_count = 0; // We still see the target!
//         }
//         else if (context->target_conflict_count < 3)
//         {
//             context->target_conflict_count += 1; // Hmm, we're losing the target..?
//         }
//         else
//         {
//             context->target_found             = false; // Target definitely lost!
//             context->target_conflict_count    = 0;
//             context->target_lost_timestamp_us = input.current_timestamp_us;
//         }
//     }
//     else
//     {
//         if (!input.most_recent_openmv_reading.computer_vision_confidence)
//         {
//             context->target_conflict_count = 0; // Target still missing...
//         }
//         else if (context->target_conflict_count < 3)
//         {
//             context->target_conflict_count += 1; // Oh, we're starting to see the target..?
//         }
//         else
//         {
//             context->target_found          = true; // Confident we now see the target!
//             context->target_conflict_count = 0;
//         }
//     }



//     // Determine the orientation the vehicle should have.

//     u32               time_since_ejection_us = input.current_timestamp_us - input.ejection_timestamp_us;
//     struct Matrix_3x6 gain                   = {0};

//     if (context->target_found)
//     {
//         if (input.most_recent_openmv_reading.computer_vision_confidence)
//         {

//             context->desired_orientation =
//                 QUATERNION_from_euler_zyx
//                 (
//                     (struct EulerZYX)
//                     {
//                         .yaw   = input.most_recent_openmv_reading.attitude_yaw,
//                         .pitch = input.most_recent_openmv_reading.attitude_pitch,
//                         .roll  = input.most_recent_openmv_reading.attitude_roll,
//                     }
//                 );

//             gain =
//                 (struct Matrix_3x6) // TODO Determine the gain matrix for this situation.
//                 {
//                     .rows =
//                         {
//                             { 1, 2, 3, 4, 5, 6, },
//                             { 2, 3, 4, 5, 6, 7, },
//                             { 3, 4, 5, 6, 7, 8, },
//                         },
//                 };

//         }
//         else
//         {
//             gain =
//                 (struct Matrix_3x6) // TODO Determine the gain matrix for this situation.
//                 {
//                     .rows =
//                         {
//                             { 1, 2, 3, 4, 5, 6, },
//                             { 2, 3, 4, 5, 6, 7, },
//                             { 3, 4, 5, 6, 7, 8, },
//                         },
//                 };
//         }
//     }
//     else if (time_since_ejection_us < 20'000'000)
//     {

//         context->desired_orientation =
//             QUATERNION_from_euler_zyx
//             (
//                 (struct EulerZYX)
//                 {
//                     .yaw   = 0.0f, // TODO: Yaw value?
//                     .pitch = 0.0f, // Reset pitch so we can do the search process.,
//                     .roll  = 0.0f, // TODO: Roll value?
//                 }
//             );

//         gain =
//             (struct Matrix_3x6) // TODO Determine the gain matrix for this situation.
//             {
//                 .rows =
//                     {
//                         { 1, 2, 3, 4, 5, 6, },
//                         { 2, 3, 4, 5, 6, 7, },
//                         { 3, 4, 5, 6, 7, 8, },
//                     },
//             };

//     }
//     else // Do the process of searching.
//     {

//         f32 t = 0.0f;

//         context->desired_orientation =
//             QUATERNION_from_euler_zyx
//             (
//                 (struct EulerZYX)
//                 {
//                     .yaw   = t, // TODO: Function of t?
//                     .pitch = arm_sin_f32(t), // TODO: Function of t?
//                     .roll  = 0, // TODO: Function of t?
//                 }
//             );

//         gain =
//             (struct Matrix_3x6) // TODO Determine the gain matrix for this situation.
//             {
//                 .rows =
//                     {
//                         { 1, 2, 3, 4, 5, 6, },
//                         { 2, 3, 4, 5, 6, 7, },
//                         { 3, 4, 5, 6, 7, 8, },
//                     },
//             };

//     }

// }


static void
GNC_update(const struct GNCInput input, struct GNCContext* context)
{

    if (!context)
        sus;


    u32     time_since_ejection_us = input.current_timestamp_us - input.ejection_timestamp_us;
    u32     time_since_target_lost_us = input.current_timestamp_us - context->target_lost_timestamp_us;
    u32     time_since_target_found_us = input.current_timestamp_us - context->target_found_timestamp_us;
    u32     time_to_start_us = 10'000'000;
    u32     time_to_align_us = 20'000'000;
    u32     search_rate = 0.17453292519943296f; // 10 degrees per second in radians per second.
    u32     rotor_inertia = 0.01f; // TODO: Get actual value for this.


    // Opertaion modes:
    // (0) Ejection: Control disabled, tracking only, obtain heading estimate (VNKMD OFF)
    // (1) Alignment: Control enabled, align to target
    //      -> Target Rates set to zero
    //      -> Drive pitch and roll error to zero, no yaw error control
    // (2) Search: Control enabled, search for target
    //      -> Target yaw rate set to constant
    //      -> Drive pitch and roll erros to zero, no yaw error control
    i32     operation_mode = 0; 
    
    struct Matrix_6x1 state = {0}; // TODO: Fill in the state vector with the actual values we need for the GNC algorithm to work on.
    struct Matrix_6x3 gain  = {0}; // TODO: Determine the gain matrix for this situation.
    struct Matrix_3x1 rates_target = {0}; // TODO: Determine the target rates based on the desired orientation and the current orientation.
    struct Quaternion quaternion_error = {0}; // TODO: Compute the quaternion error between the current orientation and the desired orientation.
    struct Quaternion quaternion_target = {0}; // TODO: Determine the target quaternion based on the desired orientation (which is determined by the CVT readings if we have a target, or some search pattern if we don't).
    struct Matrix_3x1 rates_error = {0}; // TODO: Determine the error in the target rates based on the current rates and the target rates.
    struct Matrix_3x1 control_torques = {0}; // TODO: Implement the control law to compute the angular accelerations based on the state and the gain.

    // User-defined estimate for initial target
    struct Quaternion quaternion_intial_target = 
        (struct Quaternion)
        {
            .s = input.most_recent_imu.QuatS,
            .i = input.most_recent_imu.QuatX,
            .j = input.most_recent_imu.QuatY,
            .k = input.most_recent_imu.QuatZ,
        };

    // Convert quaternion 
    struct Quaternion quaternion_current =
        (struct Quaternion)
        {
            .s = input.most_recent_imu.QuatS,
            .i = input.most_recent_imu.QuatX,
            .j = input.most_recent_imu.QuatY,
            .k = input.most_recent_imu.QuatZ,
        };

    struct Matrix_3x1 rates_current_body = {0};
    rates_current_body.rows[0][0] = input.most_recent_imu.GyroX;
    rates_current_body.rows[1][0] = input.most_recent_imu.GyroY;
    rates_current_body.rows[2][0] = input.most_recent_imu.GyroZ;

    struct Matrix_3x1 rates_current_NED = vector_from_body_to_NED(quaternion_current, rates_current_body);

    
    if (time_since_ejection_us < time_to_start_us)
    {
        // Motors Disabled
        // Send VNKMD OFF
        operation_mode = 0;
    }
    else if (time_since_ejection_us < (time_to_start_us + time_to_align_us))
    {
        // Enable Motors
        // Send VMKMD ON
        // Align to intial target
        // Operation Mode = 1
        // TODO: Implement the alignment logic and operation mode setting.

        operation_mode = 1;
        quaternion_target = quaternion_intial_target;
    }
    else
    {
        if (!context->target_found)
        {
            if (time_since_target_lost_us < time_to_align_us)
            {
                // Align to intial target
                // Operation Mode = 1
                operation_mode = 1;
                
            }
            else
            {
                // Search for Target
                operation_mode = 2;
                rates_target.rows[2][0] = search_rate; // Search pattern: Yaw rate is constant.
    

                quaternion_target =
                    QUATERNION_from_euler_zyx
                    (
                        (struct EulerZYX)
                        {
                            .yaw   = 0.0f, 
                            .pitch = 0.0f, 
                            .roll  = 0.0f, 
                        }
                    );
            }
        else
        {
            // Track Target
         
            if (time_since_target_found_us < time_to_align_us)
            {
                // Align to target
                operation_mode = 1;
                quaternion_target = 
                    Quaternion_from_euler_zyx
                    (
                        (struct EulerZYX)
                        {
                            .yaw   = input.most_recent_openmv_reading.attitude_yaw,
                            .pitch = input.most_recent_openmv_reading.attitude_pitch,
                            .roll  = input.most_recent_openmv_reading.attitude_roll,
                        }
                    );
                // gain_select = no yaw error control, min roll error control, max pitch error control
            }
            else
            {
                // Rotate about down (D) axis
                operation_mode = 2;
                rates_target.rows[2][0] = search_rate; // Search pattern: Yaw rate is constant.
               
                quaternion_target = 
                    Quaternion_from_euler_zyx
                    (
                        (struct EulerZYX)
                        {
                            .yaw   = input.most_recent_openmv_reading.attitude_yaw,
                            .pitch = input.most_recent_openmv_reading.attitude_pitch,
                            .roll  = input.most_recent_openmv_reading.attitude_roll,
                        }
                    );
                    
                // gain_select = no yaw error control, min roll error control, min pitch error control
                // max yaw rate control, min roll rate control, min pitch rate control
            }


        }


    }

    quaternion_error = Quaternion_multiply(quaternion_current, QUATERNION_conjugate(quaternion_target));
    rates_error.rows[1][0] = rates_target.rows[0][0] - rates_current_body.rows[0][0];
    rates_error.rows[2][0] = rates_target.rows[1][0] - rates_current_body.rows[1][0];
    rates_error.rows[3][0] = rates_target.rows[2][0] - rates_current_body.rows[2][0];


    state.rows[0][0] = quaternion_error.i;
    state.rows[1][0] = quaternion_error.j;
    state.rows[2][0] = quaternion_error.k;
    state.rows[3][0] = rates_error.rows[0][0];
    state.rows[4][0] = rates_error.rows[1][0];
    state.rows[5][0] = rates_error.rows[2][0];

    gain = gain_select(quaternion_error, rates_error, operation_mode); // TODO: Implement the gain selection logic based on the current state and operation mode.

    control_torques = MATRIX_multiply(gain, state); // TODO: Implement the control law to compute the angular accelerations based on the gain and the state.

    context->control_accelerations.rows[0][0] = control_torques.rows[0][0] / rotor_inertia;
    context->control_accelerations.rows[1][0] = control_torques.rows[1][0] / rotor_inertia;
    context->control_accelerations.rows[2][0] = control_torques.rows[2][0] / rotor_inertia;

    return context->control_accelerations;
}