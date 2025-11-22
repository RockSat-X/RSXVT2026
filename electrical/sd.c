#include "sd.meta"
#include "sd_initer.c"
#include "sd_cmder.c"



////////////////////////////////////////////////////////////////////////////////



#include "sdmmc_driver_support.meta"
/* #meta

    IMPLEMENT_DRIVER_SUPPORT(
        driver_type = 'SD',
        cmsis_name  = 'SDMMC',
        common_name = 'SDx',
        entries     = (
            { 'name'      : '{}'                      , 'value'       : ... },
            { 'name'      : 'NVICInterrupt_{}'        , 'value'       : ... },
            { 'name'      : 'STPY_{}_KERNEL_SOURCE'   , 'value'       : ... },
            { 'name'      : 'STPY_{}_INITIAL_DIVIDER' , 'value'       : ... },
            { 'name'      : 'STPY_{}_INITIAL_DATATIME', 'value'       : ... },
            { 'name'      : 'STPY_{}_FULL_DIVIDER'    , 'value'       : ... },
            { 'name'      : 'STPY_{}_FULL_DATATIME'   , 'value'       : ... },
            { 'name'      : '{}_RESET'                , 'cmsis_tuple' : ... },
            { 'name'      : '{}_ENABLE'               , 'cmsis_tuple' : ... },
            { 'name'      : '{}_KERNEL_SOURCE'        , 'cmsis_tuple' : ... },
            { 'interrupt' : 'INTERRUPT_{}'                                  },
        ),
    )

*/



#define SDMMC_MAX_DEADLOCK_DURATION_MS 250



struct Sector
{
    u8 data[512];
};



enum SDDriverState : u32
{
    SDDriverState_initer,
    SDDriverState_active,
    SDDriverState_error,
};



enum SDDriverError : u32
{
    SDDriverError_sdmmc_deadlocked,
    SDDriverError_cmder_needs_sdmmc_reinited,
    SDDriverError_card_likely_unmounted,
    SDDriverError_card_likely_unsupported,
};



enum SDOperation : u32
{
    SDOperation_single_read  = SDCmd_READ_SINGLE_BLOCK,
    SDOperation_single_write = SDCmd_WRITE_BLOCK,
};



enum SDTaskState : u32
{
    SDTaskState_unscheduled,
    SDTaskState_booked,
    SDTaskState_done,
    SDTaskState_error,
};



struct SDDriver
{
    volatile enum SDDriverState state;
    volatile u32                last_isr_timestamp_ms; // @/`SDMMC Deadlock`.
    struct SDIniter             initer;
    struct SDCmder              cmder;
    enum SDDriverError          error;

    struct
    {
        volatile enum SDTaskState state;
        volatile enum SDOperation operation;
        volatile struct Sector*   sector;
        volatile u32              address;
    } task;
};



static struct SDDriver _SD_drivers[SDHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



static void
_SD_check_deadlock(enum SDHandle handle)
{
    #if 0 // TODO.

        volatile u32 last_isr_timestamp_ms = driver->last_isr_timestamp_ms;
        __DMB();
        volatile u32 now_ms = systick_ms;

        u32 inactivity_ms = now_ms - last_isr_timestamp_ms;

        if (inactivity_ms >= SDMMC_MAX_DEADLOCK_DURATION_MS)
        {
            driver->state = SDDriverState_error;
            driver->error = SDDriverError_sdmmc_deadlocked;
        }

    #endif
}



////////////////////////////////////////////////////////////////////////////////



#undef  ret
#define ret(NAME) return SDDo_##NAME
static useret enum SDDo : u32
{
    SDDo_task_completed,
    SDDo_task_in_progress,
    SDDo_task_error,
    SDDo_driver_error,
    SDDo_bug,
}
SD_do
(
    enum SDHandle    handle,
    enum SDOperation operation,
    struct Sector*   sector,
    u32              address
)
{

    _EXPAND_HANDLE



    if (!sector)
        bug;



    // If the driver is in an error condition, nothing
    // will be done until the user reinitialize the driver.

    enum SDDriverState driver_state = driver->state;

    if (driver_state == SDDriverState_error)
        ret(driver_error);



    // If there was a task already, make sure the user isn't
    // trying to schedule a completely different task.

    enum SDTaskState task_state = driver->task.state;

    if (task_state != SDTaskState_unscheduled)
    {
        if (driver->task.operation != operation)
            bug;

        if (driver->task.sector != sector)
            bug;

        if (driver->task.address != address)
            bug;
    }



    switch (task_state)
    {

        // Schedule the next task for the SD driver to carry out.

        case SDTaskState_unscheduled:
        {

            driver->task.operation = operation;
            driver->task.sector    = sector;
            driver->task.address   = address;
            __DMB();
            driver->task.state     = SDTaskState_booked;

            NVIC_SET_PENDING(SDx); // Alert SD driver of the new task.

            ret(task_in_progress);

        } break;



        // The task has already been scheduled; we'll just
        // wait for the SD driver to finish carrying it out.

        case SDTaskState_booked:
        {

            _SD_check_deadlock(handle);

            driver_state = driver->state;

            if (driver_state == SDDriverState_error)
            {
                ret(driver_error);
            }
            else
            {
                ret(task_in_progress);
            }

        } break;



        // The task has been successfully executed.

        case SDTaskState_done:
        {
            driver->task.state = SDTaskState_unscheduled;
            ret(task_completed);
        } break;



        case SDTaskState_error : ret(task_error);
        default                : bug;

    }

}



////////////////////////////////////////////////////////////////////////////////



#undef  ret
#define ret(NAME) return SDPoll_##NAME
static useret enum SDPoll : u32
{
    SDPoll_idle,
    SDPoll_busy,
    SDPoll_cleared_task_error,
    SDPoll_driver_error,
    SDPoll_bug,
}
SD_poll(enum SDHandle handle)
{

    _EXPAND_HANDLE



    // In the event of a task error, the user will do a poll in order
    // to clear the error condition. The SD driver can recover from
    // task errors, so reinitialization is not necessary here.
    // Note that this check is done first before the check for driver
    // errors, because the user might've seen that there was a task error
    // as reported by `SD_do`, so they'll call `SD_poll` to clear it
    // and thus expect `SDPoll_cleared_task_error`. A driver error can
    // theoritically happen between when the task error was reported and
    // when it'll get cleared by the user, so to make it simpler for the
    // user, we prioritize clearing the task error first.

    enum SDTaskState task_state = driver->task.state;

    if (task_state == SDTaskState_error)
    {
        driver->task.state = SDTaskState_unscheduled;
        ret(cleared_task_error);
    }



    // If the driver is in an error condition, nothing
    // will be done until the user reinitialize the driver.

    enum SDDriverState driver_state = driver->state;

    if (driver_state == SDDriverState_error)
        ret(driver_error);



    switch (task_state)
    {

        // Nothing going on; the user can schedule the next task.

        case SDTaskState_unscheduled:
        {
            ret(idle);
        } break;



        // The SD driver is busy with a task right now.

        case SDTaskState_booked:
        {

            _SD_check_deadlock(handle);

            driver_state = driver->state;

            if (driver_state == SDDriverState_error)
            {
                ret(driver_error);
            }
            else
            {
                ret(busy);
            }

        } break;



        // The SD driver just finished a task and
        // the next one can be scheduled if need be.

        case SDTaskState_done:
        {
            ret(idle);
        } break;



        case SDTaskState_error : bug;
        default                : bug;

    }

}



////////////////////////////////////////////////////////////////////////////////



static void
SD_reinit(enum SDHandle handle)
{

    _EXPAND_HANDLE



    // Reset-cycle the peripheral.

    CMSIS_PUT(SDx_RESET, true );
    CMSIS_PUT(SDx_RESET, false);

    *driver = (struct SDDriver) {0};



    // Enable the interrupts.

    NVIC_ENABLE(SDx);



    // Set the kernel clock source for the peripheral.

    CMSIS_PUT(SDx_KERNEL_SOURCE, STPY_SDx_KERNEL_SOURCE);



    // Enable the peripheral.

    CMSIS_PUT(SDx_ENABLE, true);



    // Configure the peripheral.

    CMSIS_SET(SDx, POWER  , PWRCTRL , 0b11                     ); // Power the clock.
    CMSIS_SET(SDx, DTIMER , DATATIME, STPY_SDx_INITIAL_DATATIME); // Max timeout period.
    CMSIS_SET
    (
        SDx    , CLKCR                   , // TODO Use PWRSAV?
        HWFC_EN, true                    , // Allow output clock to be halted to prevent FIFO overrun/underrun.
        CLKDIV , STPY_SDx_INITIAL_DIVIDER, // Divide the kernel clock down to the desired bus frequency.
        NEGEDGE, true                    , // Sample on rise, setup on fall. @/pg 307/fig 6.6.6/`SD`.
    );

    CMSIS_SET
    (
        SDx        , MASK, // Enable interrupts for:
        CMDSENTIE  , true, //     - Command sent      (applies only when no response is expected).
        CCRCFAILIE , true, //     - Command CRC fail  (applies only when a  response is expected).
        CTIMEOUTIE , true, //     - Command timed out (applies only when a  response is expected).
        CMDRENDIE  , true, //     - Command response received.
        DCRCFAILIE , true, //     - Data-block CRC error.
        DTIMEOUTIE , true, //     - Data-block timed out.
        DBCKENDIE  , true, //     - Data-block sent/received successfully.
        DATAENDIE  , true, //     - Data transfer ended correctly.
        RXOVERRIE  , true, //     - RX-FIFO overrun.
        TXUNDERRIE , true, //     - TX-FIFO underrun.
        BUSYD0ENDIE, true, //     - Busy signal on D0 has been lifted.
    );



    // Rest of initialization is done within the interrupt handler.

    NVIC_SET_PENDING(SDx);

}



////////////////////////////////////////////////////////////////////////////////



#undef  ret
#define ret(NAME) return SDUpdateOnce_##NAME
static useret enum SDUpdateOnce : u32
{
    SDUpdateOnce_again,
    SDUpdateOnce_yield,
    SDUpdateOnce_bug,
}
_SD_update_once(enum SDHandle handle)
{

    _EXPAND_HANDLE



    // Run the SD-cmder to handle the execution of SD commands.

    enum SDCmderUpdate cmder_result = {0};

    do
    {
        cmder_result = SDCmder_update(SDx, &driver->cmder);
    }
    while (cmder_result == SDCmderUpdate_again);

    if (cmder_result == SDCmderUpdate_sdmmc_needs_reinit)
    {
        driver->state = SDDriverState_error;
        driver->error = SDDriverError_cmder_needs_sdmmc_reinited;
    }



    switch (driver->state)
    {

        // The SD card is in the process of being initialized.

        case SDDriverState_initer: switch (cmder_result)
        {

            // The SD-cmder can execute a command, so we'll
            // query SD-initer for the next command we should
            // do to get closer to initializing the SD card.

            case SDCmderUpdate_ready_for_next_command:
            {

                enum SDIniterUpdate initer_result = SDIniter_update(&driver->initer);

                switch (initer_result)
                {

                    // SD-initer gave us the command to execute next.

                    case SDIniterUpdate_do_cmd:
                    {

                        driver->cmder =
                            (struct SDCmder)
                            {
                                .state      = SDCmderState_scheduled_command,
                                .cmd        = driver->initer.cmd.cmd,
                                .arg        = driver->initer.cmd.arg,
                                .data       = driver->initer.cmd.data,
                                .remaining  = driver->initer.cmd.size,
                                .block_size = driver->initer.cmd.size,
                                .rca        = driver->initer.rca,
                            };

                        ret(again);

                    } break;



                    // The SD-initer says we can increase
                    // the bus width width now. At this point,
                    // we can also increase the bus speed.

                    case SDIniterUpdate_caller_set_bus_width:
                    {

                        CMSIS_SET(SDx, DTIMER, DATATIME, STPY_SDx_FULL_DATATIME); // New max timeout period.
                        CMSIS_SET
                        (
                            SDx   , CLKCR                ,
                            CLKDIV, STPY_SDx_FULL_DIVIDER, // Divide the kernel clock down to the desired bus frequency.
                            WIDBUS, 0b01                 , // Set peripheral to now use 4-bit wide bus. TODO Customize.
                        );

                        ret(again);

                    } break;



                    // The errors that the SD-initer is
                    // seeing is making it think that
                    // there's no SD card at all.

                    case SDIniterUpdate_card_likely_unmounted:
                    {

                        driver->state = SDDriverState_error;
                        driver->error = SDDriverError_card_likely_unmounted;

                        ret(again);

                    } break;



                    // The SD-initer couldn't initialize
                    // the SD card most likely because the
                    // card is not supported by SD-initer.

                    case SDIniterUpdate_card_likely_unsupported:
                    {

                        driver->state = SDDriverState_error;
                        driver->error = SDDriverError_card_likely_unsupported;

                        ret(again);

                    } break;



                    // Hip hip hooray! We've finished initializing the SD card!

                    case SDIniterUpdate_done:
                    {

                        driver->state = SDDriverState_active;

                        ret(again);

                    } break;



                    case SDIniterUpdate_bug : bug;
                    default                 : bug;

                }

            } break;



            // The SD-cmder is busy doing stuff...

            case SDCmderUpdate_yield:
            {
                ret(yield);
            } break;



            // The SD-cmder just finished executing SD-initer's command.
            // It may or may not have been successful, so we'll report
            // this back to SD-initer to handle and figure out what to do next.

            case SDCmderUpdate_command_attempted:
            {

                driver->initer.feedback =
                    (struct SDIniterFeedback)
                    {
                        .cmd_failed = !!driver->cmder.error,
                        .response   =
                            {
                                SDx->RESP1,
                                SDx->RESP2,
                                SDx->RESP3,
                                SDx->RESP4,
                            },
                    };

                ret(again);

            } break;



            case SDCmderUpdate_again              : bug;
            case SDCmderUpdate_sdmmc_needs_reinit : bug;
            case SDCmderUpdate_bug                : bug;
            default                               : bug;

        } break;



        // The SD card is initialized and
        // the driver is ready to handle tasks.

        case SDDriverState_active: switch (cmder_result)
        {

            // If there's a task scheduled,
            // we pass it to SD-cmder to do.

            case SDCmderUpdate_ready_for_next_command:
            {

                if (driver->task.state != SDTaskState_booked)
                    ret(yield);

                if (!driver->task.operation)
                    bug;

                if (!driver->task.sector)
                    bug;

                driver->cmder =
                    (struct SDCmder)
                    {
                        .state      = SDCmderState_scheduled_command,
                        .cmd        = (enum SDCmd) driver->task.operation,
                        .arg        = driver->task.address,
                        .data       = (u8*) driver->task.sector,
                        .remaining  = sizeof(driver->task.sector->data),
                        .block_size = sizeof(driver->task.sector->data),
                        .rca        = driver->initer.rca,
                    };

                ret(again);

            } break;



            // The SD-cmder is busy doing stuff...

            case SDCmderUpdate_yield:
            {

                if (driver->task.state != SDTaskState_booked)
                    bug;

                ret(yield);

            } break;



            // The SD-cmder just finished executing the command.
            // It may or may not have been successful, so it'll be up
            // to the user to acknowledge and handle.

            case SDCmderUpdate_command_attempted:
            {

                if (driver->task.state != SDTaskState_booked)
                    bug;

                if (driver->cmder.error)
                {
                    driver->task.state = SDTaskState_error;
                }
                else
                {
                    driver->task.state = SDTaskState_done;
                }

                ret(again);

            } break;



            case SDCmderUpdate_again              : bug;
            case SDCmderUpdate_sdmmc_needs_reinit : bug;
            case SDCmderUpdate_bug                : bug;
            default                               : bug;

        } break;



        // The SD driver is stuck in an error
        // condition right now, so nothing will
        // be done until the user reinitializes
        // the whole driver.

        case SDDriverState_error:
        {
            ret(yield);
        } break;



        default: bug;

    }

}



////////////////////////////////////////////////////////////////////////////////



static void
_SD_driver_interrupt(enum SDHandle handle)
{

    _EXPAND_HANDLE

    for (b32 yield = false; !yield;)
    {

        // TODO: driver->last_isr_timestamp_ms = systick_ms;

        enum SDUpdateOnce result = _SD_update_once(handle);

        yield = (result == SDUpdateOnce_yield);

        switch (result)
        {
            case SDUpdateOnce_again:
            {
                // The state-machine will be updated again.
            } break;

            case SDUpdateOnce_yield:
            {
                // We can stop updating the state-machine for now.
            } break;

            case SDUpdateOnce_bug : panic;
            default               : panic;
        }

    }

}
