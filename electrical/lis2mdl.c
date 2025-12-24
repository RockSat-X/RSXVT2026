#define LIS2MDL_SEVEN_BIT_ADDRESS 0x1E

#include "LIS2MDL_registers.meta"
/* #meta



    # Some info on all of the registers of the LIS2MDL.

    REGISTER_MAP = (
        ('OFFSET_X_REG_L', 0x45),
        ('OFFSET_X_REG_H', 0x46),
        ('OFFSET_Y_REG_L', 0x47),
        ('OFFSET_Y_REG_H', 0x48),
        ('OFFSET_Z_REG_L', 0x49),
        ('OFFSET_Z_REG_H', 0x4A),
        ('WHO_AM_I'      , 0x4F),
        ('CFG_REG_A'     , 0x60),
        ('CFG_REG_B'     , 0x61),
        ('CFG_REG_C'     , 0x62),
        ('INT_CRTL_REG'  , 0x63),
        ('INT_SOURCE_REG', 0x64),
        ('INT_THS_L_REG' , 0x65),
        ('INT_THS_H_REG' , 0x66),
        ('STATUS_REG'    , 0x67),
        ('OUTX_L_REG'    , 0x68),
        ('OUTX_H_REG'    , 0x69),
        ('OUTY_L_REG'    , 0x6A),
        ('OUTY_H_REG'    , 0x6B),
        ('OUTZ_L_REG'    , 0x6C),
        ('OUTZ_H_REG'    , 0x6D),
        ('TEMP_OUT_L_REG', 0x6E),
        ('TEMP_OUT_H_REG', 0x6F),
    )

    Meta.enums('LIS2MDLRegister', 'u8', (
        (register_name, f'0x{register_address :02X}')
        for register_name, register_address in REGISTER_MAP
    ))

    Meta.lut('LIS2MDL_REGISTERS', (
        (
            ('name'   , f'"{register_name}"'              ),
            ('address', f'LIS2MDLRegister_{register_name}'),
        ) for register_name, register_address in REGISTER_MAP
    ))



    # Make array of writes to the LIS2MDL sensor to configure it.

    INITIALIZATION_SEQUENCE = (

        ('CFG_REG_A', (1    << 6) # Reboot the magnetometer's memory content.
                    | (1    << 5) # Reset configuration and user registers.
        ),
        ('CFG_REG_A', (1    << 7) # Enable temperature compensation, as required.
                    | (0    << 4) # Don't use low-power mode so we can get full resolution.
                    | (0b11 << 2) # Set the rate the data is outputted.
                    | (0b00 << 0) # Continuously measure sensor data.
        ),
        ('CFG_REG_B', (1    << 1) # Enable offset cancellation.
                    | (1    << 0) # Enable digital low-pass filter.
        ),
        ('CFG_REG_C', (1    << 4) # TODO Something about preventing async read corruption?
        ),

    )

    with Meta.enter('static const u8 LIS2MDL_INITIALIZATION_SEQUENCE[][2] ='):

        for register_name, register_value in INITIALIZATION_SEQUENCE:

            Meta.line(f'''
                {{ LIS2MDLRegister_{register_name}, 0x{register_value :02X} }},
            ''')

*/



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
                &(u8) { LIS2MDLRegister_STATUS_REG },
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
