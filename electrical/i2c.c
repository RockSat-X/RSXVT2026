////////////////////////////////////////////////////////////////////////////////



enum I2CHandle : u32
{
    I2CHandle_1,
    I2CHandle_COUNT,
};

static const struct { I2C_TypeDef* I2C; } I2C_TABLE[] =
    {
        [I2CHandle_1] = { .I2C = I2C1, }
    };

enum I2CDriverState : u32
{
    I2CDriverState_standby,
    I2CDriverState_scheduled_transfer,
    I2CDriverState_transferring,
    I2CDriverState_stopping,
};

enum I2CDriverError : u32
{
    I2CDriverError_none,
    I2CDriverError_no_acknowledge,
};

struct I2CDriver
{
    volatile enum I2CDriverState state;
    u8                           address;
    b32                          reading;
    u8*                          pointer;
    i32                          amount;
    i32                          progress;
    enum I2CDriverError          error;
};

static struct I2CDriver _I2C_drivers[I2CHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



#undef  _EXPAND_HANDLE
#define _EXPAND_HANDLE                                         \
    if (!(0 <= handle && handle < I2CHandle_COUNT)) panic;     \
    struct I2CDriver* const driver = &_I2C_drivers[handle];    \
    I2C_TypeDef*      const I2C    = I2C_TABLE    [handle].I2C



////////////////////////////////////////////////////////////////////////////////



static useret enum I2CDriverError
I2C_blocking_transfer
(
    enum I2CHandle handle,
    u8             address, // 8-bit address where the LSb is don't-care.
    b32            reading,
    u8*            pointer,
    i32            amount
)
{

    _EXPAND_HANDLE;

    if (!pointer)
        panic;

    if (amount <= 0)
        panic;



    // Schedule the transfer.

    switch (driver->state)
    {
        case I2CDriverState_standby:
        {

            driver->address  = address;
            driver->reading  = reading;
            driver->pointer  = pointer;
            driver->amount   = amount;
            driver->progress = 0;
            driver->error    = I2CDriverError_none;
            __DMB();
            driver->state    = I2CDriverState_scheduled_transfer;

            NVIC_SET_PENDING(I2C1_EV); // TODO Abstract away.

        } break;

        case I2CDriverState_scheduled_transfer : panic;
        case I2CDriverState_transferring       : panic;
        case I2CDriverState_stopping           : panic;
        default                                : panic;
    }



    // Wait until the transfer is done.

    while (true)
    {
        switch (driver->state)
        {

            // The driver just finished!

            case I2CDriverState_standby:
            {
                return driver->error;
            } break;



            // The driver is still busy with our transfer.

            case I2CDriverState_scheduled_transfer:
            case I2CDriverState_transferring:
            case I2CDriverState_stopping:
            {
                // Keep waiting...
            } break;



            default: panic;

        }
    }

}



////////////////////////////////////////////////////////////////////////////////



static void
I2C_reinit(enum I2CHandle handle)
{

    _EXPAND_HANDLE;

    *driver = (struct I2CDriver) {0};

    #define I2C1_TIMINGR_PRESC_init 11     // TODO Ad-hoc.
    #define I2C1_TIMINGR_SCL_init   250    // TODO Ad-hoc.
    CMSIS_SET(RCC, CCIPR4, I2C1SEL, 0b10); // TODO Ad-hoc.



    // Reset-cycle the peripheral.

    CMSIS_SET(RCC, APB1LRSTR, I2C1RST, true ); // TODO Abstract away.
    CMSIS_SET(RCC, APB1LRSTR, I2C1RST, false); // TODO Abstract away.



    // Enable the interrupts.

    NVIC_ENABLE(I2C1_EV); // TODO Abstract away.
    NVIC_ENABLE(I2C1_ER); // TODO Abstract away.



    // Clock the peripheral.

    CMSIS_SET(RCC, APB1LENR, I2C1EN, true); // TODO Abstract away.



    // Configure the peripheral.

    CMSIS_SET // TODO Look over again.
    (
        I2C   , TIMINGR                , // TODO Handle other timing requirements?
        PRESC , I2C1_TIMINGR_PRESC_init, // Set the time base unit.
        SCLDEL, 0                      , // TODO Important?
        SDADEL, 0                      , // TODO Important?
        SCLH  , I2C1_TIMINGR_SCL_init  , // Determines the amount of high time.
        SCLL  , I2C1_TIMINGR_SCL_init  , // Determines the amount of low time.
    );

    CMSIS_SET // TODO Look over again.
    (
        I2C   , CR1   , // Interrupts for:
        ERRIE , true  , //     - Error.
        TCIE  , true  , //     - Transfer completed.
        STOPIE, true  , //     - STOP signal.
        NACKIE, true  , //     - NACK signal.
        RXIE  , true  , //     - Reception of data.
        TXIE  , true  , //     - Transmission of data.
        DNF   , 15    , // Max out the digital filtering.
        PE    , true  , // Enable the peripheral.
    );

}



////////////////////////////////////////////////////////////////////////////////



static useret enum I2CUpdateOnce : u32
{
    I2CUpdateOnce_again,
    I2CUpdateOnce_yield,
}
_I2C_update_once(enum I2CHandle handle)
{

    _EXPAND_HANDLE;



    u32 interrupt_status = I2C->ISR;

    enum I2CInterruptEvent : u32
    {
        I2CInterruptEvent_none,
        I2CInterruptEvent_nack_signaled,
        I2CInterruptEvent_stop_signaled,
        I2CInterruptEvent_transfer_completed,
        I2CInterruptEvent_data_available_to_read,
        I2CInterruptEvent_ready_to_transmit_data,
    };

    enum I2CInterruptEvent interrupt_event = {0};



    // Bus error.
    // @/pg 2116/sec 48.4.17/`H533rm`.
    if (CMSIS_GET_FROM(interrupt_status, I2C, ISR, BERR))
    {
        panic;
    }



    // Arbitration loss.
    // @/pg 2116/sec 48.4.17/`H533rm`.
    else if (CMSIS_GET_FROM(interrupt_status, I2C, ISR, ARLO))
    {
        panic;
    }



    // Data overrun/overflow.
    // @/pg 2117/sec 48.4.17/`H533rm`.
    else if (CMSIS_GET_FROM(interrupt_status, I2C, ISR, OVR))
    {
        panic;
    }



    // Packet error checking.
    // @/pg 2117/sec 48.4.17/`H533rm`.
    else if (CMSIS_GET_FROM(interrupt_status, I2C, ISR, PECERR))
    {
        panic;
    }



    // Timeout.
    // @/pg 2117/sec 48.4.17/`H533rm`.
    else if (CMSIS_GET_FROM(interrupt_status, I2C, ISR, TIMEOUT))
    {
        panic;
    }



    // Alert.
    // @/pg 2117/sec 48.4.17/`H533rm`.
    else if (CMSIS_GET_FROM(interrupt_status, I2C, ISR, ALERT))
    {
        panic;
    }



    // Transfer reloaded.
    // @/pg 2095/sec 48.4.9/`H533rm`.
    else if (CMSIS_GET_FROM(interrupt_status, I2C, ISR, TCR))
    {
        panic;
    }



    // @/pg 2085/sec 48.4.8/`H533rm`.
    else if (CMSIS_GET_FROM(interrupt_status, I2C, ISR, NACKF))
    {
        interrupt_event = I2CInterruptEvent_nack_signaled;
    }



    // @/pg 2085/sec 48.4.8/`H533rm`.
    else if (CMSIS_GET_FROM(interrupt_status, I2C, ISR, STOPF))
    {
        interrupt_event = I2CInterruptEvent_stop_signaled;
    }



    // @/pg 2095/sec 48.4.9/`H533rm`.
    else if (CMSIS_GET_FROM(interrupt_status, I2C, ISR, TC))
    {
        interrupt_event = I2CInterruptEvent_transfer_completed;
    }



    // @/pg 2089/sec 48.4.8/`H533rm`.
    else if (CMSIS_GET_FROM(interrupt_status, I2C, ISR, RXNE))
    {
        interrupt_event = I2CInterruptEvent_data_available_to_read;
    }



    // @/pg 2085/sec 48.4.8/`H533rm`.
    else if (CMSIS_GET_FROM(interrupt_status, I2C, ISR, TXIS))
    {
        interrupt_event = I2CInterruptEvent_ready_to_transmit_data;
    }



    // Nothing notable happened.
    else
    {
        interrupt_event = I2CInterruptEvent_none;
    }



    switch (driver->state)
    {

        // Nothing to do until the user schedules a transfer.

        case I2CDriverState_standby:
        {

            if (interrupt_event)
                panic;

            return I2CUpdateOnce_yield;

        } break;



        // The user has a transfer they want to do.

        case I2CDriverState_scheduled_transfer:
        {

            if (interrupt_event)
                panic;

            if (!(1 <= driver->amount && driver->amount <= 255))
                panic;

            if (CMSIS_GET(I2C, CR2, START))
                panic;

            if (driver->error)
                panic;

            CMSIS_SET
            (
                I2C   , CR2              ,
                SADD  , driver->address  ,
                RD_WRN, !!driver->reading,
                NBYTES, driver->amount   ,
                START , true             ,
            );

            driver->state = I2CDriverState_transferring;

            return I2CUpdateOnce_again;

        } break;



        // We're in the process of doing the transfer.

        case I2CDriverState_transferring: switch (interrupt_event)
        {

            // Nothing to do until there's an interrupt status.

            case I2CInterruptEvent_none:
            {

                if (driver->error)
                    panic;

                return I2CUpdateOnce_yield;

            } break;



            // The slave didn't acknowledge us.

            case I2CInterruptEvent_nack_signaled:
            {

                if (driver->error)
                    panic;

                CMSIS_SET(I2C, ICR, NACKCF, true);

                // Begin sending out the STOP signal.
                CMSIS_SET(I2C, CR2, STOP, true);

                driver->state = I2CDriverState_stopping;
                driver->error = I2CDriverError_no_acknowledge;

                return I2CUpdateOnce_again;

            } break;



            // The slave sent us some data.

            case I2CInterruptEvent_data_available_to_read:
            {

                if (driver->error)
                    panic;

                if (!(0 <= driver->progress && driver->progress < driver->amount))
                    panic;

                // Pop from the RX-buffer.
                u8 data = CMSIS_GET(I2C, RXDR, RXDATA);

                driver->pointer[driver->progress]  = data;
                driver->progress                  += 1;

                return I2CUpdateOnce_again;

            } break;



            // We're ready to send some data to the slave.

            case I2CInterruptEvent_ready_to_transmit_data:
            {

                if (driver->error)
                    panic;

                if (!(0 <= driver->progress && driver->progress < driver->amount))
                    panic;

                u8 data = driver->pointer[driver->progress];

                // Push data into the RX-buffer.
                CMSIS_SET(I2C, TXDR, TXDATA, data);

                driver->progress += 1;

                return I2CUpdateOnce_again;

            } break;



            // We finished transferring all the data.

            case I2CInterruptEvent_transfer_completed:
            {

                if (driver->error)
                    panic;

                if (!iff(driver->progress == driver->amount, !driver->error))
                    panic;

                // Begin sending out the STOP signal.
                CMSIS_SET(I2C, CR2, STOP, true);

                driver->state = I2CDriverState_stopping;

                return I2CUpdateOnce_again;

            } break;



            case I2CInterruptEvent_stop_signaled : panic;
            default                              : panic;

        } break;



        // We're in the process of stopping the transfer.

        case I2CDriverState_stopping: switch (interrupt_event)
        {

            // Nothing to do until there's an interrupt status.

            case I2CInterruptEvent_none:
            {
                return I2CUpdateOnce_yield;
            } break;



            // We're finally done with the transfer!

            case I2CInterruptEvent_stop_signaled:
            {

                if (!iff(driver->progress == driver->amount, !driver->error))
                    panic;

                // Clear the flag.
                CMSIS_SET(I2C, ICR, STOPCF, true);

                driver->state = I2CDriverState_standby;

                return I2CUpdateOnce_again;

            } break;

            case I2CInterruptEvent_transfer_completed     : panic;
            case I2CInterruptEvent_data_available_to_read : panic;
            case I2CInterruptEvent_ready_to_transmit_data : panic;
            case I2CInterruptEvent_nack_signaled          : panic;
            default                                       : panic;

        } break;



        default: panic;

    }

}



static void
_I2C_update_entirely(enum I2CHandle handle)
{
    _EXPAND_HANDLE;

    for (b32 yield = false; !yield;)
    {

        enum I2CUpdateOnce result = _I2C_update_once(handle);

        yield = (result == I2CUpdateOnce_yield);

        switch (result)
        {
            case I2CUpdateOnce_again:
            {
                // The state-machine will be updated again.
            } break;

            case I2CUpdateOnce_yield:
            {
                // We can stop updating the state-machine for now.
            } break;

            default: panic;
        }

    }
}



////////////////////////////////////////////////////////////////////////////////



INTERRUPT_I2C1_EV
{
    _I2C_update_entirely(I2CHandle_1);
}



INTERRUPT_I2C1_ER
{
    sorry
}



////////////////////////////////////////////////////////////////////////////////
