enum I2CDriverMode : u32 // @/`I2C Driver Modes`.
{
    I2CDriverMode_master_blocking,
    I2CDriverMode_master_callback,
    I2CDriverMode_slave,
};



enum I2CMasterCallbackEvent : u32
{
    I2CMasterCallbackEvent_can_schedule_next_transfer,
    I2CMasterCallbackEvent_transfer_successful,
    I2CMasterCallbackEvent_transfer_unacknowledged,
    I2CMasterCallbackEvent_bug,
};

typedef void I2CMasterCallback(enum I2CMasterCallbackEvent event);



enum I2CSlaveCallbackEvent : u32
{
    I2CSlaveCallbackEvent_data_available_to_read,
    I2CSlaveCallbackEvent_ready_to_transmit_data,
    I2CSlaveCallbackEvent_stop_signaled,
    I2CSlaveCallbackEvent_bug,
};

typedef void I2CSlaveCallback(enum I2CSlaveCallbackEvent event, u8* data);



union I2CCallback
{
    void*              function;
    I2CMasterCallback* master;
    I2CSlaveCallback*  slave;
};



#include "i2c_driver_support.meta"
/* #meta



    # Forward-declare any callbacks that a driver might use.

    for target in PER_TARGET():

        for driver in target.drivers:

            if driver['type'] != 'I2C':
                continue


            match driver['mode']:



                case 'master_callback':

                    Meta.line(f'''
                        static I2CMasterCallback INTERRUPT_I2Cx_{driver['handle']};
                    ''')

                    Meta.define(
                        f'INTERRUPT_I2Cx_{driver['handle']}',
                        ('...'),
                        f'static void INTERRUPT_I2Cx_{driver['handle']}(__VA_ARGS__)'
                    )



                case 'slave':

                    Meta.line(f'''
                        static I2CSlaveCallback INTERRUPT_I2Cx_{driver['handle']};
                    ''')

                    Meta.define(
                        f'INTERRUPT_I2Cx_{driver['handle']}',
                        ('...'),
                        f'static void INTERRUPT_I2Cx_{driver['handle']}(__VA_ARGS__)'
                    )



    IMPLEMENT_DRIVER_SUPPORT(
        driver_type = 'I2C',
        cmsis_name  = 'I2C',
        common_name = 'I2Cx',
        expansions  = (('driver', '&_I2C_drivers[handle]'),),
        terms       = lambda type, peripheral, handle, mode, address = 0: (
            ('{}_DRIVER_MODE'       , 'expression' , f'I2CDriverMode_{mode}'    ),
            ('{}_SLAVE_ADDRESS'     , 'expression' , f'((u16) 0x{address :03X})'),
            ('{}'                   , 'expression' ,                            ),
            ('NVICInterrupt_{}_EV'  , 'expression' ,                            ),
            ('NVICInterrupt_{}_ER'  , 'expression' ,                            ),
            ('STPY_{}_KERNEL_SOURCE', 'expression' ,                            ),
            ('STPY_{}_PRESC'        , 'expression' ,                            ),
            ('STPY_{}_SCLH'         , 'expression' ,                            ),
            ('STPY_{}_SCLL'         , 'expression' ,                            ),
            ('{}_RESET'             , 'cmsis_tuple',                            ),
            ('{}_ENABLE'            , 'cmsis_tuple',                            ),
            ('{}_KERNEL_SOURCE'     , 'cmsis_tuple',                            ),
            ('{}_EV'                , 'interrupt'  ,                            ),
            ('{}_ER'                , 'interrupt'  ,                            ),
            ('{}_CALLBACK'          , 'expression' ,
                f'(union I2CCallback) {{ {
                    f'&INTERRUPT_I2Cx_{handle}'
                    if mode in ('master_callback', 'slave') else
                    'nullptr'
                } }}'
            ),
        ),
    )



    for target in PER_TARGET():

        slave_drivers = [
            driver
            for driver in target.drivers
            if driver['type'] == 'I2C'
            if driver['mode'] == 'slave'
        ]



        # Make sure slave addresses are valid.

        for driver in slave_drivers:

            if 0b0000_1000 <= driver['address'] <= 0b0111_0111:
                continue

            raise ValueError(
                f'Target {repr(target.name)} has '
                f'I2C peripheral {repr(driver['handle'])} '
                f'with invalid 7-bit address of {repr(driver['address'])}.'
            )

*/



enum I2CMasterState : u32
{
    I2CMasterState_standby = 0,
    I2CMasterState_scheduled_transfer,
    I2CMasterState_transferring,
    I2CMasterState_stopping,
    I2CMasterState_bug,
};

enum I2CSlaveState : u32
{
    I2CSlaveState_standby,
    I2CSlaveState_receiving_data,
    I2CSlaveState_sending_data,
    I2CSlaveState_stopping,
    I2CSlaveState_bug,
};

enum I2CMasterError : u32
{
    I2CMasterError_none,
    I2CMasterError_no_acknowledge,
};

enum I2CAddressType : u32 // @/`I2C Slave Address`.
{
    I2CAddressType_seven = false,
    I2CAddressType_ten   = true,
};

enum I2COperation : u32
{
    I2COperation_single_write,
    I2COperation_single_read,
    I2COperation_repeating_write,
    I2COperation_repeating_read,
};

struct I2CDriver
{
    union
    {
        struct I2CDriverMaster
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

        struct I2CDriverSlave
        {
            enum I2CSlaveState state;
        } slave;
    };
};

static struct I2CDriver _I2C_drivers[I2CHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



static useret enum I2CTransferResult : u32
{
    I2CTransferResult_transfer_done,
    I2CTransferResult_transfer_ongoing,
    I2CTransferResult_no_acknowledge,
    I2CTransferResult_bug,
}
I2C_transfer
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



    // Only masters can do I2C transfers.

    switch (I2Cx_DRIVER_MODE)
    {
        case I2CDriverMode_master_blocking : break;
        case I2CDriverMode_master_callback : break;
        case I2CDriverMode_slave           : return I2CTransferResult_bug;
        default                            : return I2CTransferResult_bug;
    }



    // The source/destination must be provided.
    // We have no meaning for null-pointers as of now.

    if (!pointer)
        return I2CTransferResult_bug;



    // Catch bad math.

    if (amount <= 0)
        return I2CTransferResult_bug;



    // Ensure I2C has been initialized.

    if (!CMSIS_GET(I2Cx, CR1, PE))
        return I2CTransferResult_bug;



    // There shouldn't be any transfers on the bus of right now.

    if (CMSIS_GET(I2Cx, ISR, BUSY))
        return I2CTransferResult_bug;



    // Validate I2C slave address.

    switch (address_type)
    {

        case I2CAddressType_seven:
        {

            if (!(0b0000'1000 <= address && address <= 0b0111'0111))
                return I2CTransferResult_bug;

        } break;

        case I2CAddressType_ten:
        {

            if (address >= (1 << 10))
                return I2CTransferResult_bug;

        } break;

        default: return I2CTransferResult_bug;

    }



    // Schedule the transfer.

    switch (driver->master.state)
    {

        // The I2C driver can take our transfer request now.

        case I2CMasterState_standby:
        {

            // Prepare the desired read/write transfer for the I2C driver.

            driver->master =
                (struct I2CDriverMaster)
                {
                    .state        = I2CMasterState_standby,
                    .address      = address,
                    .address_type = address_type,
                    .operation    = operation,
                    .pointer      = pointer,
                    .amount       = amount,
                };



            // Memory release; the I2C driver can now use the above information.

            driver->master.state = I2CMasterState_scheduled_transfer;

            NVIC_SET_PENDING(I2Cx_EV);

        } break;



        // The I2C driver isn't ready to take
        // our read/write transfer right now.
        //
        // This shouldn't be possible when using `master_blocking`.
        // For `master_callback`, the callback function should not
        // be trying to schedule I2C transfers until it knows that
        // the next transfer can be queued up.

        case I2CMasterState_scheduled_transfer : return I2CTransferResult_bug;
        case I2CMasterState_transferring       : return I2CTransferResult_bug;
        case I2CMasterState_stopping           : return I2CTransferResult_bug;
        case I2CMasterState_bug                : return I2CTransferResult_bug;
        default                                : return I2CTransferResult_bug;

    }



    // Things to do after we scheduled the read/write transfer...

    switch (I2Cx_DRIVER_MODE)
    {

        // For `master_blocking`, we simplify the control-flow
        // for the user by guaranteeing them that the read
        // transfer is done by the time the function returns
        // (and thus the slave data is available to be used),
        // or for a write transfer, that the slave has received
        // all of the data by the time the function returns.

        case I2CDriverMode_master_blocking: while (true) switch (driver->master.state)
        {

            // The driver just finished!

            case I2CMasterState_standby: switch (driver->master.error)
            {
                case I2CMasterError_none           : return I2CTransferResult_transfer_done;
                case I2CMasterError_no_acknowledge : return I2CTransferResult_no_acknowledge;
                default                            : return I2CTransferResult_bug;
            } break;



            // The driver is still busy with our transfer.

            case I2CMasterState_scheduled_transfer:
            case I2CMasterState_transferring:
            case I2CMasterState_stopping:
            {
                // Keep waiting...
            } break;



            case I2CMasterState_bug : return I2CTransferResult_bug;
            default                 : return I2CTransferResult_bug;

        } break;



        // For `master_callback`, we let the I2C driver handle
        // the transfer in the background. The user-defined
        // callback will be notified of the next thing to do
        // (e.g. transfer done, error encountered, etc).

        case I2CDriverMode_master_callback:
        {
            return I2CTransferResult_transfer_ongoing;
        } break;



        case I2CDriverMode_slave : return I2CTransferResult_bug;
        default                  : return I2CTransferResult_bug;

    }

}



////////////////////////////////////////////////////////////////////////////////



static enum I2CReinitResult : u32
{
    I2CReinitResult_success,
    I2CReinitResult_bug,
}
I2C_reinit(enum I2CHandle handle)
{

    _EXPAND_HANDLE



    // Reset-cycle the peripheral.

    CMSIS_PUT(I2Cx_RESET, true );
    CMSIS_PUT(I2Cx_RESET, false);

    *driver = (struct I2CDriver) {0};



    // Enable the NVIC interrupts.

    NVIC_ENABLE(I2Cx_EV);
    NVIC_ENABLE(I2Cx_ER);



    // Set the kernel clock source for the peripheral.

    CMSIS_PUT(I2Cx_KERNEL_SOURCE, STPY_I2Cx_KERNEL_SOURCE);



    // Clock the peripheral block.

    CMSIS_PUT(I2Cx_ENABLE, true);



    // Configure the peripheral.

    switch (I2Cx_DRIVER_MODE)
    {



        // The I2C peripheral will work as a controller.

        case I2CDriverMode_master_blocking:
        case I2CDriverMode_master_callback:
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

        case I2CDriverMode_slave:
        {

            CMSIS_SET
            (
                I2Cx   , OAR1                   ,
                OA1EN  , true                   , // Enable the address.
                OA1    , I2Cx_SLAVE_ADDRESS << 1, // The slave address of the I2C driver.
                OA1MODE, false                  , // I2C driver currently only supports slave addresses of 7-bit.
            );

        } break;



        default: return I2CReinitResult_bug;

    }

    CMSIS_SET
    (
        I2Cx  , CR1 , // Interrupts for:
        ERRIE , true, //     - Error.
        TCIE  , true, //     - Transfer completed.
        STOPIE, true, //     - STOP signal.
        NACKIE, true, //     - NACK signal.
        ADDRIE, true, //     - Address match.
        RXIE  , true, //     - Reception of data.
        TXIE  , true, //     - Transmission of data.
        DNF   , 15  , // Max out the digital filtering.
        PE    , true, // Enable the peripheral.
    );



    // After initialization, the user callback function
    // will be invoked automatically so they know that
    // the first I2C transfer can be scheduled.

    if (I2Cx_DRIVER_MODE == I2CDriverMode_master_callback)
    {
        NVIC_SET_PENDING(I2Cx_EV);
    }



    return I2CReinitResult_success;

}



////////////////////////////////////////////////////////////////////////////////



static useret enum I2CUpdateOnceResult : u32
{
    I2CUpdateOnceResult_again,
    I2CUpdateOnceResult_yield,
    I2CUpdateOnceResult_bus_misbehaved,
    I2CUpdateOnceResult_bug,
}
_I2C_update_once(enum I2CHandle handle)
{

    _EXPAND_HANDLE



    enum I2CInterruptEvent : u32
    {
        I2CInterruptEvent_none,
        I2CInterruptEvent_nack_signaled,
        I2CInterruptEvent_stop_signaled,
        I2CInterruptEvent_transfer_completed_successfully,
        I2CInterruptEvent_data_available_to_read,
        I2CInterruptEvent_ready_to_transmit_data,
        I2CInterruptEvent_address_match,
    };
    enum I2CInterruptEvent interrupt_event  = {0};
    u32                    interrupt_status = I2Cx->ISR;
    u32                    interrupt_enable = I2Cx->CR1;



    // "A bus error is detected when a
    // START or a STOP condition is detected
    // and is not located after a multiple of
    // nine SCL clock pulses."
    //
    // @/pg 2116/sec 48.4.17/`H533rm`.

    if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, BERR))
    {
        return I2CUpdateOnceResult_bus_misbehaved;
    }



    // "An arbitration loss is detected when a high level
    // is sent on the SDA line, but a low level is sampled
    // on the SCL rising edge."
    //
    // @/pg 2116/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, ARLO))
    {
        return I2CUpdateOnceResult_bus_misbehaved;
    }



    // I2C clock line has been stretched for too long.
    //
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, TIMEOUT))
    {
        return I2CUpdateOnceResult_bug; // Shouldn't happen; this feature isn't used.
    }



    // An SMBALERT signal was detected.
    //
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, ALERT))
    {
        return I2CUpdateOnceResult_bug; // Shouldn't happen; this feature isn't used.
    }



    // Packet error checking detected corruption.
    //
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, PECERR))
    {
        return I2CUpdateOnceResult_bug; // Shouldn't happen; this feature isn't used.
    }



    // Transfer ready to be reloaded.
    //
    // @/pg 2095/sec 48.4.9/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, TCR))
    {
        return I2CUpdateOnceResult_bug; // Shouldn't happen; this feature isn't used.
    }



    // The RX-register got overwritten or no data was in the TX-register for transmission.
    //
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, OVR))
    {
        return I2CUpdateOnceResult_bug; // Shouldn't happen; clock-stretching is enabled by default.
    }



    // The slave didn't acknowledge.
    //
    // @/pg 2085/sec 48.4.8/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, NACKF))
    {
        CMSIS_SET(I2Cx, ICR, NACKCF, true);
        interrupt_event = I2CInterruptEvent_nack_signaled;
    }



    // A STOP condition was successfully sent.
    //
    // @/pg 2085/sec 48.4.8/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, STOPF))
    {
        CMSIS_SET(I2Cx, ISR, TXE   , true); // @/`Flushing I2C Transmission Data`.
        CMSIS_SET(I2Cx, ICR, STOPCF, true);
        interrupt_event = I2CInterruptEvent_stop_signaled;
    }



    // The I2C transfer was just finished.
    //
    // @/pg 2095/sec 48.4.9/`H533rm`.

    else if
    (
        CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, TC  ) &&
        CMSIS_GET_FROM(interrupt_enable, I2Cx, CR1, TCIE) // @/`I2C Transfer-Complete Interrupt and Repeated Starts`.
    )
    {
        interrupt_event = I2CInterruptEvent_transfer_completed_successfully;
    }



    // There's data from the slave in the RX-register.
    //
    // @/pg 2089/sec 48.4.8/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, RXNE))
    {
        interrupt_event = I2CInterruptEvent_data_available_to_read;
    }



    // Data for the slave needs to be put in the TX-register.
    //
    // @/pg 2085/sec 48.4.8/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, TXIS))
    {
        interrupt_event = I2CInterruptEvent_ready_to_transmit_data;
    }



    // Slave acknowledged our transfer request.
    //
    // @/pg 2088/fig 639/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, ADDR))
    {
        CMSIS_SET(I2Cx, ICR, ADDRCF, true);
        interrupt_event = I2CInterruptEvent_address_match;
    }



    // Nothing notable happened.

    else
    {
        interrupt_event = I2CInterruptEvent_none;
    }



    // Handle the interrupt event.

    switch (I2Cx_DRIVER_MODE)
    {



        case I2CDriverMode_master_blocking:
        case I2CDriverMode_master_callback: switch (driver->master.state)
        {



            // Nothing to do until the user schedules a transfer.

            case I2CMasterState_standby:
            {

                if (interrupt_event)
                    return I2CUpdateOnceResult_bug; // We shouldn't have unhandled interrupt events...

                if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, BUSY))
                    return I2CUpdateOnceResult_bug; // There shouldn't be any transfers on the bus of right now.



                if (I2Cx_DRIVER_MODE == I2CDriverMode_master_callback)
                {

                    I2Cx_CALLBACK.master(I2CMasterCallbackEvent_can_schedule_next_transfer);

                    // If the user's callback scheduled the next transfer,
                    // the I2C interrupt routine would be set pending, so
                    // it's okay to yield here afterwards.

                }

                return I2CUpdateOnceResult_yield;

            } break;



            // The user has a transfer they want to do.

            case I2CMasterState_scheduled_transfer:
            {

                if (interrupt_event)
                    return I2CUpdateOnceResult_bug; // We shouldn't have unhandled interrupt events...

                if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, BUSY))
                    return I2CUpdateOnceResult_bug; // There shouldn't be any transfers on the bus of right now.

                if (!(1 <= driver->master.amount && driver->master.amount <= 255))
                    return I2CUpdateOnceResult_bug; // We currently don't handle transfer sizes larger than this.

                if (CMSIS_GET(I2Cx, CR2, START))
                    return I2CUpdateOnceResult_bug; // We shouldn't be already trying to start the transfer...

                if (driver->master.error)
                    return I2CUpdateOnceResult_bug; // Any errors should've been acknowledged before the next transfer is done.



                // Configure the desired transfer.

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

                    default: return I2CUpdateOnceResult_bug;
                }

                b32 read_operation =
                    driver->master.operation == I2COperation_single_read ||
                    driver->master.operation == I2COperation_repeating_read;

                CMSIS_SET
                (
                    I2Cx   , CR2                          ,
                    SADD   , sadd                         , // I2C slave to call for.
                    ADD10  , !!driver->master.address_type, // Whether or not a 10-bit slave address is used.
                    RD_WRN , !!read_operation             , // Determine data transfer direction.
                    NBYTES , driver->master.amount        , // Determine amount of data in bytes.
                    START  , true                         , // Begin sending the START condition on the I2C bus.
                );

                CMSIS_SET(I2Cx, CR1, TCIE, true); // @/`I2C Transfer-Complete Interrupt and Repeated Starts`.



                // The I2C peripheral should be now trying to call
                // the slave device and get an acknowledgement back.

                driver->master.state = I2CMasterState_transferring;

                return I2CUpdateOnceResult_again;

            } break;



            // We're in the process of doing the transfer.

            case I2CMasterState_transferring: switch (interrupt_event)
            {



                // Nothing to do until there's an interrupt event.

                case I2CInterruptEvent_none:
                {

                    if (driver->master.error)
                        return I2CUpdateOnceResult_bug; // There's no reason to get an error right now...

                    return I2CUpdateOnceResult_yield;

                } break;



                // The slave didn't acknowledge us.

                case I2CInterruptEvent_nack_signaled:
                {

                    if (driver->master.error)
                        return I2CUpdateOnceResult_bug; // There shouldn't have been any other errors before this.



                    // Begin sending out the STOP signal.

                    CMSIS_SET(I2Cx, CR2, STOP, true);



                    // We now wait for the STOP condition on
                    // the bus to truly end the transfer.

                    driver->master.state = I2CMasterState_stopping;
                    driver->master.error = I2CMasterError_no_acknowledge;

                    return I2CUpdateOnceResult_again;

                } break;



                // The slave sent us some data.

                case I2CInterruptEvent_data_available_to_read:
                {

                    if (driver->master.error)
                        return I2CUpdateOnceResult_bug; // There shouldn't have been any other errors before this.

                    if (!(0 <= driver->master.progress && driver->master.progress < driver->master.amount))
                        return I2CUpdateOnceResult_bug; // Hardware is giving us an unexpected amount of data...



                    // Pop the byte from the RX-register and push it into the destination.

                    u8 data = CMSIS_GET(I2Cx, RXDR, RXDATA);

                    driver->master.pointer[driver->master.progress]  = data;
                    driver->master.progress                         += 1;

                    return I2CUpdateOnceResult_again;

                } break;



                // We're ready to send some data to the slave.

                case I2CInterruptEvent_ready_to_transmit_data:
                {

                    if (driver->master.error)
                        return I2CUpdateOnceResult_bug; // There shouldn't have been any other errors before this.

                    if (!(0 <= driver->master.progress && driver->master.progress < driver->master.amount))
                        return I2CUpdateOnceResult_bug; // Hardware is demanding us an unexpected amount of data...


                    // Pop a byte from the source and push it into the TX-register.

                    u8 data = driver->master.pointer[driver->master.progress];

                    CMSIS_SET(I2Cx, TXDR, TXDATA, data);

                    driver->master.progress += 1;

                    return I2CUpdateOnceResult_again;

                } break;



                // We finished transferring all the data.

                case I2CInterruptEvent_transfer_completed_successfully:
                {

                    if (driver->master.error)
                        return I2CUpdateOnceResult_bug; // But the hardware said that the transfer was completed successfully!

                    if (driver->master.progress != driver->master.amount)
                        return I2CUpdateOnceResult_bug; // Hardware says we finished the transfer, but software disagrees...



                    // Determine what to do now that the
                    // I2C read/write transfer is done.

                    switch (driver->master.operation)
                    {



                        // The I2C bus can be released.

                        case I2COperation_single_read:
                        case I2COperation_single_write:
                        {

                            // Try sending the STOP condition onto
                            // the bus and we'll wait for it.

                            CMSIS_SET(I2Cx, CR2, STOP, true);

                            driver->master.state = I2CMasterState_stopping;

                            return I2CUpdateOnceResult_again;

                        } break;



                        // The I2C bus should still be controlled by us so
                        // we can start the next transfer again (repeated START).

                        case I2COperation_repeating_read:
                        case I2COperation_repeating_write:
                        {

                            // @/`I2C Transfer-Complete Interrupt and Repeated Starts`:
                            //
                            // We disable the transfer-complete interrupt here
                            // because it only gets cleared upon us scheduling a START
                            // or STOP condition. However, we don't know what the next
                            // transfer the user is going to do, so we can't send the
                            // repeated START condition until then. So to prevent the
                            // I2C interrupt from being perpetually triggered, we
                            // temporarily disable this interrupt event.

                            CMSIS_SET(I2Cx, CR1, TCIE, false);



                            // The I2C peripheral should be good to
                            // immediately start the next transfer.

                            driver->master.state = I2CMasterState_standby;



                            // We let the master callback know that the
                            // read/write transfer was carried out to completion.

                            if (I2Cx_DRIVER_MODE == I2CDriverMode_master_callback)
                            {

                                if (driver->master.error)
                                    return I2CUpdateOnceResult_bug; // But the hardware said that the transfer was completed successfully!

                                I2Cx_CALLBACK.master(I2CMasterCallbackEvent_transfer_successful);

                            }

                            return I2CUpdateOnceResult_again;

                        } break;



                        default: return I2CUpdateOnceResult_bug;

                    }

                } break;



                case I2CInterruptEvent_stop_signaled : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_address_match : return I2CUpdateOnceResult_bug;
                default                              : return I2CUpdateOnceResult_bug;

            } break;



            // We're in the process of stopping the transfer.

            case I2CMasterState_stopping: switch (interrupt_event)
            {



                // Nothing to do until there's an interrupt event.

                case I2CInterruptEvent_none:
                {
                    return I2CUpdateOnceResult_yield;
                } break;



                // We have sent the STOP condition successfully.
                // Whether or not the transfer itself was
                // actually successful is another thing.

                case I2CInterruptEvent_stop_signaled:
                {

                    if (!iff(driver->master.progress == driver->master.amount, !driver->master.error))
                        return I2CUpdateOnceResult_bug; // Error, but we transferred all expected data?

                    if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, BUSY))
                        return I2CUpdateOnceResult_bug; // The bus should be no longer busy now.



                    // The I2C peripheral should be good to
                    // immediately start the next transfer.

                    driver->master.state = I2CMasterState_standby;



                    // Let the master callback know the results of the transfer.

                    if (I2Cx_DRIVER_MODE == I2CDriverMode_master_callback)
                    {

                        enum I2CMasterCallbackEvent callback_event = {0};

                        switch (driver->master.error)
                        {
                            case I2CMasterError_none           : callback_event = I2CMasterCallbackEvent_transfer_successful;     break;
                            case I2CMasterError_no_acknowledge : callback_event = I2CMasterCallbackEvent_transfer_unacknowledged; break;
                            default                            : return I2CUpdateOnceResult_bug;
                        }

                        I2Cx_CALLBACK.master(callback_event);

                    }



                    return I2CUpdateOnceResult_again;

                } break;



                case I2CInterruptEvent_transfer_completed_successfully : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_data_available_to_read          : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_ready_to_transmit_data          : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_nack_signaled                   : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_address_match                   : return I2CUpdateOnceResult_bug;
                default                                                : return I2CUpdateOnceResult_bug;

            } break;



            case I2CMasterState_bug : return I2CUpdateOnceResult_bug;
            default                 : return I2CUpdateOnceResult_bug;

        } break;



        case I2CDriverMode_slave: switch (driver->slave.state)
        {



            // The I2C peripheral isn't currently engaged with any transfers yet.

            case I2CSlaveState_standby: switch (interrupt_event)
            {



                // Nothing to do until there's an interrupt event.

                case I2CInterruptEvent_none:
                {
                    return I2CUpdateOnceResult_yield;
                } break;



                // The I2C peripheral just got called on by a master.

                case I2CInterruptEvent_address_match:
                {

                    if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, ADDCODE) != I2Cx_SLAVE_ADDRESS)
                        return I2CUpdateOnceResult_bug; // Address must match with what we were configured with.

                    if (!CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, TXE))
                        return I2CUpdateOnceResult_bug; // There shouldn't be anything still in the TX-register.



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



                    return I2CUpdateOnceResult_again;

                } break;



                case I2CInterruptEvent_nack_signaled                   : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_stop_signaled                   : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_data_available_to_read          : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_ready_to_transmit_data          : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_transfer_completed_successfully : return I2CUpdateOnceResult_bug;
                default                                                : return I2CUpdateOnceResult_bug;

            } break;



            // We're in the process of getting data from the master.

            case I2CSlaveState_receiving_data: switch (interrupt_event)
            {



                // Nothing to do until there's an interrupt event.

                case I2CInterruptEvent_none:
                {
                    return I2CUpdateOnceResult_yield;
                } break;



                // We got data from the master!

                case I2CInterruptEvent_data_available_to_read:
                {

                    // Pop from the RX-register and let the user callback handle the data.

                    u8 data = CMSIS_GET(I2Cx, RXDR, RXDATA);

                    I2Cx_CALLBACK.slave(I2CSlaveCallbackEvent_data_available_to_read, &data);

                    return I2CUpdateOnceResult_again;

                } break;



                // The master wants to stop sending data now.

                case I2CInterruptEvent_stop_signaled:
                {

                    // The I2C peripheral should be ready to
                    // handle the next transfer from the master now.

                    driver->slave.state = I2CSlaveState_standby;



                    // Let the slave callback know that the transfer ended.

                    I2Cx_CALLBACK.slave(I2CSlaveCallbackEvent_stop_signaled, nullptr);

                    return I2CUpdateOnceResult_again;

                } break;



                case I2CInterruptEvent_nack_signaled                   : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_ready_to_transmit_data          : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_address_match                   : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_transfer_completed_successfully : return I2CUpdateOnceResult_bug;
                default                                                : return I2CUpdateOnceResult_bug;

            } break;



            // We're in the process of sending data to the master.

            case I2CSlaveState_sending_data: switch (interrupt_event)
            {



                // Nothing to do until there's an interrupt event.

                case I2CInterruptEvent_none:
                {
                    return I2CUpdateOnceResult_yield;
                } break;



                // The TX-register can be filled with the next few bytes to send to the master.

                case I2CInterruptEvent_ready_to_transmit_data:
                {

                    // Get the next byte that the user wants to send and push it into the TX-register.
                    // If we user callback does not provide us with the data, then we just
                    // have a dummy back-up value to send out. Perhaps a better approach here
                    // would be to have our slave I2C peripheral NACK the master when there's no
                    // more data to send, but this is sufficient for now.
                    //
                    // Note that this data byte is only queued up for transmission,
                    // not that it will necessarily be transmitted to the master.
                    // See @/`Flushing I2C Transmission Data`.

                    u8 data = 0xFF;

                    I2Cx_CALLBACK.slave(I2CSlaveCallbackEvent_ready_to_transmit_data, &data);

                    CMSIS_SET(I2Cx, TXDR, TXDATA, data);

                    return I2CUpdateOnceResult_again;

                } break;



                // The master NACKs us on the last byte we sent
                // them to indicate the end of the transfer.

                case I2CInterruptEvent_nack_signaled:
                {

                    // We should be expecting the STOP signal next.

                    driver->slave.state = I2CSlaveState_stopping;

                    return I2CUpdateOnceResult_again;

                } break;



                case I2CInterruptEvent_address_match                   : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_data_available_to_read          : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_stop_signaled                   : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_transfer_completed_successfully : return I2CUpdateOnceResult_bug;
                default                                                : return I2CUpdateOnceResult_bug;

            } break;



            // We're waiting for a STOP condition on the bus now...

            case I2CSlaveState_stopping: switch (interrupt_event)
            {



                // Nothing to do until there's an interrupt event.

                case I2CInterruptEvent_none:
                {
                    return I2CUpdateOnceResult_yield;
                } break;



                // We successfully detected a STOP condition on the I2C bus.

                case I2CInterruptEvent_stop_signaled:
                {

                    // The I2C peripheral should be ready to
                    // handle the next transfer from the master now.

                    driver->slave.state = I2CSlaveState_standby;



                    // Let the user know that the transfer has ended.

                    I2Cx_CALLBACK.slave(I2CSlaveCallbackEvent_stop_signaled, nullptr);

                    return I2CUpdateOnceResult_again;

                } break;



                case I2CInterruptEvent_nack_signaled                   : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_data_available_to_read          : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_ready_to_transmit_data          : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_address_match                   : return I2CUpdateOnceResult_bug;
                case I2CInterruptEvent_transfer_completed_successfully : return I2CUpdateOnceResult_bug;
                default                                                : return I2CUpdateOnceResult_bug;

            } break;



            case I2CSlaveState_bug : return I2CUpdateOnceResult_bug;
            default                : return I2CUpdateOnceResult_bug;

        } break;



        default: return I2CUpdateOnceResult_bug;

    }

}



static void
_I2C_driver_interrupt(enum I2CHandle handle)
{

    _EXPAND_HANDLE

    for (b32 yield = false; !yield;)
    {

        enum I2CUpdateOnceResult result = _I2C_update_once(handle);

        switch (result)
        {


            case I2CUpdateOnceResult_again:
            {
                // The state-machine should be updated again.
            } break;



            case I2CUpdateOnceResult_yield:
            {
                yield = true; // We can stop updating the state-machine for now.
            } break;



            case I2CUpdateOnceResult_bus_misbehaved:
            case I2CUpdateOnceResult_bug:
            default:
            {

                // Shut down the driver!

                CMSIS_PUT(I2Cx_RESET, true );
                CMSIS_PUT(I2Cx_RESET, false);

                NVIC_DISABLE(I2Cx_EV);
                NVIC_DISABLE(I2Cx_ER);

                *driver = (struct I2CDriver) {0};



                // Let the user know that the
                // I2C driver is in a bugged state
                // and that they should reinitialize it.

                switch (I2Cx_DRIVER_MODE)
                {

                    case I2CDriverMode_master_blocking:
                    {
                        driver->master.state = I2CMasterState_bug;
                    } break;

                    case I2CDriverMode_master_callback:
                    {

                        driver->master.state = I2CMasterState_bug;

                        I2Cx_CALLBACK.master(I2CMasterCallbackEvent_bug);

                    } break;

                    case I2CDriverMode_slave:
                    {

                        driver->slave.state = I2CSlaveState_bug;

                        I2Cx_CALLBACK.slave(I2CSlaveCallbackEvent_bug, nullptr);

                    } break;

                    default:
                    {
                        // Don't care; things are already shut down.
                    } break;

                }

                yield = true;

            } break;

        }

    }

}



////////////////////////////////////////////////////////////////////////////////



// @/`I2C Driver Modes`:
//
// There are several different modes that the I2C driver can take on:
//
//      - `master_blocking`
//          The I2C driver will act as a master and each
//          read/write transfer will be blocking. This
//          simplifies the control-flow, but is obviously
//          inefficient in terms of work being done.
//
//      - `master_callback`
//          The I2C driver will act as a master where a
//          read/write transfer will be queued up to be done.
//          The user will have to define the callback function
//          that'll be executed when a particular event has happen
//          (e.g. transfer completed, transfer error, etc).
//          The user will have to handle the more complicated
//          control-flow, but the processor will spend less time
//          busy-waiting for the completion of an I2C transfer.
//
//      - `slave`
//          The I2C driver will act as a slave where a
//          read/write transfer (invoked by the master) will
//          execute the user-defined callback function.
//          The user's callback function will handle the data
//          sent by the master, or prepare the next byte to be
//          sent to the master, or whatever else.
//
// It should be noted that the I2C interrupt routine is the only one
// that can invoke the callback functions to avoid re-entrance issues.



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



// @/`Flushing I2C Transmission Data`:
//
// For slave I2C peripherals, the TX-register will hold
// the data to be transmitted to the master when the master
// requests for it; however, a NACK from the master, followed
// by a STOP condition, can occur, to which a new transfer
// can happen. If we are not quick to respond to this, the
// slave could accidentally send stale data in the TX-register.
// To prevent this, we flush the TX-register before clearing the
// STOP condition flag.
//
// "If this data byte is not the one to send,
// the I2C_TXDR register can be flushed, by setting
// the TXE bit, to program a new data byte. The STOPF
// bit must be cleared only after these actions. This
// guarantees that they are executed before the first data
// transmission starts, following the address acknowledge."
// @/pg 2085/sec 48.4.8/`H533rm`.
//
// The flushing of the TX-register should be fine to do always,
// regardless whether or not the I2C peripheral is actually acting
// as a slave or a master.
