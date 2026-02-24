#ifndef OVCAM_TIMEOUT_US
#define OVCAM_TIMEOUT_US 1'000'000
#endif

#define OVCAM_SEVEN_BIT_ADDRESS 0x3C
#define OVCAM_FRAMEBUFFER_SIZE  (100 * 1024) // @/`OVCAM DMA Block Repetitions`.



#include "OVCAM_defs.meta"
/* #meta

    # The initialization sequence of the OV5640 is... poorly understood.
    # The datasheet provided by OmniVision (version 2.03, May 2011, OV05640-A71A)
    # is not comprehensive nor descriptive. The register writes described
    # below is more-or-less taken from STM32's BSP implementation of the
    # OV5640 driver, but honestly, it's a waste of time to figure out what
    # even is going on here...



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



struct OVCAMFramebuffer
{
    i32 length;
    u8 data[OVCAM_FRAMEBUFFER_SIZE] __attribute__((aligned(4)));      // Alignment for DMA.
    static_assert(sizeof(OVCAM_FRAMEBUFFER_SIZE) % sizeof(u32) == 0); // DCMI packs data into 32-bit words.
};

enum OVCAMDriverState : u32
{
    OVCAMDriverState_uninitialized,
    OVCAMDriverState_standby,
    OVCAMDriverState_capturing,
    OVCAMDriverState_dcmi_finished_capturing,
};

struct OVCAMDriver
{
    _Atomic enum OVCAMDriverState atomic_state;
    u32                           swap_timestamp_us;
};

static struct OVCAMDriver       _OVCAM_driver             = {0};
static struct OVCAMFramebuffer* OVCAM_current_framebuffer = {0};



// Framebuffer ring-buffer must not be a part of the OVCAM driver
// because it'll be expensive to zero it out; also, debug inspection
// will become much more noisy.

static RingBuffer(struct OVCAMFramebuffer, 2) _OVCAM_framebuffers = {0};



////////////////////////////////////////////////////////////////////////////////



static useret enum OVCAMReinitResult : u32
{
    OVCAMReinitResult_success,
    OVCAMReinitResult_failed_to_initialize_with_i2c,
    OVCAMReinitResult_bug = BUG_CODE,
}
OVCAM_reinit(void)
{



    // Reset-cycle the peripherals.

    CMSIS_SET(RCC, AHB1RSTR, GPDMA1RST   , true );
    CMSIS_SET(RCC, AHB2RSTR, DCMI_PSSIRST, true );
    CMSIS_SET(RCC, AHB1RSTR, GPDMA1RST   , false);
    CMSIS_SET(RCC, AHB2RSTR, DCMI_PSSIRST, false);

    NVIC_DISABLE(GPDMA1_Channel7);
    NVIC_DISABLE(DCMI_PSSI);



    // When clearing out the framebuffer ring-buffer,
    // we make sure to only clear out the non-data part,
    // because the framebuffers are pretty big, and it'd
    // be a waste of time to do so.

    _OVCAM_framebuffers.ring_buffer_raw = (struct RingBufferRaw) {0};
    _OVCAM_driver                       = (struct OVCAMDriver) {0};
    OVCAM_current_framebuffer           = nullptr;



    // Reinitialize the I2C driver that'll
    // later initialize the OV camera module.

    enum I2CReinitResult i2c_reinit_result = I2C_reinit(I2CHandle_ovcam_sccb);

    switch (i2c_reinit_result)
    {
        case I2CReinitResult_success : break;
        case I2CReinitResult_bug     : bug;
        default                      : bug;
    }



    // Power-cycle and reset-cycle the camera module.

    GPIO_ACTIVE(ovcam_power_down);
    GPIO_ACTIVE(ovcam_reset);

    spinlock_us(1'000);

    GPIO_INACTIVE(ovcam_power_down);
    GPIO_INACTIVE(ovcam_reset);

    spinlock_us(1'000);



    // Initialize the camera module's registers.

    for
    (
        i32 sequence_offset = 0;
        sequence_offset < countof(OVCAM_INITIALIZATION_SEQUENCE);
    )
    {

        i32       amount_of_bytes_to_write =  OVCAM_INITIALIZATION_SEQUENCE[sequence_offset             ];
        const u8* data_to_send             = &OVCAM_INITIALIZATION_SEQUENCE[sequence_offset + sizeof(u8)];

        b32 success = false;

        for
        (
            i32 attempts = 0;
            attempts < 16 && !success;
            attempts += 1
        )
        {

            enum I2CTransferResult i2c_transfer_result =
                I2C_transfer
                (
                    I2CHandle_ovcam_sccb,
                    OVCAM_SEVEN_BIT_ADDRESS,
                    I2CAddressType_seven,
                    I2COperation_single_write,
                    (u8*) data_to_send,
                    sizeof(u16) + amount_of_bytes_to_write
                );

            switch (i2c_transfer_result)
            {

                case I2CTransferResult_transfer_done:
                {
                    success = true;
                } break;

                case I2CTransferResult_no_acknowledge:
                case I2CTransferResult_clock_stretch_timeout:
                {
                    // Something weird happened;
                    // let's try the transfer again.
                } break;

                case I2CTransferResult_transfer_ongoing : bug; // OVCAM driver depends on a blocking I2C driver.
                case I2CTransferResult_bug              : bug;
                default                                 : bug;

            }

        }

        if (success)
        {
            sequence_offset += sizeof(u8) + sizeof(u16) + amount_of_bytes_to_write;
        }
        else
        {
            return OVCAMReinitResult_failed_to_initialize_with_i2c;
        }

    }



    // Clock the DCMI peripheral.

    CMSIS_SET(RCC, AHB2ENR, DCMI_PSSIEN, true);



    // Configure the DCMI peripheral.

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



    // Clock the GPDMA1 peripheral.

    CMSIS_SET(RCC, AHB1ENR, GPDMA1EN, true);



    // Configure the DMA channel.

    CMSIS_SET(GPDMA1_Channel7, CSAR, SA, (u32) &DCMI->DR); // Where to get the pixel data.

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
    NVIC_ENABLE(GPDMA1_Channel7);        // "



    // We now start recording how long it takes to capture the first image.
    // If it things take too long, this indicates an issue with the camera module.

    _OVCAM_driver.swap_timestamp_us = TIMEKEEPING_COUNTER();



    // The OVCAM driver is now done being set up.

    atomic_store_explicit
    (
        &_OVCAM_driver.atomic_state,
        OVCAMDriverState_standby,
        memory_order_relaxed
    );



    // The interrupt routine will set up the capture of the first image.

    NVIC_SET_PENDING(DCMI_PSSI);

    return OVCAMReinitResult_success;

}



static useret enum OVCAMSwapFramebufferResult : u32
{
    OVCAMSwapFramebufferResult_attempted,
    OVCAMSwapFramebufferResult_timeout,
    OVCAMSwapFramebufferResult_bug = BUG_CODE,
}
OVCAM_swap_framebuffer(void)
{

    enum OVCAMDriverState observed_state =
        atomic_load_explicit
        (
            &_OVCAM_driver.atomic_state,
            memory_order_relaxed
        );

    switch (observed_state)
    {

        case OVCAMDriverState_standby:
        case OVCAMDriverState_capturing:
        case OVCAMDriverState_dcmi_finished_capturing:
        {

            // If there was already a framebuffer that the user was using, we now free it
            // so the OVCAM driver can fill in a new one and the user can get the next framebuffer.

            if (OVCAM_current_framebuffer)
            {

                if (!RingBuffer_pop(&_OVCAM_framebuffers, nullptr))
                    bug; // The ring-buffer should've had the framebuffer the user was using...



                // Indicate to the OVCAM driver that
                // there's space for the next image capture.

                NVIC_SET_PENDING(DCMI_PSSI);



                // We now start keeping track of how it's taking
                // for the OVCAM driver to get the image data...

                _OVCAM_driver.swap_timestamp_us = TIMEKEEPING_COUNTER();

            }



            // Update the current framebuffer that the
            // user can use, which may or may not be available.

            OVCAM_current_framebuffer = RingBuffer_reading_pointer(&_OVCAM_framebuffers);



            // Make sure that the OVCAM driver is aware that
            // there's an available framebuffer it should be filling.

            if (!OVCAM_current_framebuffer)
            {

                u32 capture_timestamp_us = _OVCAM_driver.swap_timestamp_us;
                u32 current_timestamp_us = TIMEKEEPING_COUNTER();
                u32 elapsed_us           = current_timestamp_us - capture_timestamp_us;

                if (elapsed_us < (TIMEKEEPING_COUNTER_TYPE) { OVCAM_TIMEOUT_US })
                {
                    NVIC_SET_PENDING(DCMI_PSSI);
                }
                else
                {
                    return OVCAMSwapFramebufferResult_timeout;
                }

            }



            return OVCAMSwapFramebufferResult_attempted;

        } break;

        case OVCAMDriverState_uninitialized : bug; // User needs to initialize the driver first.
        default                             : bug;

    }

}



////////////////////////////////////////////////////////////////////////////////



static useret enum OVCAMUpdateOnceResult : u32
{
    OVCAMUpdateOnceResult_again,
    OVCAMUpdateOnceResult_yield,
    OVCAMUpdateOnceResult_bug = BUG_CODE,
}
_OVCAM_update_once(void)
{

    enum OVCAMInterruptEvent : u32
    {
        OVCAMInterruptEvent_none,
        OVCAMInterruptEvent_dma_completed_suspension,
        OVCAMInterruptEvent_dma_transfer_complete,
        OVCAMInterruptEvent_dcmi_capture_complete
    };

    enum OVCAMInterruptEvent interrupt_event       = {0};
    u32                      dma_interrupt_status  = GPDMA1_Channel7->CSR;
    u32                      dcmi_interrupt_status = DCMI->MISR;
    u32                      dcmi_interrupt_enable = DCMI->IER;



    // The DMA failed to update the link-listed.
    //
    // @/pg 677/sec 16.4.17/`H533rm`.

    if (CMSIS_GET_FROM(dma_interrupt_status, GPDMA1_Channel7, CSR, ULEF))
    {
        bug; // Shouldn't happen; this feature isn't used.
    }



    // The DMA channel got triggered too quickly by an external trigger.
    //
    // @/pg 672/sec 16.4.12/`H533rm`.

    else if (CMSIS_GET_FROM(dma_interrupt_status, GPDMA1_Channel7, CSR, TOF))
    {
        bug; // Shouldn't happen; DMA channel isn't using an external trigger.
    }



    // The DMA configuration is invalid.
    //
    // @/pg 677/sec 16.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(dma_interrupt_status, GPDMA1_Channel7, CSR, USEF))
    {
        bug; // Something obvious like this shouldn't happen.
    }



    // The DMA encountered an error while trying to transfer the data.
    //
    // @/pg 676/sec 16.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(dma_interrupt_status, GPDMA1_Channel7, CSR, DTEF))
    {
        bug; // Something obvious like this shouldn't happen.
    }



    // VSYNC activation detected.

    else if (CMSIS_GET_FROM(dcmi_interrupt_status, DCMI, MIS, VSYNC_MIS))
    {
        bug; // Shouldn't happen; this interrupt event isn't used.
    }



    // DCMI finished receiving an entire image line.

    else if (CMSIS_GET_FROM(dcmi_interrupt_status, DCMI, MIS, LINE_MIS))
    {
        bug; // Shouldn't happen; this interrupt event isn't used.
    }



    // Error within embedded syncronization codes found.

    else if (CMSIS_GET_FROM(dcmi_interrupt_status, DCMI, MIS, ERR_MIS))
    {
        bug; // Shouldn't happen; this feature isn't used.
    }



    // DCMI's FIFO got too full.

    else if (CMSIS_GET_FROM(dcmi_interrupt_status, DCMI, MIS, OVR_MIS))
    {
        bug; // Shouldn't happen; the DMA should've handled it.
    }



    // The DMA is done transferring all of the data it was programmed to do.
    //
    // @/`OVCAM DMA Block Repetitions`.
    // @/pg 642/sec 16.4.3/`H533rm`.

    else if (CMSIS_GET_FROM(dma_interrupt_status, GPDMA1_Channel7, CSR, TCF))
    {
        CMSIS_SET(GPDMA1_Channel7, CFCR, TCF, true);
        interrupt_event = OVCAMInterruptEvent_dma_transfer_complete;
    }



    // DCMI finished capturing an entire image.

    else if (CMSIS_GET_FROM(dcmi_interrupt_status, DCMI, MIS, FRAME_MIS))
    {
        CMSIS_SET(DCMI, ICR, FRAME_ISC, true);
        interrupt_event = OVCAMInterruptEvent_dcmi_capture_complete;
    }



    // The DMA channel is done being suspended.
    //
    // @/pg 642/sec 16.4.3/`H533rm`.

    else if (CMSIS_GET_FROM(dma_interrupt_status, GPDMA1_Channel7, CSR, SUSPF))
    {
        CMSIS_SET(GPDMA1_Channel7, CFCR, SUSPF, true);
        interrupt_event = OVCAMInterruptEvent_dma_completed_suspension;
    }



    // Nothing notable happened.

    else
    {

        if (dcmi_interrupt_status & dcmi_interrupt_enable)
            bug; // We have an unhandled interrupt event...?

        interrupt_event = OVCAMInterruptEvent_none;

    }



    // Handle the interrupt event.

    enum OVCAMDriverState observed_state =
        atomic_load_explicit
        (
            &_OVCAM_driver.atomic_state,
            memory_order_relaxed
        );

    switch (observed_state)
    {



        // Driver and camera module not even initialized yet...

        case OVCAMDriverState_uninitialized: switch (interrupt_event)
        {

            case OVCAMInterruptEvent_none:
            {

                if (CMSIS_GET(GPDMA1_Channel7, CCR, EN))
                    bug; // DMA active for some reason?

                if (CMSIS_GET(DCMI, CR, CAPTURE))
                    bug; // DCMI capturing for some reason?



                // Somehow the OVCAM interrupt routines got pended but the driver is not initialized yet;
                // either way, not much we can do until the user initializes the driver.

                return OVCAMUpdateOnceResult_yield;

            } break;

            case OVCAMInterruptEvent_dma_completed_suspension : bug;
            case OVCAMInterruptEvent_dma_transfer_complete    : bug;
            case OVCAMInterruptEvent_dcmi_capture_complete    : bug;
            default                                           : bug;

        } break;



        // Driver peripherals are ready to capture the next image.

        case OVCAMDriverState_standby: switch (interrupt_event)
        {

            case OVCAMInterruptEvent_none:
            {

                if (CMSIS_GET(GPDMA1_Channel7, CCR, EN))
                    bug; // DMA already active for some reason?

                if (CMSIS_GET(DCMI, CR, CAPTURE))
                    bug; // DCMI already capturing for some reason?

                if (!CMSIS_GET(DCMI, CR, ENABLE))
                    bug; // DCMI should've been initialized...



                // Try to configure the DMA and DCMI for the next image capture.

                struct OVCAMFramebuffer* framebuffer = RingBuffer_writing_pointer(&_OVCAM_framebuffers);

                if (framebuffer)
                {

                    // Set the DMA destination of the next available framebuffer to fill.

                    CMSIS_SET(GPDMA1_Channel7, CDAR, DA, (u32) &framebuffer->data);



                    // Set the range of data that the DMA can transfer up to.
                    //
                    // @/`OVCAM DMA Block Repetitions`.

                    #define OVCAM_BYTES_PER_BLOCK_TRANSFER 1024

                    static_assert(OVCAM_FRAMEBUFFER_SIZE % OVCAM_BYTES_PER_BLOCK_TRANSFER == 0);

                    #define OVCAM_BLOCK_REPETITIONS ((OVCAM_FRAMEBUFFER_SIZE / OVCAM_BYTES_PER_BLOCK_TRANSFER) - 1)

                    static_assert(0 <= OVCAM_BLOCK_REPETITIONS && OVCAM_BLOCK_REPETITIONS < (1 << 11));

                    CMSIS_SET(GPDMA1_Channel7, CBR1, BRC , OVCAM_BLOCK_REPETITIONS       );
                    CMSIS_SET(GPDMA1_Channel7, CBR1, BNDT, OVCAM_BYTES_PER_BLOCK_TRANSFER);



                    // The DMA channel is now ready to fill in the framebuffer data;
                    // the DCMI will now also be ready to capture the next image.

                    CMSIS_SET(GPDMA1_Channel7, CCR, EN     , true);
                    CMSIS_SET(DCMI           , CR , CAPTURE, true);



                    // We finished setting things up to capture the next image.

                    atomic_store_explicit
                    (
                        &_OVCAM_driver.atomic_state,
                        OVCAMDriverState_capturing,
                        memory_order_relaxed
                    );

                    return OVCAMUpdateOnceResult_again;

                }
                else // No space for the next image; gotta wait for the user to free up a framebuffer.
                {
                    return OVCAMUpdateOnceResult_yield;
                }

            } break;

            case OVCAMInterruptEvent_dma_completed_suspension : bug;
            case OVCAMInterruptEvent_dma_transfer_complete    : bug;
            case OVCAMInterruptEvent_dcmi_capture_complete    : bug;
            default                                           : bug;

        } break;



        // The peripherals are in the process of capturing the next image.

        case OVCAMDriverState_capturing: switch (interrupt_event)
        {



            // Nothing new yet...

            case OVCAMInterruptEvent_none:
            {
                return OVCAMUpdateOnceResult_yield;
            } break;



            // All pixel data has been transferred through the DCMI frontend;
            // we now suspend the DMA transfer to reset the DMA.
            //
            // @/`OVCAM DMA Block Repetitions`.

            case OVCAMInterruptEvent_dcmi_capture_complete:
            {

                // Before we suspend the DMA to for the next image capture,
                // we should wait until the DMA has definitely grabbed all of
                // the data in DCMI's FIFO and has transferred it all. It should
                // be noted that I haven't been able to stimulate this edge-case,
                // so I can't say for certainty that this works as expected.

                i32 attempts = 0;

                while (true)
                {
                    if
                    (
                        CMSIS_GET(DCMI, SR, FNE) ||            // Data still in DCMI's FIFO?
                        CMSIS_GET(GPDMA1_Channel7, CSR, FIFOL) // Data still in DMA's FIFO?
                    )
                    {
                        if (!CMSIS_GET(GPDMA1_Channel7, CCR, EN))
                        {

                            if (CMSIS_GET(DCMI, SR, FNE))
                                bug; // DMA got disabled, but there's still DCMI data!

                            if (CMSIS_GET(GPDMA1_Channel7, CSR, FIFOL))
                                bug; // DMA got disabled, but there's still DMA data!

                        }

                        if (attempts < 4096)
                        {
                            attempts += 1;
                        }
                        else
                        {
                            bug; // We spent too much time waiting...
                        }

                    }
                    else
                    {
                        break;
                    }
                }



                // We can now finally suspend the DMA
                // channel, which will take some cycles.

                CMSIS_SET(GPDMA1_Channel7, CCR, SUSP, true);

                atomic_store_explicit
                (
                    &_OVCAM_driver.atomic_state,
                    OVCAMDriverState_dcmi_finished_capturing,
                    memory_order_relaxed
                );

                return OVCAMUpdateOnceResult_again;

            } break;



            // The DMA completely filled out the framebuffer;
            // we're going to have the discard this image frame...

            case OVCAMInterruptEvent_dma_transfer_complete:
            {

                // @/`OVCAM Discarding Frame`:
                //
                // Although the reference manual doesn't seem to be explicit about this,
                // it seems like disabling and enabling the DCMI interface is sufficient
                // in resetting the internal DCMI state machine to discard whatever's
                // remaining of the image frame.

                CMSIS_SET(DCMI, CR, ENABLE, false);
                CMSIS_SET(DCMI, CR, ENABLE, true );

                atomic_store_explicit
                (
                    &_OVCAM_driver.atomic_state,
                    OVCAMDriverState_standby,
                    memory_order_relaxed
                );

                return OVCAMUpdateOnceResult_again;

            } break;



            case OVCAMInterruptEvent_dma_completed_suspension : bug;
            default                                           : bug;

        } break;



        // We're now waiting for the DMA channel to become suspended.

        case OVCAMDriverState_dcmi_finished_capturing: switch (interrupt_event)
        {



            // Nothing new yet...

            case OVCAMInterruptEvent_none:
            {
                return OVCAMUpdateOnceResult_yield;
            } break;



            // The DMA channel can now be reset and our image be sent to the user.

            case OVCAMInterruptEvent_dma_completed_suspension:
            {

                if (CMSIS_GET(DCMI, SR, FNE))
                    bug; // The DCMI somehow still has left-over data in its FIFO.

                if (CMSIS_GET(GPDMA1_Channel7, CSR, FIFOL))
                    bug; // The DMA somehow still has left-over data in its FIFO.



                // With JPEG compression, the amount of received data will be variable,
                // so we'll have to calculate the actual amount of data that has been transferred.

                struct OVCAMFramebuffer* framebuffer = RingBuffer_writing_pointer(&_OVCAM_framebuffers);

                if (!framebuffer)
                    bug; // There should've been a framebuffer that we were filling out...

                framebuffer->length = (i32) CMSIS_GET(GPDMA1_Channel7, CDAR, DA) - (i32) &framebuffer->data;

                if (!(0 < framebuffer->length && framebuffer->length < OVCAM_FRAMEBUFFER_SIZE))
                    bug; // Non-sensical data transfer length!



                // We can now reset the DMA channel to have
                // it be reconfigured for the next transfer.

                CMSIS_SET(GPDMA1_Channel7, CCR, RESET, true);



                // User can now use the received data.

                if (!RingBuffer_push(&_OVCAM_framebuffers, nullptr))
                    bug; // There should've been the framebuffer that we were filling out...



                atomic_store_explicit
                (
                    &_OVCAM_driver.atomic_state,
                    OVCAMDriverState_standby,
                    memory_order_relaxed
                );

                return OVCAMUpdateOnceResult_again;

            } break;



            // The DCMI peripheral finished capturing all of the image pixel data,
            // but the DMA fully used all of the framebuffer space. This is probably
            // because the image was exactly the same size as the framebuffer.
            // Although the image could still be technically used, this should be
            // such a rare situation that we can safely discard it.

            case OVCAMInterruptEvent_dma_transfer_complete:
            {

                CMSIS_SET(DCMI, CR, ENABLE, false); // @/`OVCAM Discarding Frame`.
                CMSIS_SET(DCMI, CR, ENABLE, true ); // "

                atomic_store_explicit
                (
                    &_OVCAM_driver.atomic_state,
                    OVCAMDriverState_standby,
                    memory_order_relaxed
                );

                return OVCAMUpdateOnceResult_again;

            } break;



            case OVCAMInterruptEvent_dcmi_capture_complete : bug;
            default                                        : bug;

        } break;



        default: bug;

    }

}



INTERRUPT_GPDMA1_Channel7(void)
{

    // Rather than having more than one interrupt handler
    // potentially overlapping each other, the DMA routine
    // will invoke the DCMI handler, and from there is where
    // both DMA and DCMI interrupt events are handled and processed.
    //
    // This also means that the DMA channel's interrupt priority
    // needs to be less than of the DCMI's interrupt priority.

    NVIC_SET_PENDING(DCMI_PSSI);

}



INTERRUPT_DCMI_PSSI(void)
{

    for (b32 yield = false; !yield;)
    {

        enum OVCAMUpdateOnceResult result = _OVCAM_update_once();

        switch (result)
        {

            case OVCAMUpdateOnceResult_again:
            {
                // The state-machine should be updated again.
            } break;

            case OVCAMUpdateOnceResult_yield:
            {
                yield = true; // We can stop updating the state-machine for now.
            } break;

            case OVCAMUpdateOnceResult_bug:
            default:
            {

                // Something bad happened!
                // Shut down the peripherals and wait
                // for the user to reinitialize everything.

                CMSIS_SET(RCC, AHB1RSTR, GPDMA1RST   , true);
                CMSIS_SET(RCC, AHB2RSTR, DCMI_PSSIRST, true);

                NVIC_DISABLE(GPDMA1_Channel7);
                NVIC_DISABLE(DCMI_PSSI);

                _OVCAM_driver = (struct OVCAMDriver) {0};

                yield = true;

            } break;

        }

    }

}



////////////////////////////////////////////////////////////////////////////////



// @/`OVCAM DMA Block Repetitions`:
//
// We configure the DMA channel in such a way that it always
// expect more data than there'll actually be in the transfer.
// This is so we can handle variable lengthed data transfers,
// since the size of a JPEG image can vary scene-to-scene.
//
// Thus, the data transfer should technically never complete;
// if it does, then this means the framebuffer got too full.
//
// Note that the DMA has a maximum transfer size limit of
// about 64 KiB; to transfer more than that, we need to do
// multiple reptitions in smaller block chunks. Thus, the
// framebuffer should be a multiple of the block size.
