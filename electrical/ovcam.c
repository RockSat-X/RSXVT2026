#define OVCAM_SEVEN_BIT_ADDRESS 0x3C
#define OVCAM_FRAMEBUFFER_SIZE  (100 * 1024)

#include "OVCAM_defs.meta"
/* #meta

    # The initialization sequence of the OV5640 is... poorly understood.
    # The datasheet provided by OmniVision (version 2.03, May 2011, OV05640-A71A)
    # is not comprehensive nor descriptive. The register writes described
    # below is more-or-less taken from STM32's BSP implementation of the
    # OV5640 driver, but honestly, it's a waste of time to figure out what
    # even is going on here...



    # Some definitions.

    Meta.define('TV_TOKEN_START', f'"{TV_TOKEN.START.decode('UTF-8')}"')
    Meta.define('TV_TOKEN_END'  , f'"{TV_TOKEN.END  .decode('UTF-8')}"')
    Meta.define('TV_WRITE_BYTE' , f'0x{TV_WRITE_BYTE :02X}')



    # Default value for JPEG CTRL3 register.

    jpeg_ctrl3 = sum(
        field.default << bit_i
        for bit_i, field in enumerate(OVCAM_JPEG_CTRL3_FIELDS)
    )



    # List of register writes to perform.

    write_lines = f'''

        0x3103 0x11   # SCCB SYSTEM CTRL.
        0x3008 0x82   # SYSTEM CTROL.
        0x3103 0x03   # SCCB SYSTEM CTRL.

        0x3630 0x36   # Undocumented.
        0x3631 0x0E   # "
        0x3632 0xE2   # "
        0x3633 0x12   # "
        0x3621 0xE0   # "
        0x3704 0xA0   # "
        0x3703 0x5A   # "
        0x3715 0x78   # "
        0x3717 0x01   # "
        0x370B 0x60   # "
        0x3705 0x1A   # "
        0x3905 0x02   # "
        0x3906 0x10   # "
        0x3901 0x0A   # "
        0x3731 0x12   # "
        0x3600 0x08   # "
        0x3601 0x33   # "
        0x302D 0x60   # "
        0x3620 0x52   # "
        0x371B 0x20   # "
        0x471C 0x50   # "

        0x3A02 0x03D8 # AEC MAX EXPO (60Hz).
        0x3A08 0x0127 # AEC B STEP.
        0x3A0A 0x00F6 # "
        0x3A0D 0x04   # AEC CTRL.
        0x3A0E 0x03   # "
        0x3A0F 0x30   # "
        0x3A10 0x28   # "
        0x3A11 0x60   # "
        0x3A13 0x43   # "
        0x3A14 0x03D8 # AEC MAX EXPO (50Hz).
        0x3A18 0xF8   # AEC GAIN CEILING.
        0x3A1B 0x30   # AEC CTRL.
        0x3A1E 0x26   # "
        0x3A1F 0x14   # "

        0x3635 0x13   # Undocumented
        0x3636 0x03   # "
        0x3634 0x40   # "
        0x3622 0x01   # "

        0x3C01 0x34   # 5060HZ CTRL.
        0x3C04 0x28   # "
        0x3C05 0x98   # "
        0x3C06 0x0000 # LIGHTMETER.
        0x3C08 0x012C # "
        0x3C0A 0x9C40 # SAMPLE NUMBER.

        0x3820 0x0620                               # TIMING TC REG.
        0x3814 0x31                                 # TIMING X INC.
        0x3815 0x31                                 # TIMING Y INC.
        0x3800 0x0000                               # TIMING HS.
        0x3802 0x0004                               # TIMING VS.
        0x3804 0x0A3F                               # TIMING HW.
        0x3806 0x079B                               # TIMING VH.
        0x3808 0x{OVCAM_DEFAULT_RESOLUTION[0] :04X} # TIMING DVPHO.
        0x380A 0x{OVCAM_DEFAULT_RESOLUTION[1] :04X} # TIMING DVPVO.

        0x380C 0x0790 # TIMING HTS.
        0x380E 0x0440 # TIMING VTS.
        0x3810 0x0010 # TIMING HOFFSET.
        0x3812 0x0006 # TIMING VOFFSET.

        0x3618 0x00   # Undocumented.
        0x3612 0x29   # "
        0x3708 0x6452 # "
        0x370C 0x03   # "

        0x4001 0x02   # BLC CTRL.
        0x4004 0x02   # "
        0x3000 0x00   # SYSTEM RESET.
        0x3002 0x1C   # "
        0x3004 0xFF   # CLOCK ENABLE.
        0x3006 0xC3   # "
        0x300E 0x58   # MIPI CONTROL.
        0x302E 0x00
        0x4740 0x22   # POLARITY CTRL.
        0x4300 0x30   # FORMAT CONTROL.

        0x4713 0x03                # JPG MODE SELECT.
        0x4403 0x{jpeg_ctrl3 :02X} # JPEG CTRL.
        0x4407 0x04                # "

        0x440E 0x00   # Undocumented.

        0x460B 0x35   # Undocumented.
        0x460C 0x23   # VFIFO CTRL.
        0x4837 0x22   # PCLK PERIOD.

        0x3824 0x02   # Undocumented.

        0x5000 0xA7   # ISP CONTROL.
        0x5001 0xA3   # "

        0x5180 0xFF   # AWB CONTROL.
        0x5181 0xF2   # "
        0x5182 0x00   # "
        0x5183 0x14   # "
        0x5184 0x25   # "
        0x5185 0x24   # "
        0x5186 0x09   # "
        0x5187 0x09   # "
        0x5188 0x09   # "
        0x5189 0x75   # "
        0x518A 0x54   # "
        0x518B 0xE0   # "
        0x518C 0xB2   # "
        0x518D 0x42   # "
        0x518E 0x3D   # "
        0x518F 0x56   # "
        0x5190 0x46   # "
        0x5191 0xF8   # "
        0x5192 0x04   # "
        0x5193 0x70   # "
        0x5194 0xF0   # "
        0x5195 0xF0   # "
        0x5196 0x03   # "
        0x5197 0x01   # "
        0x5198 0x04   # "
        0x5199 0x12   # "
        0x519A 0x04   # "
        0x519B 0x00   # "
        0x519C 0x06   # "
        0x519D 0x82   # "
        0x519E 0x38   # "

        0x5300 0x08   # CIP SHARPENMT TH
        0x5301 0x30   # "

        0x5302 0x10   # CIP SHARPENMT OFFSET
        0x5303 0x00   # "

        0x5304 0x08   # CIP DNS TH
        0x5305 0x30   # "

        0x5306 0x08   # CIP DNS OFFSET
        0x5307 0x16   # "

        0x5308 0x08   # CIP CTRL

        0x5309 0x30   # CIP SHARPENTH TH
        0x530A 0x04   # "

        0x530B 0x06   # CIP SHARPENTH OFFSET

        0x5381 0x1E   # CMX.
        0x5382 0x5B   # "
        0x5383 0x08   # "
        0x5384 0x0A   # "
        0x5385 0x7E   # "
        0x5386 0x88   # "
        0x5387 0x7C   # "
        0x5388 0x6C   # "
        0x5389 0x10   # "

        0x538A 0x0198 # CMXSIGN.

        0x5480 0x01   # GAMMA CTRL.

        0x5481 0x08   # GAMMA YST.
        0x5482 0x14   # "
        0x5483 0x28   # "
        0x5484 0x51   # "
        0x5485 0x65   # "
        0x5486 0x71   # "
        0x5487 0x7D   # "
        0x5488 0x87   # "
        0x5489 0x91   # "
        0x548A 0x9A   # "
        0x548B 0xAA   # "
        0x548C 0xB8   # "
        0x548D 0xCD   # "
        0x548E 0xDD   # "
        0x548F 0xEA   # "
        0x5490 0x1D   # "

        0x5580 0x02   # SDE CTRL.
        0x5583 0x40   # "
        0x5584 0x10   # "
        0x5589 0x10   # "
        0x558A 0x00   # "
        0x558B 0xF8   # "

        0x5800 0x23   # GMTRX.
        0x5801 0x14   # "
        0x5802 0x0F   # "
        0x5803 0x0F   # "
        0x5804 0x12   # "
        0x5805 0x26   # "
        0x5806 0x0C   # "
        0x5807 0x08   # "
        0x5808 0x05   # "
        0x5809 0x05   # "
        0x580A 0x08   # "
        0x580B 0x0D   # "
        0x580C 0x08   # "
        0x580D 0x03   # "
        0x580E 0x00   # "
        0x580F 0x00   # "
        0x5810 0x03   # "
        0x5811 0x09   # "
        0x5812 0x07   # "
        0x5813 0x03   # "
        0x5814 0x00   # "
        0x5815 0x01   # "
        0x5816 0x03   # "
        0x5817 0x08   # "
        0x5818 0x0D   # "
        0x5819 0x08   # "
        0x581A 0x05   # "
        0x581B 0x06   # "
        0x581C 0x08   # "
        0x581D 0x0E   # "
        0x581E 0x29   # "
        0x581F 0x17   # "
        0x5820 0x11   # "
        0x5821 0x11   # "
        0x5822 0x15   # "
        0x5823 0x28   # "

        0x5824 0x46   # BRMATRX.
        0x5825 0x26   # "
        0x5826 0x08   # "
        0x5827 0x26   # "
        0x5828 0x64   # "
        0x5829 0x26   # "
        0x582A 0x24   # "
        0x582B 0x22   # "
        0x582C 0x24   # "
        0x582D 0x24   # "
        0x582E 0x06   # "
        0x582F 0x22   # "
        0x5830 0x40   # "
        0x5831 0x42   # "
        0x5832 0x24   # "
        0x5833 0x26   # "
        0x5834 0x24   # "
        0x5835 0x22   # "
        0x5836 0x22   # "
        0x5837 0x26   # "
        0x5838 0x44   # "
        0x5839 0x24   # "
        0x583A 0x26   # "
        0x583B 0x28   # "
        0x583C 0x42   # "

        0x583D 0xCE   # LENC BR OFFSET.

        0x5025 0x00   # Undocumented.

        0x3008 0x02   # SYSTEM CTROL.

        0x3017 0xFFF3 # PAD OUTPUT ENABLE.

        0x3034 0x18   # SC PLL CONTRL.
        0x3035 0x41   # "
        0x3036 0x60   # "
        0x3037 0x13   # "

        0x3108 0x01   # SYSTEM ROOT DIVIDER.

        0x501F 0x00   # FORMAT MUX CONTROL.

        0x3002 0x00   # SYSTEM RESET.
        0x3006 0xEB   # CLOCK ENABLE.

    '''.splitlines()



    # Output sequence of writes to do
    # to initialize the camera module.

    with Meta.enter('static const u8 OVCAM_INITIALIZATION_SEQUENCE[] ='):

        write_line_i = 0

        while write_line_i < len(write_lines):



            # We'll try to coalesce as many register writes
            # into a single write transaction as much as possible.

            run_address    = None
            run_data_bytes = []

            while write_line_i < len(write_lines):



                # Filter out comments.

                write_line, *_ = write_lines[write_line_i].split('#', 1)



                # Skip empty lines.

                if not write_line.strip():
                    write_line_i += 1
                    continue



                # Get the 16-bit address of the write.

                current_address_hex, current_data_hex = write_line.split()

                assert current_address_hex.startswith('0x')
                assert len(current_address_hex)     == 6

                current_address = int(current_address_hex, 16)



                # We then get the bytes of the write data.

                assert current_data_hex.startswith('0x')
                assert len(current_data_hex) % 2 == 0

                current_data_bytes = current_data_hex.removeprefix('0x')
                current_data_bytes = [
                    int(current_data_bytes[i : i + 2], 16)
                    for i in range(0, len(current_data_bytes), 2)
                ]



                # If it can be combined with current run,
                # add to it; otherwise, start a new run.

                if run_address is None:
                    run_address = current_address

                if run_address + len(run_data_bytes) == current_address:
                    run_data_bytes += current_data_bytes
                    write_line_i   += 1
                else:
                    break



            # Record the write address, length of the write, and the write data.

            Meta.line('''
                {}, 0x{:02X}, 0x{:02X},
                    {},
            '''.format(
                len(run_data_bytes),
                (run_address >> 8) & 0xFF,
                (run_address >> 0) & 0xFF,
                ', '.join(f'0x{byte :02X}' for byte in run_data_bytes)
            ))

*/



struct OVCAMSwapchain
{

    volatile u32 reader;
    volatile u32 writer;

    struct OVCAMFramebuffer
    {

        volatile u32 length;

        // The framebuffer has alignment requirements due to DMA.
        // Furthermore, the DCMI peripheral organizes its FIFO to
        // be in units of 32-bit words.

        u8 data[OVCAM_FRAMEBUFFER_SIZE] __attribute__((aligned(4)));

    } framebuffers[2];

};

static struct OVCAMSwapchain _OVCAM_swapchain = {0};

static_assert(IS_POWER_OF_TWO(countof(_OVCAM_swapchain.framebuffers)));
static_assert(sizeof(_OVCAM_swapchain.framebuffers[0].data) % sizeof(u32) == 0);



////////////////////////////////////////////////////////////////////////////////



static struct OVCAMFramebuffer*
OVCAM_get_next_framebuffer(void)
{

    if (!CMSIS_GET(DCMI, CR, ENABLE))
        panic;

    struct OVCAMFramebuffer* framebuffer = {0};

    if (_OVCAM_swapchain.reader == _OVCAM_swapchain.writer)
    {
        framebuffer = nullptr;
    }
    else
    {
        framebuffer = &_OVCAM_swapchain.framebuffers[_OVCAM_swapchain.reader % countof(_OVCAM_swapchain.framebuffers)];
    }

    return framebuffer;

}



static void
OVCAM_free_framebuffer(void)
{

    if (!CMSIS_GET(DCMI, CR, ENABLE))
        panic;

    i32 framebuffers_filled = _OVCAM_swapchain.writer - _OVCAM_swapchain.reader;

    if (!(1 <= framebuffers_filled && framebuffers_filled <= countof(_OVCAM_swapchain.framebuffers)))
        panic;

    _OVCAM_swapchain.reader += 1;

    NVIC_SET_PENDING(GPDMA1_Channel7);

}



static void
_OVCAM_begin_capture(void)
{

    // There shouldn't be an ongoing capture.

    if (CMSIS_GET(GPDMA1_Channel7, CCR, EN))
        panic;

    if (CMSIS_GET(DCMI, CR, CAPTURE))
        panic;

    if (_OVCAM_swapchain.writer - _OVCAM_swapchain.reader >= countof(_OVCAM_swapchain.framebuffers))
        panic;

    if (!CMSIS_GET(DCMI, CR, ENABLE))
        panic;



    // Configure the DMA channel in such a way that it always
    // expect more data than there'll actually be in the transfer.
    // This is so we can handle variable lengthed data transfers.
    // Thus, the data transfer should technically never complete;
    // if it does, then this means the framebuffer got too full.

    #define OVCAM_BYTES_PER_BLOCK_TRANSFER 1024

    static_assert(OVCAM_FRAMEBUFFER_SIZE % OVCAM_BYTES_PER_BLOCK_TRANSFER == 0);

    #define OVCAM_BLOCK_REPETITIONS (OVCAM_FRAMEBUFFER_SIZE / OVCAM_BYTES_PER_BLOCK_TRANSFER)

    static_assert(OVCAM_BLOCK_REPETITIONS < (1 << 11));

    CMSIS_SET(GPDMA1_Channel7, CBR1, BRC , OVCAM_BLOCK_REPETITIONS       );
    CMSIS_SET(GPDMA1_Channel7, CBR1, BNDT, OVCAM_BYTES_PER_BLOCK_TRANSFER);



    // Set the destination of the data.
    // This gets incremented on each transfer, so we
    // also have to reset it each time we do a capture.

    i32 write_index = _OVCAM_swapchain.writer % countof(_OVCAM_swapchain.framebuffers);

    CMSIS_SET(GPDMA1_Channel7, CDAR, DA, (u32) &_OVCAM_swapchain.framebuffers[write_index].data);



    // The DMA channel is now ready to begin transferring
    // data whenever the DCMI requests to do so.

    CMSIS_SET(GPDMA1_Channel7, CCR, EN, true);



    // Tell DCMI to begin capturing the next
    // frame sent by the camera module.

    CMSIS_SET(DCMI, CR, CAPTURE, true);

}



static void
OVCAM_init(void)
{

    // Reset-cycle the peripherals.

    CMSIS_SET(RCC, AHB1RSTR, GPDMA1RST   , true );
    CMSIS_SET(RCC, AHB2RSTR, DCMI_PSSIRST, true );
    CMSIS_SET(RCC, AHB1RSTR, GPDMA1RST   , false);
    CMSIS_SET(RCC, AHB2RSTR, DCMI_PSSIRST, false);



    // Reset-cycle the camera module.

    GPIO_HIGH(ovcam_power_down); // Pulling high so the camera is powered down.
    GPIO_LOW(ovcam_reset);       // Pulling low so the camera is resetting.

    spinlock_nop(100'000); // TODO something less ad-hoc?

    GPIO_LOW(ovcam_power_down);
    GPIO_HIGH(ovcam_reset);

    spinlock_nop(100'000); // TODO something less ad-hoc?



    // Initialize the camera module's registers.

    i32 sequence_index = 0;

    while (sequence_index < countof(OVCAM_INITIALIZATION_SEQUENCE))
    {

        i32       amount_of_bytes_to_write =  OVCAM_INITIALIZATION_SEQUENCE[sequence_index             ];
        const u8* data_to_send             = &OVCAM_INITIALIZATION_SEQUENCE[sequence_index + sizeof(u8)];

        enum I2CMasterError error =
            I2C_transfer
            (
                I2CHandle_ovcam_sccb,
                OVCAM_SEVEN_BIT_ADDRESS,
                I2CAddressType_seven,
                I2COperation_write,
                (u8*) data_to_send,
                sizeof(u16) + amount_of_bytes_to_write
            );

        if (error)
            sorry

        sequence_index += sizeof(u8) + sizeof(u16) + amount_of_bytes_to_write;

    }



    // Initialize the DMA for the DCMI peripheral.

    CMSIS_SET(RCC, AHB1ENR, GPDMA1EN, true); // Clock the GPDMA1 peripheral.

    CMSIS_SET(GPDMA1_Channel7, CSAR, SA, (u32) &DCMI->DR); // The address to get the data from.

    CMSIS_SET
    (
        GPDMA1_Channel7, CTR1 ,
        DINC           , true , // Destination address will be incremented on each transfer.
        SINC           , false, // Source addresss will stay fixed.
        DDW_LOG2       , 0b10 , // Transfer size of four bytes.
        SDW_LOG2       , 0b10 , // "
    );

    CMSIS_SET
    (
        GPDMA1_Channel7, CTR2,
        PFREQ          , true, // The peripheral will dictate when the transfer can happen.
        REQSEL         , 108 , // The DCMI peripheral is the one that'll be providing the transfer request.
        TCEM           , 0b01, // The transfer is considered complete when BNDT and BRC are both zero.
    );

    CMSIS_SET
    (
        GPDMA1_Channel7, CCR , // Enable interrupts for:
        TOIE           , true, //     - Trigger over-run.
        SUSPIE         , true, //     - Completed suspension.
        USEIE          , true, //     - User setting error.
        ULEIE          , true, //     - Update link transfer error.
        DTEIE          , true, //     - Data transfer error.
        TCIE           , true, //     - Transfer complete.
    );

    CMSIS_SET(GPDMA1, MISR, MIS7, true); // Allow interrupts for the DMA channel.

    NVIC_ENABLE(GPDMA1_Channel7);



    // Initialize the DCMI peripheral.

    CMSIS_SET(RCC, AHB2ENR, DCMI_PSSIEN, true); // Clock the DCMI peripheral.

    CMSIS_SET
    (
        DCMI    , IER  , // Enable interrupts for:
        ERR_IE  , true , //     - Embedded synchronization error.
        OVR_IE  , true , //     - Data overrun error.
        FRAME_IE, true , //     - Frame captured.
    );

    CMSIS_SET
    (
        DCMI  , CR   ,
        EDM   , 0b00 , // "00: Interface captures 8-bit data on every pixel clock."
        VSPOL , true , // "1: DCMI_VSYNC active high".
        HSPOL , true , // "1: DCMI_HSYNC active high".
        PCKPOL, true , // "1: Rising edge active".
        JPEG  , true , // Expect JPEG data.
        CM    , true , // "1: Snapshot mode (single frame) - ..."
        ENABLE, true , // We're done configuring and the DCMI is ready to be used.
    );

    NVIC_ENABLE(DCMI_PSSI);



    // The DMA channel will schedule the capture of the first frame.

    NVIC_SET_PENDING(GPDMA1_Channel7);

}



INTERRUPT_GPDMA1_Channel7(void)
{

    enum DMAInterruptEvent : u32
    {
        DMAInterruptEvent_none,
        DMAInterruptEvent_trigger_overrun,
        DMAInterruptEvent_completed_suspension,
        DMAInterruptEvent_user_setting_error,
        DMAInterruptEvent_update_link_transfer_error,
        DMAInterruptEvent_data_transfer_error,
        DMAInterruptEvent_transfer_complete,
    };
    enum DMAInterruptEvent interrupt_event  = {0};
    u32                    interrupt_status = GPDMA1_Channel7->CSR;

    if (CMSIS_GET_FROM(interrupt_status, GPDMA1_Channel7, CSR, SUSPF)) { CMSIS_SET(GPDMA1_Channel7, CFCR, SUSPF, true); interrupt_event = DMAInterruptEvent_completed_suspension;       } else
    if (CMSIS_GET_FROM(interrupt_status, GPDMA1_Channel7, CSR, ULEF )) { CMSIS_SET(GPDMA1_Channel7, CFCR, ULEF , true); interrupt_event = DMAInterruptEvent_update_link_transfer_error; } else
    if (CMSIS_GET_FROM(interrupt_status, GPDMA1_Channel7, CSR, TOF  )) { CMSIS_SET(GPDMA1_Channel7, CFCR, TOF  , true); interrupt_event = DMAInterruptEvent_trigger_overrun;            } else
    if (CMSIS_GET_FROM(interrupt_status, GPDMA1_Channel7, CSR, USEF )) { CMSIS_SET(GPDMA1_Channel7, CFCR, USEF , true); interrupt_event = DMAInterruptEvent_user_setting_error;         } else
    if (CMSIS_GET_FROM(interrupt_status, GPDMA1_Channel7, CSR, DTEF )) { CMSIS_SET(GPDMA1_Channel7, CFCR, DTEF , true); interrupt_event = DMAInterruptEvent_data_transfer_error;        } else
    if (CMSIS_GET_FROM(interrupt_status, GPDMA1_Channel7, CSR, TCF  )) { CMSIS_SET(GPDMA1_Channel7, CFCR, TCF  , true); interrupt_event = DMAInterruptEvent_transfer_complete;          }
    else
    {
        interrupt_event = DMAInterruptEvent_none;
    }

    switch (interrupt_event)
    {

        case DMAInterruptEvent_none:
        {

            if (CMSIS_GET(GPDMA1_Channel7, CCR, EN))
            {
                // DMA is still working...
            }
            else // Initialize the DCMI and DMA to capture an image.
            {
                _OVCAM_begin_capture();
            }

        } break;

        case DMAInterruptEvent_trigger_overrun:
        {
            sorry
        } break;


        case DMAInterruptEvent_user_setting_error:
        {
            sorry
        } break;

        case DMAInterruptEvent_data_transfer_error:
        {
            sorry
        } break;



        // The DCMI suspended the DMA channel
        // because the image frame was complete.

        case DMAInterruptEvent_completed_suspension:
        {

            if (_OVCAM_swapchain.writer - _OVCAM_swapchain.reader >= countof(_OVCAM_swapchain.framebuffers))
                panic;



            // Ensure that there's no outstanding data
            // that hasn't been transferred to the framebuffer.

            if (CMSIS_GET(DCMI, SR, FNE))
                sorry

            if (CMSIS_GET(GPDMA1_Channel7, CSR, FIFOL))
                sorry



            // With JPEG compression, the amount of
            // received data will be variable.

            i32 write_index = _OVCAM_swapchain.writer % countof(_OVCAM_swapchain.framebuffers);
            i32 length      = (i32) CMSIS_GET(GPDMA1_Channel7, CDAR, DA) - (i32) &_OVCAM_swapchain.framebuffers[write_index].data;

            if (length >= sizeof(_OVCAM_swapchain.framebuffers[write_index].data))
                sorry

            _OVCAM_swapchain.framebuffers[write_index].length = length;



            // We can now reset the DMA channel to have
            // it be reconfigured for the next transfer.

            CMSIS_SET(GPDMA1_Channel7, CCR, RESET, true);



            // User can now use the received data.

            _OVCAM_swapchain.writer += 1;



            // There's still space in the swapchain;
            // prepare to capture the next image.

            if (_OVCAM_swapchain.writer - _OVCAM_swapchain.reader < countof(_OVCAM_swapchain.framebuffers))
            {
                _OVCAM_begin_capture();
            }

        } break;



        // We ran out of framebuffer space;
        // we're going to have to discard the frame.
        // This can typically happen when the
        // camera module's register is updated in
        // such a way that the new image data coming
        // in correspond to a larger image.

        case DMAInterruptEvent_transfer_complete:
        {

            // Although the reference manual doesn't seem to be explicit about this,
            // it seems like disabling and enabling the DCMI interface is sufficient
            // in resetting the internal DCMI state machine to discard whatever's
            // remaining of the image frame and prepare for the next.

            CMSIS_SET(DCMI, CR, ENABLE, false);
            CMSIS_SET(DCMI, CR, ENABLE, true );
            _OVCAM_begin_capture();

        } break;



        case DMAInterruptEvent_update_link_transfer_error : panic; // We aren't using this DMA feature.
        default                                           : panic;

    }

}



INTERRUPT_DCMI_PSSI(void)
{

    enum DCMIInterruptEvent : u32
    {
        DCMIInterruptEvent_none,
        DCMIInterruptEvent_line_received,
        DCMIInterruptEvent_vsync,
        DCMIInterruptEvent_embedded_sync_error,
        DCMIInterruptEvent_overrun_error,
        DCMIInterruptEvent_capture_complete,
    };
    enum DCMIInterruptEvent interrupt_event  = {0};
    u32                     interrupt_status = DCMI->MISR;

    if (CMSIS_GET_FROM(interrupt_status, DCMI, MIS, VSYNC_MIS)) { CMSIS_SET(DCMI, ICR, VSYNC_ISC, true); interrupt_event = DCMIInterruptEvent_vsync;               } else
    if (CMSIS_GET_FROM(interrupt_status, DCMI, MIS, ERR_MIS  )) { CMSIS_SET(DCMI, ICR, ERR_ISC  , true); interrupt_event = DCMIInterruptEvent_embedded_sync_error; } else
    if (CMSIS_GET_FROM(interrupt_status, DCMI, MIS, OVR_MIS  )) { CMSIS_SET(DCMI, ICR, OVR_ISC  , true); interrupt_event = DCMIInterruptEvent_overrun_error;       } else
    if (CMSIS_GET_FROM(interrupt_status, DCMI, MIS, FRAME_MIS)) { CMSIS_SET(DCMI, ICR, FRAME_ISC, true); interrupt_event = DCMIInterruptEvent_capture_complete;    } else
    if (CMSIS_GET_FROM(interrupt_status, DCMI, MIS, LINE_MIS )) { CMSIS_SET(DCMI, ICR, LINE_ISC , true); interrupt_event = DCMIInterruptEvent_line_received;       }
    else
    {
        interrupt_event = DCMIInterruptEvent_none;
    }

    switch (interrupt_event)
    {

        case DCMIInterruptEvent_none:
        {
            // Nothing interesting here...
        } break;

        case DCMIInterruptEvent_overrun_error:
        {
            sorry
        } break;

        case DCMIInterruptEvent_capture_complete:
        {

            if (_OVCAM_swapchain.writer - _OVCAM_swapchain.reader >= countof(_OVCAM_swapchain.framebuffers))
                panic;



            // The DMA should should still be active; the way we
            // will disable it is through suspension. However, we
            // should wait until the DMA has definitely grabbed all
            // of the data in DCMI's FIFO and has transferred it all.
            // It should be noted that I haven't been able to stimulate
            // this edge-case, so I can't say for certainty that this
            // works as expected.

            if (!CMSIS_GET(GPDMA1_Channel7, CCR, EN))
                panic;

            while
            (
                CMSIS_GET(DCMI, SR, FNE) &&            // Data still in DCMI's FIFO?
                CMSIS_GET(GPDMA1_Channel7, CSR, FIFOL) // Data still in DMA's FIFO?
            );



            // The DMA is configured in such a way that it'll be able to
            // handle arbitrary lengthed data, but to disable it so we
            // can configure for the next transfer, we have to first
            // suspend the channel.

            CMSIS_SET(GPDMA1_Channel7, CCR, SUSP, true);

        } break;

        case DCMIInterruptEvent_line_received       : panic; // Shouldn't happen because the interrupt for it isn't enabled.
        case DCMIInterruptEvent_vsync               : panic; // "
        case DCMIInterruptEvent_embedded_sync_error : panic; // We aren't using embedded codes.
        default                                     : panic;

    }

}
