enum I2CDriverMode : u32 // @/`I2C Driver Modes`.
{
    I2CDriverMode_master,
    I2CDriverMode_slave,
};

enum I2CSlaveCallbackEvent : u32
{
    I2CSlaveCallbackEvent_data_available_to_read,
    I2CSlaveCallbackEvent_ready_to_transmit_data,
    I2CSlaveCallbackEvent_stop_signaled,
    I2CSlaveCallbackEvent_repeated_start_signaled,
    I2CSlaveCallbackEvent_clock_stretch_timeout,
    I2CSlaveCallbackEvent_bus_misbehaved,
    I2CSlaveCallbackEvent_bug,
};

typedef void I2CSlaveCallback(enum I2CSlaveCallbackEvent event, u8* data);

union I2CCallback
{
    void*              function;
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
        terms       = lambda target, type, peripheral, handle, mode, address = 0: (
            ('{}_DRIVER_MODE'           , 'expression' , f'I2CDriverMode_{mode}'    ),
            ('{}_SLAVE_ADDRESS'         , 'expression' , f'((u16) 0x{address :03X})'),
            ('{}'                       , 'expression' ,                            ),
            ('NVICInterrupt_{}_EV'      , 'expression' ,                            ),
            ('NVICInterrupt_{}_ER'      , 'expression' ,                            ),
            ('STPY_{}_KERNEL_SOURCE'    , 'expression' ,                            ),
            ('STPY_{}_PRESC'            , 'expression' ,                            ),
            ('STPY_{}_SCLH'             , 'expression' ,                            ),
            ('STPY_{}_SCLL'             , 'expression' ,                            ),
            ('STPY_{}_TIMEOUTR_TIMEOUTA', 'expression' ,                            ),
            ('{}_RESET'                 , 'cmsis_tuple',                            ),
            ('{}_ENABLE'                , 'cmsis_tuple',                            ),
            ('{}_KERNEL_SOURCE'         , 'cmsis_tuple',                            ),
            ('{}_EV'                    , 'interrupt'  ,                            ),
            ('{}_ER'                    , 'interrupt'  ,                            ),
            ('{}_CALLBACK'              , 'expression' ,
                f'(union I2CCallback) {{ {
                    f'&INTERRUPT_I2Cx_{handle}'
                    if mode == 'slave' else
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
    I2CMasterState_disabled = 0,
    I2CMasterState_standby,
    I2CMasterState_scheduled_job,
    I2CMasterState_transferring,
    I2CMasterState_stopping,
    I2CMasterState_job_attempted,
    I2CMasterState_bus_misbehaved,
};

enum I2CSlaveState : u32
{
    I2CSlaveState_disabled = 0,
    I2CSlaveState_standby,
    I2CSlaveState_receiving_data,
    I2CSlaveState_sending_data,
    I2CSlaveState_ending_transfer,
    I2CSlaveState_bus_misbehaved,
};

enum I2CMasterError : u32
{
    I2CMasterError_none,
    I2CMasterError_no_acknowledge,
    I2CMasterError_clock_stretch_timeout,
};

enum I2CAddressType : u32 // @/`I2C Slave Address`.
{
    I2CAddressType_seven = false,
    I2CAddressType_ten   = true,
};

enum I2CDoJobState : u32
{
    I2CDoJobState_ready_to_be_processed,
    I2CDoJobState_processing,
    I2CDoJobState_attempted,
};

struct I2CDoJob
{
    struct
    {
        enum I2CHandle      handle;
        enum I2CAddressType address_type; // @/`I2C Slave Address`.
        u16                 address;      // "
        b8                  writing;
        b8                  repeating;    // @/`I2C Repeating Transfers`.
        u8*                 pointer;
        i32                 amount;
    };
    struct
    {
        enum I2CDoJobState state;
    };
};

struct I2CDriver
{
    union
    {
        struct I2CDriverMaster
        {
            volatile _Atomic enum I2CMasterState atomic_state;
            enum I2CAddressType                  address_type;
            u32                                  address;
            b8                                   writing;
            b8                                   repeating;
            u8*                                  pointer;
            i32                                  amount;
            i32                                  progress;
            enum I2CMasterError                  error;
        } master;

        struct I2CDriverSlave
        {
            enum I2CSlaveState state;
        } slave;
    };
};

static struct I2CDriver _I2C_drivers[I2CHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



static useret enum I2CReinitResult : u32
{
    I2CReinitResult_success,
    I2CReinitResult_bug = BUG_CODE,
}
I2C_reinit(enum I2CHandle handle)
{

    _EXPAND_HANDLE



    // Reset-cycle the peripheral.

    CMSIS_PUT(I2Cx_RESET, true );
    CMSIS_PUT(I2Cx_RESET, false);

    *driver = (struct I2CDriver) {0};

    NVIC_DISABLE(I2Cx_EV);
    NVIC_DISABLE(I2Cx_ER);



    // Set the kernel clock source for the peripheral.

    CMSIS_PUT(I2Cx_KERNEL_SOURCE, STPY_I2Cx_KERNEL_SOURCE);



    // Clock the peripheral block.

    CMSIS_PUT(I2Cx_ENABLE, true);



    // Configure the peripheral.

    switch (I2Cx_DRIVER_MODE)
    {



        // The I2C peripheral will work as a controller.

        case I2CDriverMode_master:
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



        default: bug;

    }

    CMSIS_SET
    (
        I2Cx    , TIMEOUTR                   ,
        TIMEOUTA, STPY_I2Cx_TIMEOUTR_TIMEOUTA, // Set the desired timeout duration.
        TIMOUTEN, true                       , // Enable the timeout detection.
    );

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

    switch (I2Cx_DRIVER_MODE)
    {

        case I2CDriverMode_master:
        {
            atomic_store_explicit
            (
                &driver->master.atomic_state,
                I2CMasterState_standby,
                memory_order_relaxed // No synchronization needed.
            );
        } break;

        case I2CDriverMode_slave:
        {
            driver->slave.state = I2CSlaveState_standby;
        } break;

        default: bug;

    }

    NVIC_ENABLE(I2Cx_EV);
    NVIC_ENABLE(I2Cx_ER);

    return I2CReinitResult_success;

}



static useret enum I2CDoResult : u32
{
    I2CDoResult_success,
    I2CDoResult_no_acknowledge,
    I2CDoResult_clock_stretch_timeout,
    I2CDoResult_working,
    I2CDoResult_bus_misbehaved,
    I2CDoResult_bug = BUG_CODE,
}
I2C_do(struct I2CDoJob* job)
{

    if (!job)
        bug; // User probably gave us no job by mistake..?

    enum I2CHandle handle = job->handle;

    _EXPAND_HANDLE

    switch (I2Cx_DRIVER_MODE)
    {
        case I2CDriverMode_master : break;
        case I2CDriverMode_slave  : bug; // Only masters can initiate I2C transfers.
        default                   : bug;
    }

    switch (job->address_type)
    {

        case I2CAddressType_seven:
        {
            if (!(0b0000'1000 <= job->address && job->address <= 0b0111'0111))
                bug; // Invalid 7-bit address.
        } break;

        case I2CAddressType_ten:
        {
            if (job->address >= (1 << 10))
                bug; // Address out of range.
        } break;

        default: bug;

    }

    if (!job->pointer)
        bug; // Missing source/destination..?

    if (job->amount <= 0)
        bug; // Bad math..?

    if (!CMSIS_GET(I2Cx, CR1, PE))
        bug; // Ensure I2C has been initialized.



    enum I2CMasterState observed_driver_state =
        atomic_load_explicit
        (
            &driver->master.atomic_state,
            memory_order_acquire // Acquire for `driver->master.error`.
        );

    switch (job->state)
    {



        case I2CDoJobState_ready_to_be_processed: switch (observed_driver_state)
        {

            case I2CMasterState_standby:
            {

                driver->master.address_type = job->address_type;
                driver->master.address      = job->address;
                driver->master.writing      = job->writing;
                driver->master.repeating    = job->repeating;
                driver->master.pointer      = job->pointer;
                driver->master.amount       = job->amount;
                driver->master.progress     = 0;
                driver->master.error        = (enum I2CMasterError) {0};

                atomic_store_explicit
                (
                    &driver->master.atomic_state,
                    I2CMasterState_scheduled_job,
                    memory_order_release // The I2C driver can now handle the above job info.
                );

                NVIC_SET_PENDING(I2Cx_EV);

                job->state = I2CDoJobState_processing;
                return I2CDoResult_working;

            } break;

            case I2CMasterState_bus_misbehaved : return I2CDoResult_bus_misbehaved;
            case I2CMasterState_disabled       : bug; // User needs to reinitialize the driver...
            case I2CMasterState_scheduled_job  : bug; // The I2C driver shouldn't be busy with another job...
            case I2CMasterState_transferring   : bug; // "
            case I2CMasterState_stopping       : bug; // "
            case I2CMasterState_job_attempted  : bug; // User didn't let the previous job conclude properly..?
            default                            : bug;

        } break;



        case I2CDoJobState_processing:
        {

            if
            (
                driver->master.address_type != job->address_type ||
                driver->master.address      != job->address      ||
                driver->master.writing      != job->writing      ||
                driver->master.repeating    != job->repeating    ||
                driver->master.pointer      != job->pointer      ||
                driver->master.amount       != job->amount
            )
                bug; // The I2C driver is doing a different job from the user's...?

            switch (observed_driver_state)
            {



                // The driver just finished attempting the user's job.

                case I2CMasterState_job_attempted:
                {

                    atomic_store_explicit
                    (
                        &driver->master.atomic_state,
                        I2CMasterState_standby, // Acknowledge I2C driver's attempt at the job.
                        memory_order_relaxed    // No synchronization needed.
                    );

                    job->state = I2CDoJobState_attempted;

                    switch (driver->master.error)
                    {
                        case I2CMasterError_none                  : return I2CDoResult_success;
                        case I2CMasterError_no_acknowledge        : return I2CDoResult_no_acknowledge;
                        case I2CMasterError_clock_stretch_timeout : return I2CDoResult_clock_stretch_timeout;
                        default                                   : bug;
                    }

                } break;



                // The driver is still busy with our transfer.

                case I2CMasterState_scheduled_job:
                case I2CMasterState_transferring:
                case I2CMasterState_stopping:
                {
                    return I2CDoResult_working;
                } break;



                case I2CMasterState_bus_misbehaved : return I2CDoResult_bus_misbehaved;
                case I2CMasterState_disabled       : bug; // User needs to reinitialize the driver...
                case I2CMasterState_standby        : bug; // The driver should've been working on the job...
                default                            : bug;

            }

        } break;



        case I2CDoJobState_attempted : bug; // User is trying to update a job that has already concluded.
        default                      : bug;

    }

}



////////////////////////////////////////////////////////////////////////////////



static useret enum I2CSlaveHandleAddressMatchResult : u32
{
    I2CSlaveHandleAddressMatchResult_okay,
    I2CSlaveHandleAddressMatchResult_bug = BUG_CODE,
}
_I2C_slave_handle_address_match(enum I2CHandle handle, u32 interrupt_status)
{

    _EXPAND_HANDLE

    if (I2Cx_DRIVER_MODE != I2CDriverMode_slave)
        bug; // Wrong driver mode!

    if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, ADDCODE) != I2Cx_SLAVE_ADDRESS)
        bug; // Address must match with what we were configured with.

    if (!CMSIS_GET(I2Cx, ISR, TXE)) // TX-register should've been flushed at interrupt event decoding.
        bug; // There shouldn't be anything still in the TX-register. @/`Flushing I2C Transmission Data`.

    b32 master_wants_to_read = CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, DIR);

    if (master_wants_to_read)
    {
        driver->slave.state = I2CSlaveState_sending_data;
    }
    else
    {
        driver->slave.state = I2CSlaveState_receiving_data;
    }

    return I2CSlaveHandleAddressMatchResult_okay;

}



static useret enum I2CUpdateOnceResult : u32
{
    I2CUpdateOnceResult_again,
    I2CUpdateOnceResult_yield,
    I2CUpdateOnceResult_bus_misbehaved,
    I2CUpdateOnceResult_bug = BUG_CODE,
}
_I2C_update_once(enum I2CHandle handle)
{

    _EXPAND_HANDLE



    enum I2CInterruptEvent : u32
    {
        I2CInterruptEvent_none,
        I2CInterruptEvent_clock_stretch_timeout,
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



    // "A bus error is detected when a START or a STOP condition is detected
    // and is not located after a multiple of nine SCL clock pulses."
    //
    // @/pg 2116/sec 48.4.17/`H533rm`.

    if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, BERR))
    {
        return I2CUpdateOnceResult_bus_misbehaved;
    }



    // "An arbitration loss is detected when a high level is sent on the SDA line,
    // but a low level is sampled on the SCL rising edge."
    //
    // @/pg 2116/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, ARLO))
    {
        return I2CUpdateOnceResult_bus_misbehaved;
    }



    // An SMBALERT signal was detected.
    //
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, ALERT))
    {
        bug; // Shouldn't happen; this feature isn't used.
    }



    // Packet error checking detected corruption.
    //
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, PECERR))
    {
        bug; // Shouldn't happen; this feature isn't used.
    }



    // Transfer ready to be reloaded.
    //
    // @/pg 2095/sec 48.4.9/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, TCR))
    {
        bug; // Shouldn't happen; this feature isn't used.
    }



    // The RX-register got overwritten or no data was in the TX-register for transmission.
    //
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, OVR))
    {
        bug; // Shouldn't happen; clock-stretching is enabled by default.
    }



    // Slave acknowledged our transfer request,
    // or we are the slave and the master just called on us.
    //
    // @/pg 2088/fig 639/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, ADDR))
    {
        CMSIS_SET(I2Cx, ISR, TXE   , true); // @/`Flushing I2C Transmission Data`.
        CMSIS_SET(I2Cx, ICR, ADDRCF, true);
        interrupt_event = I2CInterruptEvent_address_match;
    }



    // The slave didn't acknowledge,
    // or the master didn't acknowledge our data.
    //
    // @/pg 2085/sec 48.4.8/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, NACKF))
    {
        CMSIS_SET(I2Cx, ICR, NACKCF, true);
        interrupt_event = I2CInterruptEvent_nack_signaled;
    }



    // There's data from the slave (or the master) in the RX-register.
    //
    // @/pg 2089/sec 48.4.8/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, RXNE))
    {
        interrupt_event = I2CInterruptEvent_data_available_to_read;
    }



    // Data for the slave (or the master) needs to be put in the TX-register.
    //
    // @/pg 2085/sec 48.4.8/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, TXIS))
    {
        interrupt_event = I2CInterruptEvent_ready_to_transmit_data;
    }



    // I2C clock line has been stretched for too long.
    //
    // This should probably be handled after we check the
    // RX-register and TX-register so there won't be any left-over data.
    //
    // @/pg 2117/sec 48.4.17/`H533rm`.

    else if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, TIMEOUT))
    {
        CMSIS_SET(I2Cx, ICR, TIMOUTCF, true);
        interrupt_event = I2CInterruptEvent_clock_stretch_timeout;
    }



    // A STOP condition was successfully sent.
    //
    // We must process the STOP condition after
    // we have read all data from the RX-register.
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



    // Nothing notable happened.

    else
    {
        interrupt_event = I2CInterruptEvent_none;
    }



    // Handle the interrupt event.

    enum I2CMasterState observed_driver_state =
        atomic_load_explicit
        (
            &driver->master.atomic_state,
            memory_order_acquire // Ensure job detail is filled out completely.
        );

    switch (I2Cx_DRIVER_MODE)
    {



        case I2CDriverMode_master: switch (observed_driver_state)
        {



            // Nothing to do until the user schedules a transfer.

            case I2CMasterState_standby: switch (interrupt_event)
            {

                case I2CInterruptEvent_none:
                {
                    if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, BUSY))
                    {
                        return I2CUpdateOnceResult_bus_misbehaved; // There shouldn't be any transfers on the bus of right now.
                    }
                    else
                    {
                        return I2CUpdateOnceResult_yield;
                    }
                } break;

                case I2CInterruptEvent_clock_stretch_timeout           : bug;
                case I2CInterruptEvent_nack_signaled                   : bug;
                case I2CInterruptEvent_stop_signaled                   : bug;
                case I2CInterruptEvent_transfer_completed_successfully : bug;
                case I2CInterruptEvent_data_available_to_read          : bug;
                case I2CInterruptEvent_ready_to_transmit_data          : bug;
                case I2CInterruptEvent_address_match                   : bug;
                default                                                : bug;

            } break;



            // The user has a transfer they want to do.

            case I2CMasterState_scheduled_job: switch (interrupt_event)
            {

                case I2CInterruptEvent_none:
                {
                    if
                    (
                        CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, BUSY) &&
                        CMSIS_GET_FROM(interrupt_enable, I2Cx, CR1, TCIE) // @/`I2C Transfer-Complete Interrupt and Repeated Starts`.
                    )
                    {
                        return I2CUpdateOnceResult_bus_misbehaved; // There shouldn't be any transfers on the bus of right now.
                    }
                    else
                    {

                        if (!(1 <= driver->master.amount && driver->master.amount <= 255))
                            bug; // We currently don't handle transfer sizes larger than this.

                        if (CMSIS_GET(I2Cx, CR2, START))
                            bug; // We shouldn't be already trying to start the transfer...

                        if (driver->master.error)
                            bug; // No reason for an error already...



                        // Configure the desired transfer.

                        u32 sadd = {0};

                        switch (driver->master.address_type)
                        {
                            case I2CAddressType_seven : sadd = driver->master.address << 1; break; // @/`I2C Slave Address`.
                            case I2CAddressType_ten   : sadd = driver->master.address;      break; // "
                            default                   : bug;
                        }

                        CMSIS_SET
                        (
                            I2Cx   , CR2                          ,
                            SADD   , sadd                         , // I2C slave to call for.
                            ADD10  , !!driver->master.address_type, // Whether or not a 10-bit slave address is used.
                            RD_WRN , !driver->master.writing      , // Determine data transfer direction.
                            NBYTES , driver->master.amount        , // Determine amount of data in bytes.
                            START  , true                         , // Begin sending the START condition on the I2C bus.
                        );

                        CMSIS_SET(I2Cx, CR1, TCIE, true); // @/`I2C Transfer-Complete Interrupt and Repeated Starts`.



                        // The I2C peripheral should be now trying to call
                        // the slave device and get an acknowledgement back.

                        atomic_store_explicit
                        (
                            &driver->master.atomic_state,
                            I2CMasterState_transferring,
                            memory_order_relaxed // No synchronization needed.
                        );

                        return I2CUpdateOnceResult_again;

                    }
                } break;

                case I2CInterruptEvent_clock_stretch_timeout           : bug;
                case I2CInterruptEvent_nack_signaled                   : bug;
                case I2CInterruptEvent_stop_signaled                   : bug;
                case I2CInterruptEvent_transfer_completed_successfully : bug;
                case I2CInterruptEvent_data_available_to_read          : bug;
                case I2CInterruptEvent_ready_to_transmit_data          : bug;
                case I2CInterruptEvent_address_match                   : bug;
                default                                                : bug;

            } break;



            // We're in the process of doing the transfer.

            case I2CMasterState_transferring: switch (interrupt_event)
            {



                // Nothing to do until there's an interrupt event.

                case I2CInterruptEvent_none:
                {

                    if (driver->master.error)
                        bug; // No reason for an error already...

                    return I2CUpdateOnceResult_yield;

                } break;



                // The slave didn't acknowledge us.

                case I2CInterruptEvent_nack_signaled:
                {

                    if (driver->master.error)
                        bug; // There shouldn't have been any other errors before this.



                    // Begin sending out the STOP signal.

                    CMSIS_SET(I2Cx, CR2, STOP, true);



                    // We now wait for the STOP condition on the bus to truly end the transfer.

                    driver->master.error = I2CMasterError_no_acknowledge;

                    atomic_store_explicit
                    (
                        &driver->master.atomic_state,
                        I2CMasterState_stopping,
                        memory_order_relaxed // No synchronization needed.
                    );

                    return I2CUpdateOnceResult_again;

                } break;



                // The slave sent us some data.

                case I2CInterruptEvent_data_available_to_read:
                {

                    if (driver->master.error)
                        bug; // There shouldn't have been any other errors before this.

                    if (!(0 <= driver->master.progress && driver->master.progress < driver->master.amount))
                        bug; // Hardware is giving us an unexpected amount of data...



                    // Pop the byte from the RX-register and push it into the destination.

                    u8 data = CMSIS_GET(I2Cx, RXDR, RXDATA); // @/`I2C FIFO Depth`.

                    driver->master.pointer[driver->master.progress]  = data;
                    driver->master.progress                         += 1;

                    return I2CUpdateOnceResult_again;

                } break;



                // We're ready to send some data to the slave.

                case I2CInterruptEvent_ready_to_transmit_data:
                {

                    if (driver->master.error)
                        bug; // There shouldn't have been any other errors before this.

                    if (!(0 <= driver->master.progress && driver->master.progress < driver->master.amount))
                        bug; // Hardware is demanding us an unexpected amount of data...


                    // Pop a byte from the source and push it into the TX-register.

                    u8 data = driver->master.pointer[driver->master.progress];

                    CMSIS_SET(I2Cx, TXDR, TXDATA, data); // @/`I2C FIFO Depth`.

                    driver->master.progress += 1;

                    return I2CUpdateOnceResult_again;

                } break;



                // We finished transferring all the data.

                case I2CInterruptEvent_transfer_completed_successfully:
                {

                    if (driver->master.error)
                        bug; // But the hardware said that the transfer was completed successfully!

                    if (driver->master.progress != driver->master.amount)
                        bug; // Hardware says we finished the transfer, but software disagrees...

                    if (driver->master.repeating)
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



                        // The I2C peripheral should be good to immediately start the next transfer.

                        atomic_store_explicit
                        (
                            &driver->master.atomic_state,
                            I2CMasterState_job_attempted,
                            memory_order_release // Ensure writes to `driver->master.error` finishes (although it's not used here).
                        );



                        return I2CUpdateOnceResult_again;

                    }
                    else // Try sending the STOP condition onto the bus and wait for it.
                    {

                        CMSIS_SET(I2Cx, CR2, STOP, true);

                        atomic_store_explicit
                        (
                            &driver->master.atomic_state,
                            I2CMasterState_stopping,
                            memory_order_relaxed // No synchronization needed.
                        );

                        return I2CUpdateOnceResult_again;

                    }

                } break;



                // The slave stretched the clock for too long,
                // or perhaps our I2C peripheral got locked up somehow.

                case I2CInterruptEvent_clock_stretch_timeout:
                {

                    if (driver->master.error)
                        bug; // There shouldn't have been any other errors before this.



                    // A STOP condition is automatically sent, so we'll wait for that.
                    // @/pg 2117/sec 48.4.17/`H533rm`.

                    driver->master.error = I2CMasterError_clock_stretch_timeout;

                    atomic_store_explicit
                    (
                        &driver->master.atomic_state,
                        I2CMasterState_stopping,
                        memory_order_relaxed // No synchronization needed.
                    );

                    return I2CUpdateOnceResult_again;

                } break;



                case I2CInterruptEvent_stop_signaled : bug;
                case I2CInterruptEvent_address_match : bug;
                default                              : bug;

            } break;



            // We're in the process of stopping the transfer.

            case I2CMasterState_stopping: switch (interrupt_event)
            {



                // Nothing to do until there's an interrupt event.

                case I2CInterruptEvent_none:
                {
                    return I2CUpdateOnceResult_yield;
                } break;



                // We have sent the STOP condition successfully. Whether or not
                // the transfer itself was actually successful is another thing.

                case I2CInterruptEvent_stop_signaled:
                {
                    if (CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, BUSY))
                    {
                        return I2CUpdateOnceResult_bus_misbehaved; // The bus should be no longer busy now.
                    }
                    else
                    {
                        atomic_store_explicit
                        (
                            &driver->master.atomic_state,
                            I2CMasterState_job_attempted, // Let the user know that we tried to do the transfer.
                            memory_order_release          // Ensure writes to `driver->master.error` finishes.
                        );
                        return I2CUpdateOnceResult_again;
                    }
                } break;



                // We couldn't put a STOP condition on the I2C bus for some reason;
                // this could be the I2C peripheral being locked up.

                case I2CInterruptEvent_clock_stretch_timeout:
                {
                    return I2CUpdateOnceResult_bus_misbehaved;
                } break;



                case I2CInterruptEvent_transfer_completed_successfully : bug;
                case I2CInterruptEvent_data_available_to_read          : bug;
                case I2CInterruptEvent_ready_to_transmit_data          : bug;
                case I2CInterruptEvent_nack_signaled                   : bug;
                case I2CInterruptEvent_address_match                   : bug;
                default                                                : bug;

            } break;



            // Waiting for user to acknowledge the completion of the transfer...

            case I2CMasterState_job_attempted: switch (interrupt_event)
            {

                case I2CInterruptEvent_none:
                {
                    if
                    (
                        CMSIS_GET_FROM(interrupt_status, I2Cx, ISR, BUSY) &&
                        CMSIS_GET_FROM(interrupt_enable, I2Cx, CR1, TCIE) // @/`I2C Transfer-Complete Interrupt and Repeated Starts`.
                    )
                    {
                        return I2CUpdateOnceResult_bus_misbehaved; // There shouldn't be any transfers on the bus of right now.
                    }
                    else
                    {
                        return I2CUpdateOnceResult_yield;
                    }
                } break;

                case I2CInterruptEvent_clock_stretch_timeout           : bug;
                case I2CInterruptEvent_nack_signaled                   : bug;
                case I2CInterruptEvent_stop_signaled                   : bug;
                case I2CInterruptEvent_transfer_completed_successfully : bug;
                case I2CInterruptEvent_data_available_to_read          : bug;
                case I2CInterruptEvent_ready_to_transmit_data          : bug;
                case I2CInterruptEvent_address_match                   : bug;
                default                                                : bug;

            } break;



            case I2CMasterState_bus_misbehaved:
            {
                return I2CUpdateOnceResult_bus_misbehaved; // I2C driver stuck in this error condition...
            } break;



            case I2CMasterState_disabled : bug;
            default                      : bug;

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
                    enum I2CSlaveHandleAddressMatchResult result = _I2C_slave_handle_address_match(handle, interrupt_status);
                    switch (result)
                    {
                        case I2CSlaveHandleAddressMatchResult_okay : return I2CUpdateOnceResult_again;
                        case I2CSlaveHandleAddressMatchResult_bug  : bug;
                        default                                    : bug;
                    }
                } break;



                // If the I2C peripheral is detecting that the
                // clock has been stretched for too long, even
                // though we're not handling any transfers, then
                // it's likely there's something weird that's happening
                // on the I2C bus lines.

                case I2CInterruptEvent_clock_stretch_timeout:
                {
                    return I2CUpdateOnceResult_bus_misbehaved;
                } break;



                case I2CInterruptEvent_nack_signaled                   : bug;
                case I2CInterruptEvent_stop_signaled                   : bug;
                case I2CInterruptEvent_data_available_to_read          : bug;
                case I2CInterruptEvent_ready_to_transmit_data          : bug;
                case I2CInterruptEvent_transfer_completed_successfully : bug;
                default                                                : bug;

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

                    u8 data = CMSIS_GET(I2Cx, RXDR, RXDATA); // @/`I2C FIFO Depth`.

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



                // Either the master is taking too long to send us data,
                // or the user is processing the data too slow, or the
                // I2C peripheral is locked up somehow. The clock-stretching
                // timeout duration should be long enough that the processing
                // time of both master and slave is unlikely to be the reason
                // anyways.

                case I2CInterruptEvent_clock_stretch_timeout:
                {

                    // We just abort the data reception and
                    // hope the I2C bus will figure itself out.
                    // We also let the user know about this time-out
                    // so they can reinitialize if desired.

                    driver->slave.state = I2CSlaveState_standby;

                    I2Cx_CALLBACK.slave(I2CSlaveCallbackEvent_clock_stretch_timeout, nullptr);

                    return I2CUpdateOnceResult_again;

                } break;



                // The master did a repeated START.

                case I2CInterruptEvent_address_match:
                {
                    enum I2CSlaveHandleAddressMatchResult result = _I2C_slave_handle_address_match(handle, interrupt_status);
                    switch (result)
                    {
                        case I2CSlaveHandleAddressMatchResult_okay : return I2CUpdateOnceResult_again;
                        case I2CSlaveHandleAddressMatchResult_bug  : bug;
                        default                                    : bug;
                    }
                } break;



                case I2CInterruptEvent_nack_signaled                   : bug;
                case I2CInterruptEvent_ready_to_transmit_data          : bug;
                case I2CInterruptEvent_transfer_completed_successfully : bug;
                default                                                : bug;

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

                    CMSIS_SET(I2Cx, TXDR, TXDATA, data); // @/`I2C FIFO Depth`.

                    return I2CUpdateOnceResult_again;

                } break;



                // The master gave us a NACK, which typically means that was
                // the last byte of the transfer. We should be expecting the
                // STOP signal next, or perhaps a repeated START condition.

                case I2CInterruptEvent_nack_signaled:
                {
                    driver->slave.state = I2CSlaveState_ending_transfer;
                    return I2CUpdateOnceResult_again;
                } break;



                // Either the master is taking too long to process our data,
                // or the user is queuing the data too slow, or the
                // I2C peripheral is locked up somehow. The clock-stretching
                // timeout duration should be long enough that the processing
                // time of both master and slave is unlikely to be the reason
                // anyways.

                case I2CInterruptEvent_clock_stretch_timeout:
                {

                    // We just abort the data transmission and
                    // hope the I2C bus will figure itself out.
                    // We also let the user know about this time-out
                    // so they can reinitialize if desired.

                    driver->slave.state = I2CSlaveState_standby;

                    I2Cx_CALLBACK.slave(I2CSlaveCallbackEvent_clock_stretch_timeout, nullptr);

                    return I2CUpdateOnceResult_again;

                } break;



                // The master did a repeated START.

                case I2CInterruptEvent_address_match:
                {
                    enum I2CSlaveHandleAddressMatchResult result = _I2C_slave_handle_address_match(handle, interrupt_status);
                    switch (result)
                    {
                        case I2CSlaveHandleAddressMatchResult_okay : return I2CUpdateOnceResult_again;
                        case I2CSlaveHandleAddressMatchResult_bug  : bug;
                        default                                    : bug;
                    }
                } break;



                case I2CInterruptEvent_data_available_to_read          : bug;
                case I2CInterruptEvent_stop_signaled                   : bug;
                case I2CInterruptEvent_transfer_completed_successfully : bug;
                default                                                : bug;

            } break;



            // We're waiting for a STOP condition on the bus now,
            // or perhaps a repeated START condition.

            case I2CSlaveState_ending_transfer: switch (interrupt_event)
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



                // We got a repeated START condition.

                case I2CInterruptEvent_address_match:
                {
                    enum I2CSlaveHandleAddressMatchResult result = _I2C_slave_handle_address_match(handle, interrupt_status);
                    switch (result)
                    {

                        case I2CSlaveHandleAddressMatchResult_okay:
                        {
                            I2Cx_CALLBACK.slave(I2CSlaveCallbackEvent_repeated_start_signaled, nullptr);
                            return I2CUpdateOnceResult_again;
                        } break;

                        case I2CSlaveHandleAddressMatchResult_bug : bug;
                        default                                   : bug;

                    }
                } break;



                // We couldn't detect a STOP condition on the I2C bus for some reason;
                // this could be the I2C peripheral being locked up.

                case I2CInterruptEvent_clock_stretch_timeout:
                {
                    return I2CUpdateOnceResult_bus_misbehaved;
                } break;



                case I2CInterruptEvent_nack_signaled                   : bug;
                case I2CInterruptEvent_data_available_to_read          : bug;
                case I2CInterruptEvent_ready_to_transmit_data          : bug;
                case I2CInterruptEvent_transfer_completed_successfully : bug;
                default                                                : bug;

            } break;



            case I2CSlaveState_bus_misbehaved:
            {
                return I2CUpdateOnceResult_bus_misbehaved; // I2C driver stuck in this error condition...
            } break;



            case I2CSlaveState_disabled : bug; // User needs to reinitialize the I2C driver.
            default                     : bug;

        } break;



        default: bug;

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
            {

                NVIC_DISABLE(I2Cx_EV);
                NVIC_DISABLE(I2Cx_ER);

                switch (I2Cx_DRIVER_MODE)
                {

                    case I2CDriverMode_master:
                    {
                        atomic_store_explicit
                        (
                            &driver->master.atomic_state,
                            I2CMasterState_bus_misbehaved, // The user will be notified after trying to do any sort of transfer.
                            memory_order_relaxed           // No synchronization needed.
                        );
                    } break;

                    case I2CDriverMode_slave:
                    {
                        driver->slave.state = I2CSlaveState_bus_misbehaved;
                        I2Cx_CALLBACK.slave(I2CSlaveCallbackEvent_bus_misbehaved, nullptr);
                    } break;

                    default: goto BUG;

                }

                yield = true;

            } break;



            BUG:;
            case I2CUpdateOnceResult_bug:
            default:
            {

                // Shut down the driver!

                CMSIS_PUT(I2Cx_RESET, true);

                NVIC_DISABLE(I2Cx_EV);
                NVIC_DISABLE(I2Cx_ER);

                *driver = (struct I2CDriver) {0};



                // Let the user know that the I2C driver is in a disabled
                // state and that they should reinitialize it.

                switch (I2Cx_DRIVER_MODE)
                {

                    case I2CDriverMode_master:
                    {
                        // The user will be notified after trying to do any sort of transfer.
                    } break;

                    case I2CDriverMode_slave:
                    {
                        I2Cx_CALLBACK.slave(I2CSlaveCallbackEvent_bug, nullptr);
                    } break;

                    default:
                    {
                        // Not much we can do here...
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
// There are different modes that the I2C driver can take on:
//
//      - `master`
//          The I2C driver will act as a master and each
//          read/write transfer will be blocking. This
//          simplifies the control-flow, but is obviously
//          inefficient in terms of work being done.
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
//
// The callback routines should be able to reinitialize the I2C driver
// any time it wants to.



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
//
// The flushing is also done for repeated START conditions too,
// which happen on the interrupt event of address match.



// @/`I2C Repeating Transfers`:
//
// A repeated I2C transfer is when the master finishes a transfer,
// but rather than letting go of the I2C bus, a REPEATED-START condition
// is sent that allows the master to keep arbitration of the bus.
// This only has mild implication for read/write throughput, more
// importance for when there's multiple masters, but it's really
// important depending on the slave device that the user is
// communicating with.
//
// Some I2C devices have the protocol where, before any read-transfers,
// a write must be done first (this would be specifying a register address
// for instance), and then only can the read transfer be done. The
// slave device, however, may expect that this is done with a repeated START,
// thus the overall read-transfer cannot be split into a write-transfer followed
// by a read-transfer.



// @/`I2C FIFO Depth`:
//
// The I2C peripheral on the STM32H533xx has a whopping 1-byte deep FIFO.
// Not great for high-throughput transfers, so DMA would be nice here, but
// really, the user should not use I2C for anything that requires such high
// read/write performance. Just as of writing, it's not worth the complication
// of introducing DMA coordination here.
