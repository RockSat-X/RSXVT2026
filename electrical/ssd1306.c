#ifndef SSD1306_SEVEN_BIT_ADDRESS
#define SSD1306_SEVEN_BIT_ADDRESS 0x3C
#endif



#define SSD1306_ROWS    64
#define SSD1306_COLUMNS 128



static const u8 SSD1306_INITIALIZATION_SEQUENCE[] =
    {
        (0 << 6),                  // Interpret the following bytes as commands. @/pg 20/sec 8.1.5.2/`SSD1306`.
        0xA8, SSD1306_ROWS - 1   , // Indicate the amount of rows the display has.
        0x20, 0x00               , // "Horizontal Addressing Mode".
        0x81, 0xFF               , // "Set Contrast Control" TODO Play with brightness?
        0xD5, (8 << 4) | (0 << 0), // Set the clock divider and oscillator frequency. TODO Play with flashing?
        0x8D, 0x14               , // "Enable charge pump during display on".
        0xD9, 0x11               , // Sets the pre-charge period for phase 1 and 2; correlated with refresh rate.
        0xAF,                      // "Display ON in normal mode".
    };



#define   _SSD1306_framebuffer (u8(*)[SSD1306_ROWS / bitsof(u8)][SSD1306_COLUMNS]) (_SSD1306_data_payload + 1)
static u8 _SSD1306_data_payload[1 + SSD1306_ROWS * SSD1306_COLUMNS / bitsof(u8)] =
    {

        // To send pixel data to the screen, we first send a control byte that indicates
        // the following transfer consists entirely of data bytes. To make things work out
        // nicely between the I2C transfer and the framebuffer, the control byte is placed
        // right before `_SSD1306_framebuffer`, so we can then just use `_SSD1306_data_payload`
        // to do the entire transfer in one go.
        //
        // @/pg 20/sec 8.1.5.2/`SSD1306`.

        [0] = (1 << 6),
    };



////////////////////////////////////////////////////////////////////////////////



static useret enum SSD1306ReinitResult : u32
{
    SSD1306ReinitResult_success,
    SSD1306ReinitResult_failed_to_initialize_with_i2c,
    SSD1306ReinitResult_bug = BUG_CODE,
}
SSD1306_reinit(void)
{



    // Reset-cycle the I2C peripheral.

    enum I2CReinitResult i2c_reinit_result = I2C_reinit(I2CHandle_ssd1306);

    switch (i2c_reinit_result)
    {
        case I2CReinitResult_success : break;
        case I2CReinitResult_bug     : bug;
        default                      : bug;
    }



    // Configure the registers of the SSD1306 driver.

    struct I2CDoJob job =
        {
            .handle       = I2CHandle_ssd1306, // TODO Coupled.
            .address_type = I2CAddressType_seven,
            .address      = SSD1306_SEVEN_BIT_ADDRESS,
            .writing      = true,
            .repeating    = false,
            .pointer      = (u8*) SSD1306_INITIALIZATION_SEQUENCE,
            .amount       = countof(SSD1306_INITIALIZATION_SEQUENCE),
        };

    while (true)
    {
        enum I2CDoResult result = I2C_do(&job);

        switch (result)
        {

            case I2CDoResult_working:
            {
                FREERTOS_delay_ms(1); // Transfer busy...
            } break;

            case I2CDoResult_success:
            {
                return SSD1306ReinitResult_success;
            } break;

            case I2CDoResult_no_acknowledge        : return SSD1306ReinitResult_failed_to_initialize_with_i2c;
            case I2CDoResult_clock_stretch_timeout : return SSD1306ReinitResult_failed_to_initialize_with_i2c;
            case I2CDoResult_bus_misbehaved        : return SSD1306ReinitResult_failed_to_initialize_with_i2c;
            case I2CDoResult_watchdog_expired      : return SSD1306ReinitResult_failed_to_initialize_with_i2c;
            case I2CDoResult_bug                   : bug;
            default                                : bug;

        }

    }

}
