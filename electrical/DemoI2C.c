#include "system.h"
#include "timekeeping.c"
#include "uxart.c"
#include "i2c.c"



#define DEMO_MODE 1 // See below for different kinds of tests.



static void
reinitialize_i2c_driver(enum I2CHandle handle)
{

    // When the I2C driver encounters an issue, it quickly goes into a bugged state
    // where nothing can be done until the driver is completely reinitialized.
    //
    // This could be due to a bug within the driver code, or the user passing in
    // invalid parameters, or some sort of unexpected error on the I2C bus clock
    // and data lines.

    enum I2CReinitResult result = I2C_reinit(handle);

    switch (result)
    {
        case I2CReinitResult_success : break;
        case I2CReinitResult_bug     : sorry
        default                      : sorry
    }

}



static b32 // Success?
queen_do(struct I2CDoJob job)
{
    while (true)
    {

        enum I2CDoResult result = {0};

        do
        {
            result = I2C_do(&job);
        }
        while (result == I2CDoResult_working); // Spinlock until the job's done.

        switch (result)
        {
            case I2CDoResult_success               : break;
            case I2CDoResult_no_acknowledge        : stlink_tx("[Queen] (0x%03X) no_acknowledge"        "\n", job.address); break;
            case I2CDoResult_clock_stretch_timeout : stlink_tx("[Queen] (0x%03X) clock_stretch_timeout" "\n", job.address); break;
            case I2CDoResult_bus_misbehaved        : stlink_tx("[Queen] (0x%03X) bus_misbehaved"        "\n", job.address); break;
            case I2CDoResult_watchdog_expired      : stlink_tx("[Queen] (0x%03X) watchdog_expired"      "\n", job.address); break;
            case I2CDoResult_working               : sorry
            case I2CDoResult_bug                   : sorry
            default                                : sorry
        }

        if (result == I2CDoResult_success)
        {
            return true;
        }
        else if (result == I2CDoResult_no_acknowledge)
        {
            return false;
        }
        else
        {
            reinitialize_i2c_driver(job.handle); // Other errors are fatal and must have the driver be reinitialized.
            return false;
        }

    }
}



extern noret void
main(void)
{

    STPY_init();
    UXART_reinit(UXARTHandle_stlink);

    {

        // Set the prescaler that'll affect all timers' kernel frequency.

        CMSIS_SET(RCC, CFGR1, TIMPRE, STPY_GLOBAL_TIMER_PRESCALER);



        // Configure the other registers to get timekeeping up and going.

        TIMEKEEPING_partial_init();

    }

    reinitialize_i2c_driver(I2CHandle_queen);
    reinitialize_i2c_driver(I2CHandle_bee);



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

                u16 start_address = {0};
                u16 end_address   = {0};

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

                    default: sorry
                }



                // Test every valid 7-bit and 10-bit addresses to
                // see what slaves are connected to the I2C bus.

                for
                (
                    u16 slave_address = start_address;
                    slave_address <= end_address;
                    slave_address += 1
                )
                {

                    // We try to read a single byte from the slave at the
                    // current slave address to see if we get an acknowledge.

                    b32 success =
                        queen_do
                        (
                            (struct I2CDoJob)
                            {
                                .handle       = I2CHandle_queen,
                                .address_type = address_type,
                                .address      = slave_address,
                                .writing      = false,
                                .repeating    = false,
                                .pointer      = &(u8) {0},
                                .amount       = 1,
                            }
                        );

                    if (success)
                    {
                        stlink_tx("[Queen] (0x%03X) Acknowledged!\n", slave_address);
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

            for (i32 iteration = 0;; iteration += 1)
            {

                stlink_tx("Queen : writing to bee...\n");
                {

                    char message[] =
                        "Doing taxes suck! "
                        "It's incredibly confusing and predatory that it's not even accessible. "
                        "I mean, the tax companies actually lobby congress to make the system as "
                        "complicated as it is in order to reap profits off of the poor working people "
                        "who can't understand the system. "
                        "That's captialism for you.";

                    b32 success =
                        queen_do
                        (
                            (struct I2CDoJob)
                            {
                                .handle       = I2CHandle_queen,
                                .address_type = I2CAddressType_seven,
                                .address      = I2C_TABLE[I2CHandle_bee].I2Cx_SLAVE_ADDRESS,
                                .writing      = true,
                                .repeating    = !!(iteration & (1 << 2)),
                                .pointer      = (u8*) message,
                                .amount       = sizeof(message) - 1,
                            }
                        );

                    if (success)
                    {
                        stlink_tx("Queen : transmission successful!\n");
                    }
                    else
                    {
                        spinlock_nop(1'000'000);
                    }

                    GPIO_TOGGLE(led_green);
                    spinlock_nop(200'000'000);

                }
                stlink_tx("\n");

                stlink_tx("Queen : reading from bee...\n");
                {

                    char response[512] = {0};

                    b32 success =
                        queen_do
                        (
                            (struct I2CDoJob)
                            {
                                .handle       = I2CHandle_queen,
                                .address_type = I2CAddressType_seven,
                                .address      = I2C_TABLE[I2CHandle_bee].I2Cx_SLAVE_ADDRESS,
                                .writing      = false,
                                .repeating    = !!(iteration & (1 << 3)),
                                .pointer      = (u8*) response,
                                .amount       = sizeof(response),
                            }
                        );

                    if (success)
                    {
                        stlink_tx("Queen : reception successful! : `%.*s`\n", sizeof(response), response);
                    }
                    else
                    {
                        spinlock_nop(1'000'000);
                    }

                    GPIO_TOGGLE(led_green);
                    spinlock_nop(200'000'000);

                }
                stlink_tx("\n\n\n");

            }

        } break;



        default: sorry

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

INTERRUPT_I2Cx_bee(enum I2CSlaveCallbackEvent event, u8* data)
{

    static i32 reply_index = 0;
    static i32 stop_count  = 0;

    switch (event)
    {



        ////////////////////////////////////////////////////////////////////////////////
        //
        // A read/write transfer is starting.
        //

        {
            case I2CSlaveCallbackEvent_transmission_initiated : stlink_tx("Bee   : beginning to send data..."            "\n"); goto BEGINNING;
            case I2CSlaveCallbackEvent_reception_initiated    : stlink_tx("Bee   : getting data..."                      "\n"); goto BEGINNING;
            case I2CSlaveCallbackEvent_transmission_repeated  : stlink_tx("Bee   : beginning to send data... (repeated)" "\n"); goto BEGINNING;
            case I2CSlaveCallbackEvent_reception_repeated     : stlink_tx("Bee   : getting data... (repeated)"           "\n"); goto BEGINNING;
            BEGINNING:;

            reply_index  = 0;
            stop_count  += 1;

        } break;



        ////////////////////////////////////////////////////////////////////////////////
        //
        // The master gave us data.
        //

        case I2CSlaveCallbackEvent_data_available_to_read:
        {
            if (32 <= *data && *data <= 126)
            {
                stlink_tx("Bee   : got byte : `%c`\n", *data);
            }
            else
            {
                stlink_tx("Bee   : got byte : 0x%02X\n", *data);
            }
        } break;



        ////////////////////////////////////////////////////////////////////////////////
        //
        // The master wants data.
        //

        case I2CSlaveCallbackEvent_ready_to_transmit_data:
        {

            // Send the next byte for the master.

            char reply[512] = {0};
            snprintf_
            (
                reply,
                sizeof(reply),
                "I definitely agree. "
                "Tax code? The only code I know is the one I'm writing right now. "
                "You know what we should do, fellow anonymous? "
                "Eat the rich! x %d"
                "Those businessmen? "
                "Come the revolution, we're gonna string 'em up! "
                "All of them, every single one of them from every possible demographic of businessmen. "
                "Yes, even the businessmen women! The businessmen children! "
                "I hate them all!",
                stop_count
            );

            if (reply_index < (i32) strnlen(reply, sizeof(reply)))
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

        case I2CSlaveCallbackEvent_stop_signaled:
        {
            stlink_tx("Bee   : stop detected\n");
        } break;



        ////////////////////////////////////////////////////////////////////////////////
        //
        // The I2C driver needs to be reinitialized due to an issue.
        //

        {
            case I2CSlaveCallbackEvent_clock_stretch_timeout : stlink_tx("[Bee] clock_stretch_timeout" "\n"); goto ERROR;
            case I2CSlaveCallbackEvent_bus_misbehaved        : stlink_tx("[Bee] bus_misbehaved"        "\n"); goto ERROR;
            case I2CSlaveCallbackEvent_watchdog_expired      : stlink_tx("[Bee] watchdog_expired"      "\n"); goto ERROR;
            ERROR:;

            reinitialize_i2c_driver(I2CHandle_bee);
            spinlock_nop(1'000'000);

        } break;

        case I2CSlaveCallbackEvent_bug : sorry
        default                        : sorry

    }

}
