#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"
#include "spi.c"
#include "sd.c"
#include "filesystem.c"
#include "stepper.c"
#include "buzzer.c"
#include "matrix.c"



////////////////////////////////////////////////////////////////////////////////
//
// VN-100 stuff.
//



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

static RingBuffer(struct VN100Packet, 4) VN100_ring_buffer = {0};



////////////////////////////////////////////////////////////////////////////////
//
// Pre-scheduler initialization.
//



extern noret void
main(void)
{

    // General peripheral initializations.

    STPY_init();
    UXART_init(UXARTHandle_stlink);
    UXART_init(UXARTHandle_stepper_uart);
    UXART_init(UXARTHandle_vn100_esp32);
    {
        enum I2CReinitResult result = I2C_reinit(I2CHandle_vehicle_interface);
        switch (result)
        {
            case I2CReinitResult_success : break;
            case I2CReinitResult_bug     : sorry
            default                      : sorry
        }
    }
    SPI_reinit(SPIHandle_openmv);



    // Set the prescaler that'll affect all timers' kernel frequency.

    CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



    // Initialize timekeeping.

    {

        // Enable the timekeeping's timer peripheral.

        CMSIS_PUT(TIMEKEEPING_TIMER_ENABLE, true);



        // Configure the divider to set the rate at
        // which the timer's counter will increment.

        CMSIS_SET(TIMEKEEPING_TIMER, PSC, PSC, TIMEKEEPING_DIVIDER);



        // Trigger an update event so that the shadow registers
        // are what we initialize them to be.
        // The hardware uses shadow registers in order for updates
        // to these registers not result in a corrupt timer output.

        CMSIS_SET(TIMEKEEPING_TIMER, EGR, UG, true);



        // Enable the timekeeping timer's counter.

        CMSIS_SET(TIMEKEEPING_TIMER, CR1, CEN, true);

    }



    // More peripheral initializations that depend on the above initializations.

    BUZZER_partial_init();
    STEPPER_partial_reinit();



    // TODO Dumb demo of matrix stuff.

    {

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

        struct Matrix* control_output = Matrix(3, 1);



        MATRIX_multiply(control_output, gain, state);

        MATRIX_multiply_add(control_output, control_output, -2);



        stlink_tx("Resultant Matrix is:\n");

        MATRIX_stlink_tx(control_output);

    }



    // When the vehicle becomes powered on,
    // it's typically because of the external
    // power suplly through the vehicle interface.
    //
    // TODO Add a delay before we enable the battery?
    // TODO Check if there's actually external power?

    BUZZER_play(BuzzerTune_three_tone);
    BUZZER_spinlock_to_completion();

    GPIO_ACTIVE(battery_allowed);



    // Begin the FreeRTOS task scheduler.

    FREERTOS_init();

}



////////////////////////////////////////////////////////////////////////////////
//
// Update motor angular velocities.
//



static volatile f32 current_angular_acceleration = 0.0f;

FREERTOS_TASK(stepper_motor_controller, 1024, 0)
{

    static f32 current_angular_velocity = 1.0f;

    for (;;)
    {

        #define MAX_ANGULAR_VELOCITY (2.0f * PI * 8.0f)

        b32 max_angular_velocity_has_already_been_reached =
            current_angular_velocity >=  MAX_ANGULAR_VELOCITY ||
            current_angular_velocity <= -MAX_ANGULAR_VELOCITY;



        // Find new angular velocity.

        current_angular_velocity += current_angular_acceleration * (STEPPER_VELOCITY_UPDATE_US / 100'000.0f);



        // Limit the angular velocity.

        b32 max_angular_velocity_reached = false;

        if (current_angular_velocity >= MAX_ANGULAR_VELOCITY)
        {
            current_angular_velocity     = MAX_ANGULAR_VELOCITY;
            max_angular_velocity_reached = true;
        }
        else if (current_angular_velocity <= -MAX_ANGULAR_VELOCITY)
        {
            current_angular_velocity     = -MAX_ANGULAR_VELOCITY;
            max_angular_velocity_reached = true;
        }



        // Indicate that the max angular velocity has been reached.

        GPIO_SET(led_channel_red, max_angular_velocity_reached);

        if (!max_angular_velocity_has_already_been_reached && max_angular_velocity_reached)
        {
            BUZZER_play(BuzzerTune_heartbeat);
        }



        // Queue up the angular velocity.

        while
        (
            !STEPPER_push_angular_velocities
            (
                &(f32[])
                {
                    // TODO: Broken UART line on my PCB. [StepperInstanceHandle_axis_x] = current_angular_velocity,
                    [StepperInstanceHandle_axis_y] = current_angular_velocity,
                    [StepperInstanceHandle_axis_z] = current_angular_velocity,
                }
            )
        );

    }

}



////////////////////////////////////////////////////////////////////////////////
//
// Take user input and do stuff.
//



FREERTOS_TASK(user_inputter, 1024, 0)
{
    for (;;)
    {

        u8 input = {0};
        while (!stlink_rx(&input));

        switch (input)
        {
            case 'j': current_angular_acceleration += -0.1f; break;
            case 'J': current_angular_acceleration += -1.0f; break;
            case 'k': current_angular_acceleration +=  0.1f; break;
            case 'K': current_angular_acceleration +=  1.0f; break;
            case '0': current_angular_acceleration  =  0.0f; break;
            default: break;
        }

    }
}



////////////////////////////////////////////////////////////////////////////////
//
// Periodically log out stuff.
//



FREERTOS_TASK(logger, 2048, 0)
{
    for (;;)
    {
        stlink_tx("Angular acceleration: %.6f\n", current_angular_acceleration);

        struct VN100Packet packet = {0};
        if (RingBuffer_pop_to_latest(&VN100_ring_buffer, &packet))
        {
            stlink_tx
            (
                "QuatX  : %f" "\n"
                "QuatY  : %f" "\n"
                "QuatZ  : %f" "\n"
                "QuatS  : %f" "\n"
                "MagX   : %f" "\n"
                "MagY   : %f" "\n"
                "MagZ   : %f" "\n"
                "AccelX : %f" "\n"
                "AccelY : %f" "\n"
                "AccelZ : %f" "\n"
                "GyroX  : %f" "\n"
                "GyroY  : %f" "\n"
                "GyroZ  : %f" "\n"
                "\n",
                packet.QuatX,
                packet.QuatY,
                packet.QuatZ,
                packet.QuatS,
                packet.MagX,
                packet.MagY,
                packet.MagZ,
                packet.AccelX,
                packet.AccelY,
                packet.AccelZ,
                packet.GyroX,
                packet.GyroY,
                packet.GyroZ
            );
        }

        stlink_tx("OpenMV data:\n");

        while (true)
        {

            SPIBlock* block = RingBuffer_reading_pointer(SPI_ring_buffer(SPIHandle_openmv));

            if (block)
            {

                for (i32 i = 0; i < countof(*block); i += 1)
                {
                    stlink_tx(" 0x%02X", (*block)[i]);
                }

                if (!RingBuffer_pop(SPI_ring_buffer(SPIHandle_openmv), nullptr))
                    sorry

            }
            else
            {
                break;
            }

        }

        stlink_tx("\n\n");

        spinlock_us(100'000);
    }
}



////////////////////////////////////////////////////////////////////////////////
//
// Handle VN-100.
//



FREERTOS_TASK(vn100, 2048, 0)
{
    for (;;)
    {

        // Get the payload.

        u8  payload_buffer[256]      = {0};
        i32 payload_length           = 0;
        i32 checksum_indicator_index = 0;

        while (true)
        {

            u8 data = {0};
            while (!UXART_rx(UXARTHandle_vn100_esp32, &data));

            if (implies(payload_length == 0, data == '$'))
            {
                payload_buffer[payload_length]  = data;
                payload_length                 += 1;

                if (payload_length == countof(payload_buffer))
                {

                    // Ran out of buffer space.
                    // Could be due to corrupt message
                    // where we didn't find the checksum indicator.

                    checksum_indicator_index = 0; // It'll be invalid to try to read the checksum.

                    break;

                }
                else if (checksum_indicator_index && payload_length == checksum_indicator_index + 3)
                {
                    break; // We reached the end of the VN-100 register read payload.
                }
                else if (data == '*')
                {
                    checksum_indicator_index = payload_length - 1;
                }
            }

        }



        // Verify payload integrity. TODO Cite.

        u8 expected_checksum = 0;
        for
        (
            i32 i = 1;
            i < checksum_indicator_index;
            i += 1
        )
        {
            expected_checksum ^= payload_buffer[i];
        }

        u32 received_checksum =
            ((payload_buffer[checksum_indicator_index + 1] - (u32) '0') << 4) |
            ((payload_buffer[checksum_indicator_index + 2] - (u32) '0') << 0);

        b32 checksum_valid = expected_checksum == received_checksum;

        if (!checksum_valid)
        {
            stlink_tx("Bad checksum! `%.*s` (expected 0x%02X)\n", countof(payload_buffer), payload_buffer, expected_checksum);
        }



        // Parse the VN-100 payload.

        else
        {

            struct VN100Packet packet               = {0};
            b32                valid_packet         = true;
            i32                field_position_index = 0;
            i32                field_start_index    = 0;
            i32                field_length         = 0;

            while (true)
            {

                b32 found_comma = payload_buffer[field_start_index + field_length] == ',';
                b32 reached_end = field_start_index + field_length >= checksum_indicator_index;

                if (found_comma || reached_end)
                {

                    // Magic starting token.

                    if (field_position_index == 0)
                    {
                        valid_packet =
                            !strncmp
                            (
                                (char*) (payload_buffer + field_start_index),
                                "$VNRRG",
                                (u32) field_length
                            );
                    }



                    // Register ID.

                    else if (field_position_index == 1)
                    {
                        valid_packet =
                            !strncmp
                            (
                                (char*) (payload_buffer + field_start_index),
                                "15",
                                (u32) field_length
                            );
                    }



                    // Field values of the register.

                    else if (field_position_index - 2 < sizeof(struct VN100Packet) / sizeof(f32))
                    {

                        // Parse the field as a float, if possible.

                        f32 parsed_value   = 0.0f;
                        b32 negative       = false;
                        f32 decimal_factor = {0};

                        for
                        (
                            i32 index = 0;
                            index < field_length && valid_packet;
                            index += 1
                        )
                        {
                            if (payload_buffer[field_start_index + index] == '+' && index == 0)
                            {
                                negative = false;
                            }
                            else if (payload_buffer[field_start_index + index] == '-' && index == 0)
                            {
                                negative = true;
                            }
                            else if (payload_buffer[field_start_index + index] == '.' && !decimal_factor)
                            {
                                decimal_factor = 0.1f;
                            }
                            else if ('0' <= payload_buffer[field_start_index + index] && payload_buffer[field_start_index + index] <= '9')
                            {
                                f32 digit = (f32) (payload_buffer[field_start_index + index] - '0');

                                if (decimal_factor)
                                {
                                    parsed_value   += decimal_factor * digit;
                                    decimal_factor *= 0.1f;
                                }
                                else
                                {
                                    parsed_value = parsed_value * 10.0f + digit;
                                }
                            }
                            else
                            {
                                valid_packet = false;
                            }
                        }

                        if (valid_packet)
                        {

                            if (negative)
                            {
                                parsed_value = -parsed_value;
                            }

                            ((f32*) &packet)[field_position_index - 2] = parsed_value;

                        }

                    }



                    // Extraneous fields...

                    else
                    {
                        valid_packet = false;
                    }



                    if (!valid_packet)
                    {
                        break; // Abort parsing the received data.
                    }
                    else if (reached_end)
                    {
                        break; // No more data to process.
                    }
                    else // Move onto next field.
                    {
                        field_position_index += 1;
                        field_start_index    += field_length + 1; // +1 to skip the comma.
                        field_length          = 0;
                    }

                }
                else
                {
                    field_length += 1;
                }

            }

            if (!valid_packet)
            {
                // Something was wrong with the received VN-100 payload...
            }
            else if (!RingBuffer_push(&VN100_ring_buffer, &packet))
            {
                // VN-100 data overrun!
            }

        }

    }
}



////////////////////////////////////////////////////////////////////////////////
//
// Handle ESP32 payload transmission.
//



FREERTOS_TASK(esp32, 2048, 0)
{
    for (;;)
    {

        struct PacketESP32 payload =
            {
                .magnetometer_x                          = 3.1f,
                .magnetometer_y                          = 3.2f,
                .magnetometer_z                          = 3.3f,
                .image_chunk                             = { 'M', 'E', 'O', 'W', '!', '?' },
                .nonredundant.quaternion_i               = 0.1f,
                .nonredundant.quaternion_j               = 0.2f,
                .nonredundant.quaternion_k               = 0.3f,
                .nonredundant.quaternion_r               = 0.4f,
                .nonredundant.accelerometer_x            = 1.1f,
                .nonredundant.accelerometer_y            = 1.2f,
                .nonredundant.accelerometer_z            = 1.3f,
                .nonredundant.gyro_x                     = 2.1f,
                .nonredundant.gyro_y                     = 2.2f,
                .nonredundant.gyro_z                     = 2.3f,
                .nonredundant.computer_vision_confidence = -1.0f,
                .nonredundant.timestamp_ms               = 0,
                .nonredundant.sequence_number            = 0,
                .nonredundant.crc                        = 0x00,
            };

        payload.nonredundant.crc = ESP32_calculate_crc((u8*) &payload, sizeof(payload) - sizeof(payload.nonredundant.crc));

        UXART_tx_bytes(UXARTHandle_vn100_esp32, (u8*) &(u16) { PACKET_ESP32_START_TOKEN }, sizeof(u16));
        UXART_tx_bytes(UXARTHandle_vn100_esp32, (u8*) &payload, sizeof(payload));

    }
}



////////////////////////////////////////////////////////////////////////////////
//
// Handle vehicle interface.
//



INTERRUPT_I2Cx_vehicle_interface(enum I2CSlaveCallbackEvent event, u8* data)
{

    static b32 payload_has_valid_data = false;

    switch (event)
    {

        // No sense in having main send data to the vehicle.

        case I2CSlaveCallbackEvent_data_available_to_read:
        {
            // Don't care.
        } break;



        // Send next byte of the vehicle interface payload over.

        case I2CSlaveCallbackEvent_ready_to_transmit_data:
        {

            // Prepare the payload.

            static i32                            byte_index = 0;
            static struct VehicleInterfacePayload payload    = {0};

            if (!payload_has_valid_data)
            {

                byte_index = 0;

                payload =
                    (struct VehicleInterfacePayload)
                    {
                        .timestamp_us = (u16) TIMEKEEPING_COUNTER(),
                        .flags        = 0, // TODO.
                    };

                payload.crc =
                    VEHICLE_INTERFACE_calculate_crc
                    (
                        (u8*) &payload,
                        sizeof(payload) - sizeof(payload.crc)
                    );

                payload_has_valid_data = true;

            }



            // Prepare next byte.

            if (byte_index < sizeof(payload))
            {
                *data = ((u8*) &payload)[byte_index];
            }
            else
            {
                *data = 0xFF; // Garbage.
            }

            byte_index += 1;

        } break;



        // TODO

        case I2CSlaveCallbackEvent_stop_signaled:
        case I2CSlaveCallbackEvent_transmission_initiated:
        case I2CSlaveCallbackEvent_reception_initiated:
        case I2CSlaveCallbackEvent_transmission_repeated:
        case I2CSlaveCallbackEvent_reception_repeated:
        {
            payload_has_valid_data = false;
        } break;


        case I2CSlaveCallbackEvent_bus_misbehaved : sorry // TODO.
        case I2CSlaveCallbackEvent_watchdog_expired : sorry // TODO.

        case I2CSlaveCallbackEvent_clock_stretch_timeout : sorry
        case I2CSlaveCallbackEvent_bug                   : sorry
        default                                          : sorry

    }

}
