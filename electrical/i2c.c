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

struct I2CDriver
{
    volatile enum I2CDriverState state;
    u8                           address;
    b32                          reading;
    u8*                          pointer;
    i32                          amount;
    i32                          progress;
};

static struct I2CDriver _I2C_drivers[I2CHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



#undef  _EXPAND_HANDLE
#define _EXPAND_HANDLE                                         \
    struct I2CDriver* const driver = &_I2C_drivers[handle];    \
    I2C_TypeDef*      const I2C    = I2C_TABLE    [handle].I2C



////////////////////////////////////////////////////////////////////////////////



static useret enum I2CBlockingTransfer
{
    I2CBlockingTransfer_done,
}
I2C_blocking_transfer
(
    enum I2CHandle handle,
    u8             address,
    b32            reading,
    u8*            pointer,
    i32            amount
)
{

    _EXPAND_HANDLE;



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

            __DMB();

            driver->state = I2CDriverState_scheduled_transfer;

            NVIC_SET_PENDING(I2C1_EV);
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
            case I2CDriverState_standby:
            {
                return I2CBlockingTransfer_done;
            } break;

            case I2CDriverState_scheduled_transfer:
            case I2CDriverState_transferring:
            case I2CDriverState_stopping:
            {
                // The driver is busy...
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

    CMSIS_SET(RCC, APB1LRSTR, I2C1RST, true );
    CMSIS_SET(RCC, APB1LRSTR, I2C1RST, false);



    // Enable the interrupts.

    NVIC_ENABLE(I2C1_EV);
    NVIC_ENABLE(I2C1_ER);



    // Clock the peripheral.

    CMSIS_SET(RCC, APB1LENR, I2C1EN, true);



    // Configure the peripheral.

    CMSIS_SET
    (
        I2C   , TIMINGR                , // TODO Handle other timing requirements?
        PRESC , I2C1_TIMINGR_PRESC_init, // Set the time base unit.
        SCLDEL, 0                      , // TODO Important?
        SDADEL, 0                      , // TODO Important?
        SCLH  , I2C1_TIMINGR_SCL_init  , // Determines the amount of high time.
        SCLL  , I2C1_TIMINGR_SCL_init  , // Determines the amount of low time.
    );

    CMSIS_SET
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



static useret enum I2CUpdateOnce
{
    I2CUpdateOnce_again,
    I2CUpdateOnce_yield,
}
_I2C_update_once(enum I2CHandle handle)
{

    _EXPAND_HANDLE;

    u32 i2c_status = I2C->ISR;



    // Bus error.
    // @/pg 2116/sec 48.4.17/`H533rm`.

    if (CMSIS_GET_FROM(i2c_status, I2C, ISR, BERR))
    {
        sorry
    }



    // Arbitration loss.
    // @/pg 2116/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, ARLO))
    {
        sorry
    }



    // Data overrun/overflow.
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, OVR))
    {
        sorry
    }



    // Packet error checking.
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, PECERR))
    {
        sorry
    }



    // Timeout.
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, TIMEOUT))
    {
        sorry
    }



    // Alert.
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, ALERT))
    {
        sorry
    }



    // A NACK was received.
    // @/pg 2085/sec 48.4.8/`H533rm`.

    else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, NACKF))
    {
        sorry
    }



    // A STOP was sent.
    // @/pg 2085/sec 48.4.8/`H533rm`.

    else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, STOPF))
    {

        CMSIS_SET(I2C, ICR, STOPCF, true); // We're done with the transfer!

        driver->state = I2CDriverState_standby;

        return I2CUpdateOnce_again;

    }



    // Transfer completed.
    // @/pg 2095/sec 48.4.9/`H533rm`.

    else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, TC))
    {

        CMSIS_SET(I2C, CR2, STOP, true); // Begin sending out the STOP signal.

        driver->state = I2CDriverState_stopping;

        return I2CUpdateOnce_again;

    }



    // Transfer reloaded.
    // @/pg 2095/sec 48.4.9/`H533rm`.

    else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, TCR))
    {
        sorry
    }



    // Received data.
    // @/pg 2089/sec 48.4.8/`H533rm`.

    else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, RXNE))
    {
        u8 data = CMSIS_GET(I2C, RXDR, RXDATA); // Pop data from the RX-buffer.

        driver->pointer[driver->progress]  = data;
        driver->progress                  += 1;

        return I2CUpdateOnce_again;
    }



    // Data can be sent.
    // @/pg 2085/sec 48.4.8/`H533rm`.

    else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, TXIS))
    {
        sorry
    }



    // No interrupt status flags to handle right now.

    else switch (driver->state)
    {

        // The driver has nothing to do currently.

        case I2CDriverState_standby:
        {
            return I2CUpdateOnce_yield; // Nothing to do until the user schedules a transfer.
        } break;



        // The user has a transfer they want to do.

        case I2CDriverState_scheduled_transfer:
        {

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



        // We've already started the current transfer.

        case I2CDriverState_transferring:
        {
            return I2CUpdateOnce_yield; // Nothing to do until there's an interrupt status.
        } break;



        // We're in the process of stopping the transfer.

        case I2CDriverState_stopping:
        {
            return I2CUpdateOnce_yield; // Nothing to do until there's an interrupt status.
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
            case I2CUpdateOnce_again : break; // The state-machine will be updated again.
            case I2CUpdateOnce_yield : break; // We can stop updating the state-machine for now.
            default                  : panic;
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
