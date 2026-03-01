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
        0xA1,                      // Set the order of the columns (i.e. draw left-to-right or right-to-left).
        0xC8,                      // Set te order of the COM scan (i.e. draw bottom-up or top-down).
        0x81, 0xFF               , // "Set Contrast Control" TODO Play with brightness?
        0xD5, (8 << 4) | (0 << 0), // Set the clock divider and oscillator frequency. TODO Play with flashing?
        0x8D, 0x14               , // "Enable charge pump during display on".
        0xD9, 0x11               , // Sets the pre-charge period for phase 1 and 2; correlated with refresh rate.
        0xAF,                      // "Display ON in normal mode".
    };

#include "SSD1306_FONT.meta"
/* #meta

    with Meta.enter('static const u8 SSD1306_FONT[256][8] ='):

        TABLE = { # Reference font repository: @/url:`github.com/dhepper/font8x8`.
            0         : (0x55, 0x00, 0xAA, 0x00, 0x55, 0x00, 0xAA, 0x00),
            ord(' ' ) : (0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            ord('!' ) : (0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00),
            ord('"' ) : (0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
            ord('#' ) : (0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00),
            ord('$' ) : (0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00),
            ord('%' ) : (0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00),
            ord('&' ) : (0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00),
            ord("'" ) : (0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00),
            ord('(' ) : (0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00),
            ord(')' ) : (0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00),
            ord('*' ) : (0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00),
            ord('+' ) : (0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00),
            ord(',' ) : (0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06),
            ord('-' ) : (0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00),
            ord('.' ) : (0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00),
            ord('/' ) : (0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00),
            ord('0' ) : (0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00),
            ord('1' ) : (0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00),
            ord('2' ) : (0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00),
            ord('3' ) : (0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00),
            ord('4' ) : (0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00),
            ord('5' ) : (0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00),
            ord('6' ) : (0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00),
            ord('7' ) : (0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00),
            ord('8' ) : (0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00),
            ord('9' ) : (0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00),
            ord(':' ) : (0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00),
            ord(';' ) : (0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06),
            ord('<' ) : (0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00),
            ord('=' ) : (0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00),
            ord('>' ) : (0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00),
            ord('?' ) : (0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00),
            ord('@' ) : (0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00),
            ord('A' ) : (0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00),
            ord('B' ) : (0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00),
            ord('C' ) : (0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00),
            ord('D' ) : (0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00),
            ord('E' ) : (0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00),
            ord('F' ) : (0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00),
            ord('G' ) : (0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00),
            ord('H' ) : (0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00),
            ord('I' ) : (0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00),
            ord('J' ) : (0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00),
            ord('K' ) : (0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00),
            ord('L' ) : (0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00),
            ord('M' ) : (0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00),
            ord('N' ) : (0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00),
            ord('O' ) : (0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00),
            ord('P' ) : (0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00),
            ord('Q' ) : (0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00),
            ord('R' ) : (0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00),
            ord('S' ) : (0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00),
            ord('T' ) : (0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00),
            ord('U' ) : (0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00),
            ord('V' ) : (0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00),
            ord('W' ) : (0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00),
            ord('X' ) : (0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00),
            ord('Y' ) : (0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00),
            ord('Z' ) : (0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00),
            ord('[' ) : (0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00),
            ord('\\') : (0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00),
            ord(']' ) : (0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00),
            ord('^' ) : (0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00),
            ord('_' ) : (0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF),
            ord('`' ) : (0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00),
            ord('a' ) : (0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00),
            ord('b' ) : (0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00),
            ord('c' ) : (0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00),
            ord('d' ) : (0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00),
            ord('e' ) : (0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00),
            ord('f' ) : (0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00),
            ord('g' ) : (0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F),
            ord('h' ) : (0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00),
            ord('i' ) : (0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00),
            ord('j' ) : (0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E),
            ord('k' ) : (0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00),
            ord('l' ) : (0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00),
            ord('m' ) : (0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00),
            ord('n' ) : (0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00),
            ord('o' ) : (0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00),
            ord('p' ) : (0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F),
            ord('q' ) : (0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78),
            ord('r' ) : (0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00),
            ord('s' ) : (0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00),
            ord('t' ) : (0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00),
            ord('u' ) : (0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00),
            ord('v' ) : (0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00),
            ord('w' ) : (0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00),
            ord('x' ) : (0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00),
            ord('y' ) : (0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F),
            ord('z' ) : (0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00),
            ord('{' ) : (0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00),
            ord('|' ) : (0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00),
            ord('}' ) : (0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00),
            ord('~' ) : (0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
        }



        # The font repository we're referencing has their font in row-major order,
        # but with how the SSD1306 display we're working with works, we need it to
        # be column-major, so we're going to have to transpose the font.

        for character_code in range(256):

            character_rows    = TABLE[character_code if character_code in TABLE else 0]
            character_columns = []

            for bit_i in range(8):

                column = 0

                for row_i, row in enumerate(character_rows):
                    column |= ((row >> bit_i) & 1) << row_i

                character_columns += [column]

            Meta.line(f'''
                [{character_code :<3}] = {{ {', '.join(f'0x{byte :02X}' for byte in character_columns) } }},
            ''')

*/

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
            .handle       = I2CHandle_ssd1306,
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



static useret enum SSD1306RefreshResult : u32
{
    SSD1306RefreshResult_success,
    SSD1306RefreshResult_i2c_transfer_error,
    SSD1306RefreshResult_bug = BUG_CODE,
}
SSD1306_refresh(void)
{

    // Reset the draw pointer to the beginning of the display.

    {

        static const u8 SSD1306_REPOSITION[] =
            {
                (0 << 6),   // Interpret the following bytes as commands. @/pg 20/sec 8.1.5.2/`SSD1306`.
                0x00,       // Reset the column address. @/pg 30/tbl 3/`SSD1306`.
                0x10,       // "
                0xB0,       // Reset the page index. @/pg 31/tbl 3/`SSD1306`.
            };

        struct I2CDoJob job =
            {
                .handle       = I2CHandle_ssd1306,
                .address_type = I2CAddressType_seven,
                .address      = SSD1306_SEVEN_BIT_ADDRESS,
                .writing      = true,
                .repeating    = false,
                .pointer      = (u8*) SSD1306_REPOSITION,
                .amount       = countof(SSD1306_REPOSITION),
            };

        for (b32 yield = false; !yield;)
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
                    yield = true;
                } break;

                case I2CDoResult_no_acknowledge        : return SSD1306RefreshResult_i2c_transfer_error;
                case I2CDoResult_clock_stretch_timeout : return SSD1306RefreshResult_i2c_transfer_error;
                case I2CDoResult_bus_misbehaved        : return SSD1306RefreshResult_i2c_transfer_error;
                case I2CDoResult_watchdog_expired      : return SSD1306RefreshResult_i2c_transfer_error;
                case I2CDoResult_bug                   : bug;
                default                                : bug;

            }

        }

    }



    // Transfer the pixel data.

    {

        struct I2CDoJob job =
            {
                .handle       = I2CHandle_ssd1306,
                .address_type = I2CAddressType_seven,
                .address      = SSD1306_SEVEN_BIT_ADDRESS,
                .writing      = true,
                .repeating    = false,
                .pointer      = _SSD1306_data_payload,
                .amount       = sizeof(_SSD1306_data_payload),
            };

        for (b32 yield = false; !yield;)
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
                    yield = true;
                } break;

                case I2CDoResult_no_acknowledge        : return SSD1306RefreshResult_i2c_transfer_error;
                case I2CDoResult_clock_stretch_timeout : return SSD1306RefreshResult_i2c_transfer_error;
                case I2CDoResult_bus_misbehaved        : return SSD1306RefreshResult_i2c_transfer_error;
                case I2CDoResult_watchdog_expired      : return SSD1306RefreshResult_i2c_transfer_error;
                case I2CDoResult_bug                   : bug;
                default                                : bug;

            }

        }

    }



    return SSD1306RefreshResult_success;


}



struct SSD1306WriteFormatContext
{
    i32 current_pixel_x;
    i32 page_y;
};

static void
_SSD1306_write_format_callback(char byte, void* void_context)
{

    struct SSD1306WriteFormatContext* context = void_context;

    for (i32 character_column = 0; character_column < countof(SSD1306_FONT[(u8) byte]); character_column += 1)
    {

        if // Ensure within bounds.
        (
            0 <= context->page_y          && context->page_y          < SSD1306_ROWS / bitsof(u8) &&
            0 <= context->current_pixel_x && context->current_pixel_x < SSD1306_COLUMNS
        )
        {
            (*_SSD1306_framebuffer)[context->page_y][context->current_pixel_x] = SSD1306_FONT[(u8) byte][character_column];
        }

        context->current_pixel_x += 1;

    }

}

static void __attribute__((format(printf, 3, 4)))
SSD1306_write_format(i32 pixel_x, i32 page_y, char* format, ...)
{

    va_list arguments = {0};
    va_start(arguments);

    struct SSD1306WriteFormatContext context =
        {
            .current_pixel_x = pixel_x,
            .page_y          = page_y,
        };

    vfctprintf
    (
        &_SSD1306_write_format_callback,
        &context,
        format,
        arguments
    );

    va_end(arguments);

}
