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
            (1 << 4)      // TODO Something about preventing async read corruption?
        },
    };



struct LIS2MDLPayload
{
    u8  status;      // STATUS_REG.
    i16 x;           // OUTX_L_REG, OUTX_H_REG.
    i16 y;           // OUTY_L_REG, OUTY_H_REG.
    i16 z;           // OUTZ_L_REG, OUTZ_H_REG.
    i16 temperature; // TEMP_OUT_L_REG, TEMP_OUT_H_REG.
};



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

}



static useret struct LIS2MDLPayload
LIS2MDL_get_payload(void)
{

    struct LIS2MDLPayload payload = {0};

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
                (u8*) &payload,
                sizeof(payload)
            );

        if (error)
            sorry

    }

    return payload;

}
