////////////////////////////////////////////////////////////////////////////////



enum I2CDriverRole : u32
{
    I2CDriverRole_master,
    I2CDriverRole_slave,
};



enum I2CSlaveEvent : u32
{
    I2CSlaveEvent_data_available_to_read,
    I2CSlaveEvent_ready_to_transmit_data,
    I2CSlaveEvent_stop_signaled,
};



#include "i2c_driver_support.meta"
/* #meta

    IMPLEMENT_DRIVER_SUPPORT(
        driver_type = 'I2C',
        cmsis_name  = 'I2C',
        common_name = 'I2Cx',
        entries     = (
            {
                'name'  : '{}_DRIVER_ROLE',
                'value' : lambda driver: f'I2CDriverRole_{driver['role']}',
            },
            {
                'name'  : '{}_SLAVE_ADDRESS',
                'value' : lambda driver: f'((u16) 0x{driver.get('address', 0) :03X})',
            },
            { 'name'      : '{}'                   , 'value'       : ... },
            { 'name'      : 'NVICInterrupt_{}_EV'  , 'value'       : ... },
            { 'name'      : 'NVICInterrupt_{}_ER'  , 'value'       : ... },
            { 'name'      : 'STPY_{}_KERNEL_SOURCE', 'value'       : ... },
            { 'name'      : 'STPY_{}_PRESC'        , 'value'       : ... },
            { 'name'      : 'STPY_{}_SCLH'         , 'value'       : ... },
            { 'name'      : 'STPY_{}_SCLL'         , 'value'       : ... },
            { 'name'      : '{}_RESET'             , 'cmsis_tuple' : ... },
            { 'name'      : '{}_ENABLE'            , 'cmsis_tuple' : ... },
            { 'name'      : '{}_KERNEL_SOURCE'     , 'cmsis_tuple' : ... },
            { 'interrupt' : 'INTERRUPT_{}_EV'                            },
            { 'interrupt' : 'INTERRUPT_{}_ER'                            },
        ),
    )

    for target in PER_TARGET():

        for driver_settings in target.drivers:

            if driver_settings['type'] != 'I2C':
                continue

            address = driver_settings.get('address', None)

            if address is not None and not (0b0000_1000 <= address <= 0b0111_0111):
                raise ValueError(
                    f'Target {repr(target.name)} has '
                    f'I2C peripheral {repr(driver_settings['handle'])} '
                    f'with invalid 7-bit address of {repr(address)}.'
                )

        if any(driver_settings['type'] == 'I2C' and driver_settings['role'] == 'slave' for driver_settings in target.drivers):

            Meta.line('''
                static void
                INTERRUPT_i2c_slave_callback(enum I2CHandle handle, enum I2CSlaveEvent event, u8* data);
            ''')

        else:

            Meta.line('''
                static void
                INTERRUPT_i2c_slave_callback(enum I2CHandle handle, enum I2CSlaveEvent event, u8* data)
                {
                    panic;
                }
            ''')

*/



enum I2CMasterState : u32
{
    I2CMasterState_standby,
    I2CMasterState_scheduled_transfer,
    I2CMasterState_transferring,
    I2CMasterState_stopping,
};

enum I2CSlaveState : u32
{
    I2CSlaveState_standby,
    I2CSlaveState_receiving_data,
    I2CSlaveState_sending_data,
    I2CSlaveState_stopping,
};

enum I2CMasterError : u32
{
    I2CMasterError_none,
    I2CMasterError_no_acknowledge,
};

enum I2CAddressType : u32
{
    I2CAddressType_seven = false,
    I2CAddressType_ten   = true,
};

enum I2COperation : u32
{
    I2COperation_write = false,
    I2COperation_read  = true,
};

struct I2CDriver
{
    union
    {
        struct
        {
            volatile enum I2CMasterState state;
            u32                          address;
            enum I2CAddressType          address_type;
            enum I2COperation            operation;
            u8*                          pointer;
            i32                          amount;
            i32                          progress;
            enum I2CMasterError          error;
        } master;

        struct
        {
            volatile enum I2CSlaveState state;
        } slave;
    };
};



static struct I2CDriver _I2C_drivers[I2CHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



static useret enum I2CMasterError
I2C_blocking_transfer
(
    enum I2CHandle      handle,
    u32                 address,      // @/`I2C Slave Address`.
    enum I2CAddressType address_type, // "
    enum I2COperation   operation,
    u8*                 pointer,
    i32                 amount
)
{

    _EXPAND_HANDLE



    // Validation.

    if (I2Cx_DRIVER_ROLE != I2CDriverRole_master)
        panic;

    if (!pointer)
        panic;

    if (amount <= 0)
        panic;

    switch (address_type)
    {
        case I2CAddressType_seven:
        {
            if (!(0b0000'1000 <= address && address <= 0b0111'0111))
                panic;
        } break;

        case I2CAddressType_ten:
        {
            if (address >= (1 << 10))
                panic;
        } break;

        default: panic;
    }



    // Schedule the transfer.

    switch (driver->master.state)
    {
        case I2CMasterState_standby:
        {

            driver->master.address      = address;
            driver->master.address_type = address_type;
            driver->master.operation    = operation;
            driver->master.pointer      = pointer;
            driver->master.amount       = amount;
            driver->master.progress     = 0;
            driver->master.error        = I2CMasterError_none;
            __DMB();
            driver->master.state        = I2CMasterState_scheduled_transfer;

            NVIC_SET_PENDING(I2Cx_EV);

        } break;

        case I2CMasterState_scheduled_transfer : panic;
        case I2CMasterState_transferring       : panic;
        case I2CMasterState_stopping           : panic;
        default                                : panic;
    }



    // Wait until the transfer is done.

    while (true)
    {
        switch (driver->master.state)
        {

            // The driver just finished!

            case I2CMasterState_standby:
            {
                return driver->master.error;
            } break;



            // The driver is still busy with our transfer.

            case I2CMasterState_scheduled_transfer:
            case I2CMasterState_transferring:
            case I2CMasterState_stopping:
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



    // Set the kernel clock source for the peripheral.

    CMSIS_PUT(I2Cx_KERNEL_SOURCE, STPY_I2Cx_KERNEL_SOURCE);



    // Enable the peripheral.

    CMSIS_PUT(I2Cx_ENABLE, true);



    // Configure the peripheral.

    switch (I2Cx_DRIVER_ROLE)
    {



        // The I2C peripheral will work as a controller.
        // @/pg 2091/sec 48.4.9/`H533rm`.

        case I2CDriverRole_master:
        {

            CMSIS_SET
            (
                I2Cx  , TIMINGR        ,
                PRESC , STPY_I2Cx_PRESC, // Set the time base unit.
                SCLH  , STPY_I2Cx_SCLH , // Determines the amount of high time.
                SCLL  , STPY_I2Cx_SCLL , // Determines the amount of low time.
            );

        } break;



        // The I2C peripheral will work as a target.
        // @/pg 2082/sec 48.4.8/`H533rm`.

        case I2CDriverRole_slave:
        {

            CMSIS_SET
            (
                I2Cx   , OAR1                   ,
                OA1EN  , true                   , // Enable the address.
                OA1MODE, false                  , // We currently only support I2C
                OA1    , I2Cx_SLAVE_ADDRESS << 1, // driver slave addresses of 7-bit.
            );

        } break;



        default: panic;

    }



    // Select the interrupt events.

    CMSIS_SET
    (
        I2Cx  , CR1   , // Interrupts for:
        ERRIE , true  , //     - Error.
        TCIE  , true  , //     - Transfer completed.
        STOPIE, true  , //     - STOP signal.
        NACKIE, true  , //     - NACK signal.
        ADDRIE, true  , //     - Address match.
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
        I2CInterruptEvent_address_match,
    };
    enum I2CInterruptEvent interrupt_event  = {0};
    u32                    interrupt_status = I2Cx->ISR;



    // TODO Handle gracefully.
    // @/pg 2116/sec 48.4.17/`H533rm`.

    if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, BERR))
    {
        sorry
    }



    // TODO Handle gracefully.
    // @/pg 2116/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, ARLO))
    {
        sorry
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



    // @/pg 2088/fig 639/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, ADDR))
    {
        interrupt_event = I2CInterruptEvent_address_match;
    }



    // Nothing notable happened.

    else
    {
        interrupt_event = I2CInterruptEvent_none;
    }



    // Handle the interrupt event.

    switch (I2Cx_DRIVER_ROLE)
    {

        ////////////////////////////////////////////////////////////////////////////////
        //
        // Master initiates all data transfers.
        //

        case I2CDriverRole_master:
        {

            switch (driver->master.state)
            {

                // Nothing to do until the user schedules a transfer.

                case I2CMasterState_standby:
                {

                    if (interrupt_event)
                        panic;

                    return I2CUpdateOnce_yield;

                } break;



                // The user has a transfer they want to do.

                case I2CMasterState_scheduled_transfer:
                {

                    if (interrupt_event)
                        panic;

                    if (!(1 <= driver->master.amount && driver->master.amount <= 255))
                        panic;

                    if (CMSIS_GET(I2Cx, CR2, START))
                        panic;

                    if (driver->master.error)
                        panic;

                    u32 sadd = {0};

                    switch (driver->master.address_type)
                    {
                        case I2CAddressType_seven:
                        {
                            sadd = driver->master.address << 1; // @/`I2C Slave Address`.
                        } break;

                        case I2CAddressType_ten:
                        {
                            sadd = driver->master.address;
                        } break;

                        default: panic;
                    }

                    CMSIS_SET
                    (
                        I2Cx   , CR2                          ,
                        SADD   , sadd                         ,
                        RD_WRN , !!driver->master.operation   ,
                        NBYTES , driver->master.amount        ,
                        START  , true                         ,
                        ADD10  , !!driver->master.address_type,
                    );

                    driver->master.state = I2CMasterState_transferring;

                    return I2CUpdateOnce_again;

                } break;



                // We're in the process of doing the transfer.

                case I2CMasterState_transferring: switch (interrupt_event)
                {

                    // Nothing to do until there's an interrupt status.

                    case I2CInterruptEvent_none:
                    {

                        if (driver->master.error)
                            panic;

                        return I2CUpdateOnce_yield;

                    } break;



                    // The slave didn't acknowledge us.

                    case I2CInterruptEvent_nack_signaled:
                    {

                        if (driver->master.error)
                            panic;

                        CMSIS_SET(I2Cx, ICR, NACKCF, true);

                        // Begin sending out the STOP signal.
                        CMSIS_SET(I2Cx, CR2, STOP, true);

                        driver->master.state = I2CMasterState_stopping;
                        driver->master.error = I2CMasterError_no_acknowledge;

                        return I2CUpdateOnce_again;

                    } break;



                    // The slave sent us some data.

                    case I2CInterruptEvent_data_available_to_read:
                    {

                        if (driver->master.error)
                            panic;

                        if (!(0 <= driver->master.progress && driver->master.progress < driver->master.amount))
                            panic;

                        // Pop from the RX-buffer.
                        u8 data = CMSIS_GET(I2Cx, RXDR, RXDATA);

                        driver->master.pointer[driver->master.progress]  = data;
                        driver->master.progress                         += 1;

                        return I2CUpdateOnce_again;

                    } break;



                    // We're ready to send some data to the slave.

                    case I2CInterruptEvent_ready_to_transmit_data:
                    {

                        if (driver->master.error)
                            panic;

                        if (!(0 <= driver->master.progress && driver->master.progress < driver->master.amount))
                            panic;

                        u8 data = driver->master.pointer[driver->master.progress];

                        // Push data into the TX-buffer.
                        CMSIS_SET(I2Cx, TXDR, TXDATA, data);

                        driver->master.progress += 1;

                        return I2CUpdateOnce_again;

                    } break;



                    // We finished transferring all the data.

                    case I2CInterruptEvent_transfer_completed:
                    {

                        if (driver->master.error)
                            panic;

                        if (!iff(driver->master.progress == driver->master.amount, !driver->master.error))
                            panic;

                        // Begin sending out the STOP signal.
                        CMSIS_SET(I2Cx, CR2, STOP, true);

                        driver->master.state = I2CMasterState_stopping;

                        return I2CUpdateOnce_again;

                    } break;



                    case I2CInterruptEvent_stop_signaled : panic;
                    case I2CInterruptEvent_address_match : panic;
                    default                              : panic;

                } break;



                // We're in the process of stopping the transfer.

                case I2CMasterState_stopping: switch (interrupt_event)
                {

                    // Nothing to do until there's an interrupt status.

                    case I2CInterruptEvent_none:
                    {
                        return I2CUpdateOnce_yield;
                    } break;



                    // We're finally done with the transfer!

                    case I2CInterruptEvent_stop_signaled:
                    {

                        if (!iff(driver->master.progress == driver->master.amount, !driver->master.error))
                            panic;

                        CMSIS_SET(I2Cx, ICR, STOPCF, true);

                        driver->master.state = I2CMasterState_standby;

                        return I2CUpdateOnce_again;

                    } break;

                    case I2CInterruptEvent_transfer_completed     : panic;
                    case I2CInterruptEvent_data_available_to_read : panic;
                    case I2CInterruptEvent_ready_to_transmit_data : panic;
                    case I2CInterruptEvent_nack_signaled          : panic;
                    case I2CInterruptEvent_address_match          : panic;
                    default                                       : panic;

                } break;



                default: panic;

            }

        } break;



        ////////////////////////////////////////////////////////////////////////////////
        //
        // Slave must wait for all data transfers.
        //

        case I2CDriverRole_slave:
        {

            switch (interrupt_event)
            {

                // Nothing to do until there's an interrupt status.

                case I2CInterruptEvent_none:
                {
                    return I2CUpdateOnce_yield;
                } break;



                // The slave I2C driver just got called on.

                case I2CInterruptEvent_address_match:
                {

                    if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, ADDCODE) != I2Cx_SLAVE_ADDRESS)
                        // Address must match with what we were configured with.
                        panic;

                    if (!CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, TXE))
                        // There shouldn't be anything still in the TX-buffer.
                        panic;

                    if (driver->slave.state != I2CSlaveState_standby)
                        // Unexpected interrupt event given the I2C driver state.
                        panic;



                    // Acknowledge the interrupt.

                    CMSIS_SET(I2Cx, ICR, ADDRCF, true);



                    // Determine the data transfer direction.

                    b32 master_wants_to_read = CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, DIR);

                    if (master_wants_to_read)
                    {
                        driver->slave.state = I2CSlaveState_sending_data;
                    }
                    else
                    {
                        driver->slave.state = I2CSlaveState_receiving_data;
                    }

                    return I2CUpdateOnce_again;

                } break;



                // The master wants data.

                case I2CInterruptEvent_ready_to_transmit_data:
                {

                    if (driver->slave.state != I2CSlaveState_sending_data)
                        // Unexpected interrupt event given the I2C driver state.
                        panic;



                    // Dummy value when the user has no more data to send.

                    u8 data = 0xFF;



                    // Get the next byte that the user wants to send.

                    INTERRUPT_i2c_slave_callback(handle, I2CSlaveEvent_ready_to_transmit_data, &data);



                    // Push into the TX-buffer.

                    CMSIS_SET(I2Cx, TXDR, TXDATA, data);

                    return I2CUpdateOnce_again;

                } break;



                // The master sent us data.

                case I2CInterruptEvent_data_available_to_read:
                {

                    if (driver->slave.state != I2CSlaveState_receiving_data)
                        // Unexpected interrupt event given the I2C driver state.
                        panic;



                    // Pop from the RX-buffer.

                    u8 data = CMSIS_GET(I2Cx, RXDR, RXDATA);



                    // Let the user handle the data.

                    INTERRUPT_i2c_slave_callback(handle, I2CSlaveEvent_data_available_to_read, &data);

                    return I2CUpdateOnce_again;

                } break;



                // The master can NACK on the data we just sent them to
                // indicate an error, but most of the time, this is just
                // happens whenever the master received the last byte, so
                // this will be the only situation we should be expecting it.

                case I2CInterruptEvent_nack_signaled:
                {

                    if (driver->slave.state != I2CSlaveState_sending_data)
                        // Unexpected interrupt event given the I2C driver state.
                        panic;



                    // Acknowledge the interrupt event.

                    CMSIS_SET(I2Cx, ICR, NACKCF, true);



                    // We should be expecting the stop signal next.

                    driver->slave.state = I2CSlaveState_stopping;

                    return I2CUpdateOnce_again;

                } break;



                // We've reached the end of the transfer.
                // TODO I don't think this will happen if there's a repeated start;
                //      we should test it.

                case I2CInterruptEvent_stop_signaled:
                {

                    if
                    (
                        driver->slave.state != I2CSlaveState_receiving_data &&
                        driver->slave.state != I2CSlaveState_stopping
                    )
                        // Unexpected interrupt event given the I2C driver state.
                        panic;



                    // Acknowledge the interrupt event.

                    CMSIS_SET(I2Cx, ICR, STOPCF, true);



                    // Let the user know that the transfer ended.

                    INTERRUPT_i2c_slave_callback(handle, I2CSlaveEvent_stop_signaled, nullptr);



                    // Flush the transmit buffer so any left-over data
                    // won't accidentally be transmitted on the next transfer.

                    CMSIS_SET(I2Cx, ISR, TXE, true);



                    // Ready for the next transfer.

                    driver->slave.state = I2CSlaveState_standby;

                    return I2CUpdateOnce_again;

                } break;



                case I2CInterruptEvent_transfer_completed : panic;
                default                                   : panic;

            }

        } break;



        default: panic;

    }

}



static void
_I2C_driver_interrupt(enum I2CHandle handle)
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



// @/`I2C Slave Address`:
//
// There's a bit of ambiguity on whether or not an "I2C slave address" is
// using the 7-bit convention (as used by the I2C specification) or the 8-bit
// convention where the LSb is reserved for the read/write bit.
//
// This driver uses the 7-bit convention because it's simpler to understand
// that way; however, the underlying I2C hardware uses the 8-bit convention.
//
// To add another wrinkle, some of the 7-bit address space is reserved or have
// a special meaning (e.g. general call).
//
// When using 10-bit addressing, it's all much simpler;
// the entire address space is solely for slave devices.
//
// @/pg 16/tbl 4/`I2C`.
