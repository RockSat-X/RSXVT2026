////////////////////////////////////////////////////////////////////////////////



#define LIS2MDL_SEVEN_BIT_ADDRESS 0x1E



static const struct { u8 address; u8 value; } LIS2MDL_INITIALIZATION_SEQUENCE[] =
    {
        {
            0x60,         // CFG_REG_A.
              (1 << 6)    // Reboot the magnetometer's memory content.
            | (1 << 5)    // Reset configuration and user registers.
        },
        {
            0x60,         // CFG_REG_A.
              (1    << 7) // Enable temperature compensation, as required.
            | (0    << 4) // Don't use low-power mode so we can get full resolution.
            | (0b11 << 2) // Set the rate the data is outputted.
            | (0b00 << 0) // Continuously measure sensor data.
        },
        {
            0x61,         // CFG_REG_B.
              (1 << 1)    // Enable offset cancellation.
            | (1 << 0)    // Enable digital low-pass filter.
        },
        {
            0x62,         // CFG_REG_C.
              (1 << 4)    // Prevent incorrect data from being read for 16-bit words.
            | (1 << 0)    // "If 1, the data-ready signal [...] is driven on the INT/DRDY pin."
        },
    };



struct LIS2MDLMeasurement
{
    u8  status;      // STATUS_REG.
    i16 x;           // OUTX_L_REG, OUTX_H_REG.
    i16 y;           // OUTY_L_REG, OUTY_H_REG.
    i16 z;           // OUTZ_L_REG, OUTZ_H_REG.
    i16 temperature; // TEMP_OUT_L_REG, TEMP_OUT_H_REG.
};

enum LIS2MDLDriverState : u32
{
    LIS2MDLDriverState_initing,
    LIS2MDLDriverState_idle,
    LIS2MDLDriverState_reading_measurement,
};

struct LIS2MDLDriver
{
    enum LIS2MDLDriverState   state;
    i32                       initialization_sequence_index;
    struct LIS2MDLMeasurement freshest_measurement;
    struct LIS2MDLMeasurement measurements[64];
    volatile u32              reader;
    volatile u32              writer;
};

static struct LIS2MDLDriver _LIS2MDL_driver = {0};



static_assert(IS_POWER_OF_TWO(countof(_LIS2MDL_driver.measurements)));



////////////////////////////////////////////////////////////////////////////////



static void
LIS2MDL_init(void)
{
    NVIC_SET_PENDING(I2C1_EV); // TODO Coupled.
}



INTERRUPT_EXTIx_lis2mdl_data_ready
{
    NVIC_SET_PENDING(I2C1_EV); // TODO Coupled.
}



static useret b32
LIS2MDL_pop_measurement(struct LIS2MDLMeasurement* dst)
{

    b32 got_measurement = _LIS2MDL_driver.reader != _LIS2MDL_driver.writer;

    if (got_measurement)
    {
        i32 index = _LIS2MDL_driver.reader % countof(_LIS2MDL_driver.measurements);
        *dst                    = _LIS2MDL_driver.measurements[index];
        _LIS2MDL_driver.reader += 1;
    }
    else
    {
        *dst = (struct LIS2MDLMeasurement) {0};
    }

    return got_measurement;

}



INTERRUPT_I2Cx_primary // TODO Coupled.
{

    switch (_LIS2MDL_driver.state)
    {



        // The sensor's registers needs to be configured.

        case LIS2MDLDriverState_initing: switch (event)
        {

            case I2CMasterCallbackEvent_can_schedule_next_transfer:
            {

                if (_LIS2MDL_driver.initialization_sequence_index >= countof(LIS2MDL_INITIALIZATION_SEQUENCE))
                    panic;

                I2C_initiate_transfer
                (
                    I2CHandle_primary, // TODO Coupled.
                    LIS2MDL_SEVEN_BIT_ADDRESS,
                    I2CAddressType_seven,
                    I2COperation_write,
                    (u8*) &LIS2MDL_INITIALIZATION_SEQUENCE[_LIS2MDL_driver.initialization_sequence_index],
                    sizeof(LIS2MDL_INITIALIZATION_SEQUENCE[_LIS2MDL_driver.initialization_sequence_index])
                );

            } break;

            case I2CMasterCallbackEvent_transfer_done:
            {

                if (_I2C_drivers[I2CHandle_primary].master.error) // TODO Coupled.
                    sorry

                _LIS2MDL_driver.initialization_sequence_index += 1;

                if (_LIS2MDL_driver.initialization_sequence_index < countof(LIS2MDL_INITIALIZATION_SEQUENCE))
                {
                    // The sensor still has registers to be configured.
                }
                else // We're done configuring the sensor's registers.
                {
                    _LIS2MDL_driver.state = LIS2MDLDriverState_idle;
                }

            } break;

            default: panic;

        } break;



        // No transfer is currently being done;
        // see if we can poll a measurement from the sensor.

        case LIS2MDLDriverState_idle: switch (event)
        {

            case I2CMasterCallbackEvent_can_schedule_next_transfer:
            {

                if (GPIO_READ(lis2mdl_data_ready))
                {
                    // Set the address of where to start
                    // reading the sensor's register data.
                    I2C_initiate_transfer
                    (
                        I2CHandle_primary, // TODO Coupled.
                        LIS2MDL_SEVEN_BIT_ADDRESS,
                        I2CAddressType_seven,
                        I2COperation_write,
                        &(u8) { 0x67 },
                        1
                    );
                }

            } break;

            case I2CMasterCallbackEvent_transfer_done:
            {

                if (_I2C_drivers[I2CHandle_primary].master.error) // TODO Coupled.
                    sorry

                _LIS2MDL_driver.state = LIS2MDLDriverState_reading_measurement;

            } break;

            default: panic;

        } break;



        // Now actually read the sensor data.

        case LIS2MDLDriverState_reading_measurement: switch (event)
        {

            case I2CMasterCallbackEvent_can_schedule_next_transfer:
            {
                I2C_initiate_transfer
                (
                    I2CHandle_primary, // TODO Coupled.
                    LIS2MDL_SEVEN_BIT_ADDRESS,
                    I2CAddressType_seven,
                    I2COperation_read,
                    (u8*) &_LIS2MDL_driver.freshest_measurement,
                    sizeof(_LIS2MDL_driver.freshest_measurement)
                );
            } break;

            case I2CMasterCallbackEvent_transfer_done:
            {

                if (_I2C_drivers[I2CHandle_primary].master.error) // TODO Coupled.
                    sorry



                // See if we can insert the measurement into the ring-buffer.

                if (_LIS2MDL_driver.writer - _LIS2MDL_driver.reader < countof(_LIS2MDL_driver.measurements))
                {
                    i32 index = _LIS2MDL_driver.writer % countof(_LIS2MDL_driver.measurements);
                    _LIS2MDL_driver.measurements[index]  = _LIS2MDL_driver.freshest_measurement;
                    _LIS2MDL_driver.writer              += 1;
                }



                // Now back to waiting for the sensor
                // to set its data-ready signal.

                _LIS2MDL_driver.state = LIS2MDLDriverState_idle;

            } break;

            default: panic;

        } break;



        default: panic;

    }

}
