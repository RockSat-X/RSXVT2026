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
              (1 << 4)    // TODO Something about preventing async read corruption?
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



struct LIS2MDLBuffer
{
    struct LIS2MDLMeasurement measurements[64];
    volatile u32              reader;
    volatile u32              writer;
};



static volatile b32         _LIS2MDL_inited = false;
static struct LIS2MDLBuffer _LIS2MDL_buffer = {0};



static_assert(IS_POWER_OF_TWO(countof(_LIS2MDL_buffer.measurements)));



////////////////////////////////////////////////////////////////////////////////



static void
LIS2MDL_init(void)
{

    // Simply do some writes to the LIS2MDL
    // sensor to get it up and going.

    for (i32 write_index = 0; write_index < countof(LIS2MDL_INITIALIZATION_SEQUENCE); write_index += 1)
    {

        enum I2CMasterError error =
            I2C_blocking_transfer
            (
                I2CHandle_primary,
                LIS2MDL_SEVEN_BIT_ADDRESS,
                I2CAddressType_seven,
                I2COperation_write,
                (u8*) &LIS2MDL_INITIALIZATION_SEQUENCE[write_index],
                sizeof(LIS2MDL_INITIALIZATION_SEQUENCE[write_index])
            );

        if (error)
            sorry

    }

    _LIS2MDL_inited = true;

}



INTERRUPT_EXTIx_lis2mdl_data_ready
{
    if (_LIS2MDL_inited)
    {

        struct LIS2MDLMeasurement measurement = {0};

        {

            enum I2CMasterError error =
                I2C_blocking_transfer
                (
                    I2CHandle_primary,
                    LIS2MDL_SEVEN_BIT_ADDRESS,
                    I2CAddressType_seven,
                    I2COperation_write,
                    &(u8) { 0x67 },
                    1
                );

            if (error)
                sorry

        }
        {

            enum I2CMasterError error =
                I2C_blocking_transfer
                (
                    I2CHandle_primary,
                    LIS2MDL_SEVEN_BIT_ADDRESS,
                    I2CAddressType_seven,
                    I2COperation_read,
                    (u8*) &measurement,
                    sizeof(measurement)
                );

            if (error)
                sorry

        }

        if (_LIS2MDL_buffer.writer - _LIS2MDL_buffer.reader < countof(_LIS2MDL_buffer.measurements))
        {
            i32 index = _LIS2MDL_buffer.writer % countof(_LIS2MDL_buffer.measurements);
            _LIS2MDL_buffer.measurements[index]  = measurement;
            _LIS2MDL_buffer.writer              += 1;
        }
        else
        {
            // Ring-buffer is full; we won't be able
            // to save this measurement from the sensor.
            // It's still important that we nonetheless
            // did the above I2C transaction so that we
            // can clear the data-ready signal.
        }

    }
}



static useret b32
LIS2MDL_pop_measurement(struct LIS2MDLMeasurement* dst)
{

    b32 got_measurement = _LIS2MDL_buffer.reader != _LIS2MDL_buffer.writer;

    if (got_measurement)
    {
        i32 index = _LIS2MDL_buffer.reader % countof(_LIS2MDL_buffer.measurements);
        *dst                    = _LIS2MDL_buffer.measurements[index];
        _LIS2MDL_buffer.reader += 1;
    }
    else
    {
        *dst = (struct LIS2MDLMeasurement) {0};
    }

    return got_measurement;

}
