#include "sd.meta"
#include "sd_initer.c"
#include "sd_cmder.c"



////////////////////////////////////////////////////////////////////////////////



#include "sdmmc_driver_support.meta"
/* #meta

    IMPLEMENT_DRIVER_ALIASES(
        driver_name = 'SD',
        cmsis_name  = 'SDMMC',
        common_name = 'SDx',
        identifiers = (
            'NVICInterrupt_{}',
            'STPY_{}_KERNEL_SOURCE',
            'STPY_{}_INITIAL_DIVIDER',
            'STPY_{}_INITIAL_DATATIME',
            'STPY_{}_FULL_DIVIDER',
            'STPY_{}_FULL_DATATIME',
        ),
        cmsis_tuple_tags = (
            '{}_ENABLE',
            '{}_RESET',
            '{}_KERNEL_SOURCE',
        ),
    )

    for target in PER_TARGET():

        for handle, instance in target.drivers.get('SD', ()):

            Meta.line(f'''

                static void
                _SD_update_entirely(enum SDHandle handle);

                INTERRUPT_{instance}
                {{
                    _SD_update_entirely(SDHandle_{handle});
                }}

            ''')

*/



enum SD
{
    SD_SDMMC1,
    SD_COUNT,
};



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



enum SDOp : u32
{
    SDOp_single_read  = SDCmd_READ_SINGLE_BLOCK,
    SDOp_single_write = SDCmd_WRITE_BLOCK,
};



enum SDTaskState : u32
{
    SDTaskState_unscheduled,
    SDTaskState_booked,
    SDTaskState_done,
    SDTaskState_error,
};



#define SDMMC_MAX_DEADLOCK_DURATION_MS 250



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
        volatile enum SDOp        op;
        volatile struct Sector*   sector;
        volatile u32              addr;
    } task;
};



enum SDTestKind : u32
{
    SDTestKind_basic_reads,
    SDTestKind_profiling_reads,
    SDTestKind_basic_writes,
    SDTestKind_profiling_writes,
    SDTestKind_stability,
};



static struct SDDriver     _SD_drivers     [SD_COUNT] = {0};



static const struct { SDMMC_TypeDef* peripheral; enum NVICInterrupt interrupt; } SD_DRIVER_TABLE[] =
    {
        { SDMMC1, NVICInterrupt_SDMMC1 },
    };



////////////////////////////////////////////////////////////////////////////////



static void
_SD_check_deadlock(enum SDHandle handle)
{
    // TODO.
    //volatile u32 last_isr_timestamp_ms = driver->last_isr_timestamp_ms;
    //__DMB();
    //volatile u32 now_ms = systick_ms;

    //u32 inactivity_ms = now_ms - last_isr_timestamp_ms;

    //if (inactivity_ms >= SDMMC_MAX_DEADLOCK_DURATION_MS)
    //{
    //    driver->state = SDDriverState_error;
    //    driver->error = SDDriverError_sdmmc_deadlocked;
    //}
}



////////////////////////////////////////////////////////////////////////////////



static useret enum SDDo : u32
{
    SDDo_task_completed,
    SDDo_task_in_progress,
    SDDo_task_error,
    SDDo_driver_error,
    SDDo_bug,
}
SD_do(enum SDHandle handle, enum SDOp op, struct Sector* sector, u32 addr)
{

    _EXPAND_HANDLE

    #undef  ret
    #define ret(NAME) return SDDo_##NAME

    if (!sector)
        bug;

    enum SDTaskState   task_state   = driver->task.state;
    enum SDDriverState driver_state = driver->state;

    if (driver_state == SDDriverState_error)
    {
        ret(driver_error); // SD driver is in a locked-up state until the user reinitializes everything.
    }
    else
    {
        if (task_state != SDTaskState_unscheduled)
        {
            if (driver->task.op     != op    )
                bug; // Make sure it's corresponding to the same operation.
            if (driver->task.sector != sector)
                bug; // "
            if (driver->task.addr   != addr  )
                bug; // "
        }

        switch (task_state)
        {
            case SDTaskState_unscheduled:
            {
                driver->task.op     = op;
                driver->task.sector = sector;
                driver->task.addr   = addr;

                __DMB(); // `SDTaskState_booked` will indicate that the task is available for the SDMMC ISR to process.

                driver->task.state = SDTaskState_booked;

                NVIC_SET_PENDING(SDx); // Alert SD driver of the new task.

                ret(task_in_progress);
            } break;

            case SDTaskState_done:
            {
                driver->task.state = SDTaskState_unscheduled;
                ret(task_completed);
            } break;

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

            case SDTaskState_error  : ret(task_error);
            default                 : bug;
        }
    }
}



////////////////////////////////////////////////////////////////////////////////



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

    #undef  ret
    #define ret(NAME) return SDPoll_##NAME

    enum SDTaskState   task_state   = driver->task.state;
    enum SDDriverState driver_state = driver->state;

    if (task_state == SDTaskState_error)
    {
        // We check for task error first before driver error so that the caller
        // can clear the task error before worrying about if there's any driver errors.
        driver->task.state = SDTaskState_unscheduled;
        ret(cleared_task_error);
    }
    else if (driver_state == SDDriverState_error)
    {
        ret(driver_error); // Something bad happened while working with the SD card.
    }
    else switch (task_state)
    {
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

        case SDTaskState_unscheduled : ret(idle);
        case SDTaskState_done        : ret(idle);
        case SDTaskState_error       : bug;
        default                      : bug;
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

    CMSIS_PUT(SDx_KERNEL_SOURCE, STPY_SDMMC1_KERNEL_SOURCE);



    // Enable the peripheral.

    CMSIS_PUT(SDx_ENABLE, true);



    // Configure the peripheral.

    CMSIS_SET(SDx, POWER  , PWRCTRL , 0b11                        ); // Power the clock.
    CMSIS_SET(SDx, DTIMER , DATATIME, STPY_SDMMC1_INITIAL_DATATIME); // Max timeout period.
    CMSIS_SET
    (
        SDx    , CLKCR                      , // TODO Use PWRSAV?
        HWFC_EN, true                       , // Allow output clock to be halted to prevent FIFO overrun/underrun.
        CLKDIV , STPY_SDMMC1_INITIAL_DIVIDER, // Divide the kernel clock down to the desired bus frequency.
        NEGEDGE, true                       , // Sample on rise, setup on fall. @/pg 307/fig 6.6.6/`SD`.
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



static useret enum SDUpdateOnce : u32
{
    SDUpdateOnce_again,
    SDUpdateOnce_yield,
    SDUpdateOnce_bug,
}
_SD_update_once(enum SDHandle handle)
{

    _EXPAND_HANDLE

    #undef  ret
    #define ret(NAME) return SDUpdateOnce_##NAME

    enum SDCmderUpdate cmder_ret = {0};
    do     cmder_ret = SDCmder_update(SDx, &driver->cmder);
    while (cmder_ret == SDCmderUpdate_again);

    if (cmder_ret == SDCmderUpdate_sdmmc_needs_reinit)
    {
        driver->state = SDDriverState_error;
        driver->error = SDDriverError_cmder_needs_sdmmc_reinited;
    }

    switch (driver->state)
    {
        case SDDriverState_initer: switch (cmder_ret)
        {
            case SDCmderUpdate_ready_for_next_command:
            {
                enum SDIniterUpdate initer_ret = SDIniter_update(&driver->initer);
                switch (initer_ret)
                {
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
                    } break;

                    case SDIniterUpdate_caller_set_bus_width:
                    {
                        CMSIS_SET(SDx, DTIMER, DATATIME, STPY_SDMMC1_FULL_DATATIME); // New max timeout period.
                        CMSIS_SET
                        (
                            SDx  , CLKCR                     ,
                            CLKDIV , STPY_SDMMC1_FULL_DIVIDER, // Divide the kernel clock down to the desired bus frequency.
                            WIDBUS , 0b01                      , // Set peripheral to now use 4-bit wide bus.
                        );
                    } break;

                    case SDIniterUpdate_card_likely_unmounted:
                    {
                        driver->state = SDDriverState_error;
                        driver->error = SDDriverError_card_likely_unmounted;
                    } break;

                    case SDIniterUpdate_card_likely_unsupported:
                    {
                        driver->state = SDDriverState_error;
                        driver->error = SDDriverError_card_likely_unsupported;
                    } break;

                    case SDIniterUpdate_done:
                    {
                        driver->state = SDDriverState_active;
                    } break;

                    case SDIniterUpdate_bug : bug;
                    default                    : bug;
                }
            } break;

            case SDCmderUpdate_command_attempted:
            {
                driver->initer.feedback =
                    (struct SDIniterFeedback)
                    {
                        .cmd_failed = !!driver->cmder.error,
                        .response   = { SDx->RESP1, SDx->RESP2, SDx->RESP3, SDx->RESP4, },
                    };
            } break;

            case SDCmderUpdate_yield              : ret(yield);
            case SDCmderUpdate_again              : bug;
            case SDCmderUpdate_sdmmc_needs_reinit : bug;
            case SDCmderUpdate_bug                : bug;
            default                                  : bug;
        } break;

        case SDDriverState_active: switch (cmder_ret)
        {
            case SDCmderUpdate_ready_for_next_command:
            {
                if (driver->task.state != SDTaskState_booked)
                    ret(yield);
                if (!driver->task.op)
                    ret(bug  );
                if (!driver->task.sector)
                    ret(bug  ); // TODO What if reading/writing no data?

                driver->cmder =
                    (struct SDCmder)
                    {
                        .state      = SDCmderState_scheduled_command,
                        .cmd        = (enum SDCmd) driver->task.op,
                        .arg        = driver->task.addr,
                        .data       = (u8*) driver->task.sector,
                        .remaining  = sizeof(driver->task.sector->data),
                        .block_size = sizeof(driver->task.sector->data),
                        .rca        = driver->initer.rca,
                    };
            } break;

            case SDCmderUpdate_command_attempted:
            {
                if (driver->task.state != SDTaskState_booked)
                    bug;

                if (driver->cmder.error)
                {
                    driver->task.state = SDTaskState_error; // Pass the error to the user to poll.
                }
                else
                {
                    driver->task.state = SDTaskState_done; // Hip-hip hooray! Task finished succesfully!
                }
            } break;

            case SDCmderUpdate_yield              : ret(yield);
            case SDCmderUpdate_again              : bug;
            case SDCmderUpdate_sdmmc_needs_reinit : bug;
            case SDCmderUpdate_bug                : bug;
            default                                  : bug;
        } break;

        case SDDriverState_error : ret(yield);
        default                  : bug;
    }

    ret(again);

}



////////////////////////////////////////////////////////////////////////////////



static void
_SD_update_entirely(enum SDHandle handle)
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
