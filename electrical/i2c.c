////////////////////////////////////////////////////////////////////////////////



#include "i2c_aliases.meta"

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

enum I2COperation : u32
{
    I2COperation_write = false,
    I2COperation_read  = true,
};

struct I2CDriver
{
    volatile enum I2CDriverState state;
    u8                           address;
    enum I2COperation            operation;
    u8*                          pointer;
    i32                          amount;
    i32                          progress;
    enum I2CDriverError          error;
};

static struct I2CDriver _I2C_drivers[I2CHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



static useret enum I2CDriverError
I2C_blocking_transfer
(
    enum I2CHandle    handle,
    u8                address, // @/`I2C Slave Address`.
    enum I2COperation operation,
    u8*               pointer,
    i32               amount
)
{

    _EXPAND_HANDLE

    if (!pointer)
        panic;

    if (amount <= 0)
        panic;

    if ((address & 1) || !(0b0001'000'0 <= address && address <= 0b1110'111'0))
        panic; // @/`I2C Slave Address`.



    // Schedule the transfer.

    switch (driver->state)
    {
        case I2CDriverState_standby:
        {

            driver->address   = address;
            driver->operation = operation;
            driver->pointer   = pointer;
            driver->amount    = amount;
            driver->progress  = 0;
            driver->error     = I2CDriverError_none;
            __DMB();
            driver->state     = I2CDriverState_scheduled_transfer;

            NVIC_SET_PENDING(I2Cx_EV);

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

    _EXPAND_HANDLE



    // Reset-cycle the peripheral.

    CMSIS_PUT(I2Cx_RESET, true );
    CMSIS_PUT(I2Cx_RESET, false);

    *driver = (struct I2CDriver) {0};



    // Enable the interrupts.

    NVIC_ENABLE(I2Cx_EV);
    NVIC_ENABLE(I2Cx_ER);



    // Set the kernel clock source for the I2C peripheral.

    CMSIS_PUT(I2Cx_KERNEL_SOURCE, I2Cx_KERNEL_SOURCE_init);



    // Clock the peripheral.

    CMSIS_PUT(I2Cx_ENABLE, true);



    // Configure the peripheral.

    CMSIS_SET // TODO Look over again.
    (
        I2Cx  , TIMINGR                , // TODO Handle other timing requirements?
        PRESC , I2Cx_TIMINGR_PRESC_init, // Set the time base unit.
        SCLDEL, 0                      , // TODO Important?
        SDADEL, 0                      , // TODO Important?
        SCLH  , I2Cx_TIMINGR_SCL_init  , // Determines the amount of high time.
        SCLL  , I2Cx_TIMINGR_SCL_init  , // Determines the amount of low time.
    );

    CMSIS_SET // TODO Look over again.
    (
        I2Cx  , CR1   , // Interrupts for:
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

    _EXPAND_HANDLE



    enum I2CInterruptEvent : u32
    {
        I2CInterruptEvent_none,
        I2CInterruptEvent_nack_signaled,
        I2CInterruptEvent_stop_signaled,
        I2CInterruptEvent_transfer_completed,
        I2CInterruptEvent_data_available_to_read,
        I2CInterruptEvent_ready_to_transmit_data,
    };
    enum I2CInterruptEvent interrupt_event  = {0};
    u32                    interrupt_status = I2Cx->ISR;



    // Currently not implemented.
    // TODO Handle gracefully?
    // @/pg 2116/sec 48.4.17/`H533rm`.

    if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, BERR))
    {
        panic;
    }



    // Currently not implemented.
    // TODO Handle gracefully?
    // @/pg 2116/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, ARLO))
    {
        panic;
    }



    // Currently not implemented.
    // TODO Detect if clock-stretching goes on for too long?
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, TIMEOUT))
    {
        panic;
    }



    // This status bit is only applicable to SMBus.
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, ALERT))
    {
        panic;
    }



    // This status bit is only applicable to SMBus.
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, PECERR))
    {
        panic;
    }



    // Transfer reloaded.
    // @/pg 2095/sec 48.4.9/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, TCR))
    {
        panic;
    }



    // Clock-stretching is enabled by default, so we
    // shouldn't be getting any data overrun/underrun.
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, OVR))
    {
        panic;
    }



    // @/pg 2085/sec 48.4.8/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, NACKF))
    {
        interrupt_event = I2CInterruptEvent_nack_signaled;
    }



    // @/pg 2085/sec 48.4.8/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, STOPF))
    {
        interrupt_event = I2CInterruptEvent_stop_signaled;
    }



    // @/pg 2095/sec 48.4.9/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, TC))
    {
        interrupt_event = I2CInterruptEvent_transfer_completed;
    }



    // @/pg 2089/sec 48.4.8/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, RXNE))
    {
        interrupt_event = I2CInterruptEvent_data_available_to_read;
    }



    // @/pg 2085/sec 48.4.8/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, TXIS))
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

            if (CMSIS_GET(I2Cx, CR2, START))
                panic;

            if (driver->error)
                panic;

            CMSIS_SET
            (
                I2Cx  , CR2                ,
                SADD  , driver->address    ,
                RD_WRN, !!driver->operation,
                NBYTES, driver->amount     ,
                START , true               ,
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

                CMSIS_SET(I2Cx, ICR, NACKCF, true);

                // Begin sending out the STOP signal.
                CMSIS_SET(I2Cx, CR2, STOP, true);

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
                u8 data = CMSIS_GET(I2Cx, RXDR, RXDATA);

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
                CMSIS_SET(I2Cx, TXDR, TXDATA, data);

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
                CMSIS_SET(I2Cx, CR2, STOP, true);

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

                CMSIS_SET(I2Cx, ICR, STOPCF, true);

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

    _EXPAND_HANDLE

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



// Define the interrupt handler for each I2C peripheral used.

#include "i2c_interrupts.meta"
/* #meta

    for target in PER_TARGET():

        if 'i2c_units' not in target.__dict__:
            continue

        for unit in target.i2c_units:
            for suffix in ('EV', 'ER'):
                Meta.line(f'''
                    INTERRUPT_I2C{unit}_{suffix}
                    {{
                        _I2C_update_entirely(I2CHandle_{unit});
                    }}
                ''')

*/



// Stuff to make working with any I2C unit easy.

/* #include "i2c_aliases.meta"
/* #meta



    # Things to be aliased for I2C.

    IDENTIFIERS = (
        'I2C{}',
        'NVICInterrupt_I2C{}_EV',
        'NVICInterrupt_I2C{}_ER',
        'I2C{}_KERNEL_SOURCE_init',
        'I2C{}_TIMINGR_PRESC_init',
        'I2C{}_TIMINGR_SCL_init',
    )

    CMSIS_TUPLE_TAGS = (
        'I2C{}_RESET',
        'I2C{}_ENABLE',
        'I2C{}_KERNEL_SOURCE',
    )



    # Some target-specific support definitions.

    for target in PER_TARGET():

        if 'i2c_units' not in target.__dict__ or not target.i2c_units:

            Meta.line(
                f'#error Target {target.name} cannot use the I2C driver '
                f'because no I2C unit have been assigned.'
            )

            continue



        # Have the user be able to specify a specific I2C unit.

        Meta.enums('I2CHandle', 'u32', target.i2c_units)



        # A look-up table to allow generic code to be written for any I2C peripheral.

        Meta.lut('I2C_TABLE', (
            (
                f'I2CHandle_{unit}',
                *[
                    (
                        identifier.format('x'),
                        identifier.format(unit)
                    )
                    for identifier in IDENTIFIERS
                ],
                *[
                    (
                        tag.format('x'),
                        CMSIS_TUPLE(SYSTEM_DATABASE[target.mcu][tag.format(unit)])
                    ) for tag in CMSIS_TUPLE_TAGS
                ]
            )
            for unit in target.i2c_units
        ))



    # Macro to mostly bring stuff in the look-up table into the local scope.

    Meta.line(f'#define I2Cx_ I2C_')
    Meta.line('#undef _EXPAND_HANDLE')

    with Meta.enter('#define _EXPAND_HANDLE'):

        Meta.line(f'''

            if (!(0 <= handle && handle < I2CHandle_COUNT))
            {{
                panic;
            }}

            struct I2CDriver* const driver = &_I2C_drivers[handle];

        ''')

        for identifier in IDENTIFIERS + CMSIS_TUPLE_TAGS:
            Meta.line(f'''
                auto const {identifier.format('x')} = I2C_TABLE[handle].{identifier.format('x')};
            ''')

*/



////////////////////////////////////////////////////////////////////////////////



// @/`I2C Slave Address`:
//
// When saying "slave address", it's a bit ambiguous as to whether or not
// this includes the read/write bit. The I2C specification always uses
// the 7-bit convention (the R/W bit is not a part of the "slave address")
// but other people sometime use the 8-bit convention. In this I2C driver, we
// are using the 8-bit convention where the LSb is "reserved" for the R/W bit,
// since this works out better for the register-writes.
//
// So that being said, we should always expect the LSb of the "slave address"
// to be zero. If it's not, the user is likely thinking in the 7-bit convention.
//
// Furthermore, not all I2C addresses are the same;
// some are reserved or have special meaning.
// @/pg 16/tbl 4/`I2C`.
