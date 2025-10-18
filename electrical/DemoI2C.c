#include "system.h"
#include "uxart.c"
#include "i2c.c"



#define DEMO_MODE 1 // See below for different kinds of tests.



extern noret void
main(void)
{

    STPY_init();
    UXART_init(UXARTHandle_stlink);
    I2C_reinit(I2CHandle_queen);
    I2C_reinit(I2CHandle_bee);

    switch (DEMO_MODE)
    {

        ////////////////////////////////////////////////////////////////////////////////
        //
        // Scan for 7-bit and 10-bit I2C slave addresses.
        //

        case 0:
        {

            for
            (
                enum I2CAddressType address_type = I2CAddressType_seven;
                ;
                address_type = !address_type // Flip-flop between 7-bit and 10-bit addressing.
            )
            {

                // Get the address range.

                u32 start_address = {0};
                u32 end_address   = {0};

                switch (address_type)
                {
                    case I2CAddressType_seven:
                    {
                        start_address = 0b0000'1000; // @/`I2C Slave Address`.
                        end_address   = 0b0111'0111; // "
                    } break;

                    case I2CAddressType_ten:
                    {
                        start_address = 0;
                        end_address   = (1 << 10) - 1;
                    } break;

                    default: panic;
                }



                // Test every valid 7-bit and 10-bit addresses to
                // see what slaves are connected to the I2C bus.

                for
                (
                    u32 slave_address = start_address;
                    slave_address <= end_address;
                    slave_address += 1
                )
                {

                    // We try to read a single byte from the slave at the
                    // current slave address to see if we get an acknowledge.

                    enum I2CDriverError error =
                        I2C_blocking_transfer
                        (
                            I2CHandle_queen,
                            slave_address,
                            address_type,
                            I2COperation_read,
                            &(u8) {0},
                            1
                        );



                    // Check the results of the transfer.

                    switch (error)
                    {
                        case I2CDriverError_none:
                        {
                            stlink_tx("Slave 0x%03X acknowledged!\n", slave_address);
                        } break;

                        case I2CDriverError_no_acknowledge:
                        {
                            stlink_tx("Slave 0x%03X didn't acknowledge!\n", slave_address);
                        } break;

                        default: panic;
                    }



                    // Bit of breather...

                    GPIO_TOGGLE(led_green);

                    spinlock_nop(1'000'000);

                }



                // Longer breather before starting all over again.

                stlink_tx("--------------------------------\n");

                spinlock_nop(400'000'000);

            }

        } break;



        ////////////////////////////////////////////////////////////////////////////////
        //
        // Have an I2C peripheral read/write data to another I2C peripheral.
        //

        case 1:
        {

            for (;;)
            {

                enum I2CAddressType address_type  = I2CAddressType_seven;
                u32                 slave_address = TMP_SLAVE_ADDRESS;

                static u8 TMP = 0;

                enum I2CDriverError error =
                    I2C_blocking_transfer
                    (
                        I2CHandle_queen,
                        slave_address,
                        address_type,
                        I2COperation_read,
                        &TMP,
                        1
                    );

                TMP += 1;


                // Check the results of the transfer.

                switch (error)
                {
                    case I2CDriverError_none:
                    {
                        stlink_tx("Slave 0x%03X acknowledged!\n", slave_address);
                    } break;

                    case I2CDriverError_no_acknowledge:
                    {
                        stlink_tx("Slave 0x%03X didn't acknowledge!\n", slave_address);
                    } break;

                    default: panic;
                }



                // Bit of breather...

                GPIO_TOGGLE(led_green);

                spinlock_nop(100'000'000);

            }

        } break;



        default: panic;

    }

}



static void
INTERRUPT_i2c_slave_callback(enum I2CHandle handle, enum I2CSlaveEvent event, u8* data)
{
    switch (handle)
    {

        case I2CHandle_bee: switch (event)
        {

            case I2CSlaveEvent_data_available_to_read:
            {
                // TODO.
            } break;

            case I2CSlaveEvent_ready_to_transmit_data:
            {
                static u8 TMP = 0;
                *data = ++TMP;
            } break;

            case I2CSlaveEvent_stop_signaled:
            {
                // TODO.
            } break;

            default: panic;

        } break;

        case I2CHandle_queen : panic;
        default              : panic;

    }
}
