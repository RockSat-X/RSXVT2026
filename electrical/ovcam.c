#define OVCAM_SEVEN_BIT_ADDRESS 0x3C

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

    Meta.define('OVCAM_RESOLUTION_X', OVCAM_RESOLUTION[0])
    Meta.define('OVCAM_RESOLUTION_Y', OVCAM_RESOLUTION[1])

    assert OVCAM_RESOLUTION in (
        (160, 120),
        (320, 240),
        (480, 272),
        (640, 480),
        (800, 480),
    ), f'Unsupported resolution: {repr(OVCAM_RESOLUTION)}.'



    # Color test pattern configuration for debugging purposes.

    pre_isp_test_setting = (
        (False << 7) | # Pre-ISP test enable.
        (True  << 6) | # Rolling.
        (False << 5) | # Transparent.
        (0b00  << 2) | # Pre-ISP bar style.
                       #     0b00 : "Standard 8 color bar".
                       #     0b01 : "Gradual change at vertical mode 1".
                       #     0b10 : "Gradual change at horizontal".
                       #     0b11 : "Gradual change at vertical mode 2".
        (0b00  << 0)   # Test select.
                       #     0b00 : "Color bar".
                       #     0b01 : "Random data".
                       #     0b10 : "Square data".
                       #     0b11 : "Black image".
    )



    # List of register writes to perform.

    write_lines = f'''

        0x3008 0x82   # SYSTEM CTROL.

        0x3000 0x00   # SYSTEM RESET.
        0x3002 0x1C   # "

        0x3004 0xFF   # CLOCK ENABLE.
        0x3006 0xC3   # "

        0x300E 0x58   # MIPI CONTROL.

        0x3017 0xFF   # PAD OUTPUT ENABLE.
        0x3018 0xF3   # "

        0x3034 0x18   # SC PLL CONTRL.
        0x3035 0x41   # "
        0x3036 0x38   # "
        0x3037 0x16   # "

        0x3103 0x03   # SCCB SYSTEM CTRL.

        0x3108 0x01   # SYSTEM ROOT DIVIDER.

        0x3632 0xE2   # Undocumented.
        0x3634 0x40   # "
        0x3709 0x52   # "
        0x370C 0x03   # "

        0x3800 0x0000 # TIMING HS.
        0x3802 0x0004 # TIMING VS.
        0x3804 0x0A3F # TIMING HW.
        0x3806 0x079B # TIMING VH.

        0x3808 0x{OVCAM_RESOLUTION[0] :04X} # TIMING DVPHO.
        0x380A 0x{OVCAM_RESOLUTION[1] :04X} # TIMING DVPVO.

        0x380C 0x0790 # TIMING HTS.
        0x380E 0x0440 # TIMING VTS.
        0x3810 0x0010 # TIMING HOFFSET.
        0x3812 0x0006 # TIMING VOFFSET.
        0x3814 0x31   # TIMING X INC.
        0x3815 0x31   # TIMING Y INC.
        0x3820 0x0600 # TIMING TC REG.

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

        0x3C01 0x34   # 5060HZ CTRL.
        0x3C04 0x28   # "
        0x3C05 0x98   # "

        0x3C06 0x0000 # LIGHTMETER.
        0x3C08 0x012C # "

        0x3C0A 0x9C40 # SAMPLE NUMBER.

        0x4001 0x02   # BLC CTRL.
        0x4004 0x02   # "

        0x4300 0xFA   # FORMAT CONTROL. TODO Look into JPEG?

        0x4407 0x04   # JPEG CTRL.

        0x4713 0x03   # JPG MODE SELECT.

        0x4740 0x22   # POLARITY CTRL.

        0x4837 0x22   # PCLK PERIOD.

        0x5000 0xA7   # ISP CONTROL.
        0x5001 0xA3   # "

        0x503D 0x{pre_isp_test_setting :02X} # PRE ISP TEST SETTING.

        0x501F 0x01   # FORMAT MUX CONTROL.

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



enum OVCAMFramebufferState : u32
{
    OVCAMFramebufferState_empty,
    OVCAMFramebufferState_filling,
    OVCAMFramebufferState_filled,
};

static volatile enum OVCAMFramebufferState OVCAM_framebuffer_state = {0};

// The framebuffer has alignment requirements due to DMA.
static volatile u8 OVCAM_framebuffer[OVCAM_RESOLUTION_X * OVCAM_RESOLUTION_Y * 3] __attribute__((aligned(4))) = {0};



static void
OVCAM_begin_capture(void)
{

    // There shouldn't be an ongoing capture.

    if (CMSIS_GET(GPDMA1_Channel7, CCR, EN))
        panic;

    if (CMSIS_GET(DCMI, CR, CAPTURE))
        panic;

    if
    (
        OVCAM_framebuffer_state != OVCAMFramebufferState_empty &&
        OVCAM_framebuffer_state != OVCAMFramebufferState_filled
    )
        panic;



    // The framebuffer will be filled
    // with data by DMA over time.

    OVCAM_framebuffer_state = OVCAMFramebufferState_filling;



    // Set the amount of bytes to be transferred.
    // DMA channels operate data transfers in terms of blocks,
    // which can be up to 64KiB; to transfer more data
    // than that, we apply a reptition of a certain amount
    // until we get the desired transfer size.

    #define GPDMA1_CHANNEL7_BLOCK_SIZE (160 * 120 * 3)

    static_assert(GPDMA1_CHANNEL7_BLOCK_SIZE % sizeof(u32) == 0);
    static_assert(GPDMA1_CHANNEL7_BLOCK_SIZE < (1 << 16));
    static_assert(sizeof(OVCAM_framebuffer) % GPDMA1_CHANNEL7_BLOCK_SIZE == 0);

    CMSIS_SET(GPDMA1_Channel7, CBR1, BRC , sizeof(OVCAM_framebuffer) / GPDMA1_CHANNEL7_BLOCK_SIZE - 1);
    CMSIS_SET(GPDMA1_Channel7, CBR1, BNDT, GPDMA1_CHANNEL7_BLOCK_SIZE);



    // Set the destination of the data.
    // This gets incremented on each transfer, so we
    // also have to reset it each time we do a capture.

    CMSIS_SET(GPDMA1_Channel7, CDAR, DA, (u32) &OVCAM_framebuffer);



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
    GPIO_LOW(ovcam_restart);     // Pulling low so the camera is resetting.

    spinlock_nop(100'000);

    GPIO_LOW(ovcam_power_down);
    GPIO_HIGH(ovcam_restart);

    spinlock_nop(100'000);



    // Initialize the camera module's registers.

    i32 sequence_index = 0;

    while (sequence_index < countof(OVCAM_INITIALIZATION_SEQUENCE))
    {

        i32       amount_of_bytes_to_write =  OVCAM_INITIALIZATION_SEQUENCE[sequence_index             ];
        const u8* data_to_send             = &OVCAM_INITIALIZATION_SEQUENCE[sequence_index + sizeof(u8)];

        enum I2CMasterError error =
            I2C_blocking_transfer
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
        GPDMA1_Channel7, CTR1,
        DINC           , true , // Destination address will be incremented on each transfer.
        SINC           , false, // Source addresss will stay fixed.
        DDW_LOG2       , 0b10 , // Transfer size of four bytes.
        SDW_LOG2       , 0b10 , // "
    );

    CMSIS_SET
    (
        GPDMA1_Channel7, CTR2 ,
        PFREQ          , true , // The peripheral will dictate when the transfer can happen.
        REQSEL         , 108  , // The DCMI peripheral is the one that'll be providing the transfer request.
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
        DCMI     , IER  , // Enable interrupts for:
        ERR_IE   , true , //     - Embedded synchronization error.
        OVR_IE   , true , //     - Data overrun error.
        FRAME_IE , true , //     - Frame captured.
    );

    CMSIS_SET
    (
        DCMI      , CR   ,
        EDM       , 0b00 , // "00: Interface captures 8-bit data on every pixel clock."
        VSPOL     , true , // "1: DCMI_VSYNC active high".
        HSPOL     , true , // "1: DCMI_HSYNC active high".
        PCKPOL    , true , // "1: Rising edge active".
        JPEG      , false, // TODO Use JPEG.
        CM        , true , // "1: Snapshot mode (single frame) - ..."
        ENABLE    , true , // We're done configuring and the DCMI is ready to be used.
    );

    NVIC_ENABLE(DCMI_PSSI);

}



INTERRUPT_GPDMA1_Channel7
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
            // Nothing interesting here...
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

        case DMAInterruptEvent_transfer_complete:
        {

            // We got the last bit of data into the framebuffer!

            if (OVCAM_framebuffer_state != OVCAMFramebufferState_filling)
                panic;

            OVCAM_framebuffer_state = OVCAMFramebufferState_filled;

        } break;

        case DMAInterruptEvent_completed_suspension       : panic; // We aren't using this DMA feature.
        case DMAInterruptEvent_update_link_transfer_error : panic; // "
        default                                           : panic;

    }

}



INTERRUPT_DCMI_PSSI
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
            switch (OVCAM_framebuffer_state)
            {
                case OVCAMFramebufferState_empty   : panic; // Invalid.
                case OVCAMFramebufferState_filling : break; // DMA might still be transferring from DCMI's FIFO into the framebuffer.
                case OVCAMFramebufferState_filled  : break; // DMA finished filling the framebuffer.
                default                            : panic;
            }
        } break;

        case DCMIInterruptEvent_line_received       : panic; // Shouldn't happen because the interrupt for it isn't enabled.
        case DCMIInterruptEvent_vsync               : panic; // "
        case DCMIInterruptEvent_embedded_sync_error : panic; // We aren't using embedded codes.
        default                                     : panic;

    }

}
