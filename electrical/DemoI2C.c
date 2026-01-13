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

                    enum I2CMasterError error =
                        I2C_transfer
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
                        case I2CMasterError_none:
                        {
                            stlink_tx("Slave 0x%03X acknowledged!\n", slave_address);
                        } break;

                        case I2CMasterError_no_acknowledge:
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
        // Have an I2C peripheral on the MCU talk
        // with another I2C peripheral on the same MCU.
        //

        case 1:
        {

            for (;;)
            {

                stlink_tx("Queen : writing to bee...\n");

                {

                    // Send data to the slave.

                    char message[] = "Doing taxes suck!";

                    enum I2CMasterError error =
                        I2C_transfer
                        (
                            I2CHandle_queen,
                            I2C_TABLE[I2CHandle_bee].I2Cx_SLAVE_ADDRESS,
                            I2CAddressType_seven,
                            I2COperation_write,
                            (u8*) message,
                            sizeof(message) - 1
                        );



                    // Check the results of the transfer.

                    switch (error)
                    {
                        case I2CMasterError_none:
                        {
                            stlink_tx("Queen : transmission successful!\n");
                        } break;

                        case I2CMasterError_no_acknowledge:
                        {
                            stlink_tx("Queen : transmission failed!\n");
                        } break;

                        default: panic;
                    }



                    // Bit of breather...

                    GPIO_TOGGLE(led_green);

                    spinlock_nop(300'000'000);

                }

                stlink_tx("\n");

                stlink_tx("Queen : reading from bee...\n");

                {

                    // Read data from the slave.

                    char response[24] = {0};

                    enum I2CMasterError error =
                        I2C_transfer
                        (
                            I2CHandle_queen,
                            I2C_TABLE[I2CHandle_bee].I2Cx_SLAVE_ADDRESS,
                            I2CAddressType_seven,
                            I2COperation_read,
                            (u8*) response,
                            sizeof(response)
                        );



                    // Check the results of the transfer.

                    switch (error)
                    {
                        case I2CMasterError_none:
                        {
                            stlink_tx("Queen : reception successful! : `%.*s`\n", sizeof(response), response);
                        } break;

                        case I2CMasterError_no_acknowledge:
                        {
                            stlink_tx("Queen : reception failed!\n");
                        } break;

                        default: panic;
                    }



                    // Bit of breather...

                    GPIO_TOGGLE(led_green);

                    spinlock_nop(300'000'000);

                }

                stlink_tx("\n");

            }

        } break;



        default: panic;

    }

}



////////////////////////////////////////////////////////////////////////////////
//
// For I2C drivers that are in the role of slave,
// a callback is used for whenever a particular
// event has happened (e.g. received data).
// A state machine should be built upon this callback
// for more complex slave-master transactions.
//

static void
INTERRUPT_i2c_slave_callback(enum I2CHandle handle, enum I2CSlaveEvent event, u8* data)
{
    switch (handle)
    {

        case I2CHandle_bee:
        {

            static i32 reply_index = 0;
            static i32 stop_count  = 0;

            switch (event)
            {

                ////////////////////////////////////////////////////////////////////////////////
                //
                // The master gave us data.
                //

                case I2CSlaveEvent_data_available_to_read:
                {
                    stlink_tx("Bee   : got byte : `%c`\n", *data);
                } break;



                ////////////////////////////////////////////////////////////////////////////////
                //
                // The master wants data.
                //

                case I2CSlaveEvent_ready_to_transmit_data:
                {

                    // Send the next byte for the master.

                    char reply[20] = {0};
                    snprintf_(reply, sizeof(reply), "Eat the rich! x %d", stop_count);

                    if (reply_index < sizeof(reply))
                    {
                        *data = reply[reply_index];
                    }

                    reply_index += 1;



                    // Note that there's nothing to stop the master from reading
                    // more data than they should be doing; best we can
                    // do as the slave is transmit back the dummy byte 0xFF.

                    if (*data == 0xFF)
                    {
                        stlink_tx("Bee   : dummy byte : 0x%02X\n", *data);
                    }
                    else
                    {
                        stlink_tx("Bee   : sending byte : `%c`\n", *data);
                    }

                } break;



                ////////////////////////////////////////////////////////////////////////////////
                //
                // A read/write transfer just ended.
                //

                case I2CSlaveEvent_stop_signaled:
                {

                    stlink_tx("Bee   : stop detected\n");

                    reply_index  = 0;
                    stop_count  += 1;

                } break;



                default: panic;

            } break;

        } break;

        case I2CHandle_queen : panic;
        default              : panic;

    }
}
