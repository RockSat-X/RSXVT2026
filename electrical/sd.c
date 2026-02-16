#ifndef SD_PROFILER_ENABLE
#define SD_PROFILER_ENABLE false
#endif

#include "sd.meta"
#include "sd_initer.c"
#include "sd_cmder.c"

#include "sdmmc_driver_support.meta"
/* #meta

    IMPLEMENT_DRIVER_SUPPORT(
        driver_type = 'SD',
        cmsis_name  = 'SDMMC',
        common_name = 'SDx',
        expansions  = (('driver', '&_SD_drivers[handle]'),),
        terms       = lambda type, peripheral, handle: (
            ('{}'                      , 'expression' ),
            ('NVICInterrupt_{}'        , 'expression' ),
            ('STPY_{}_KERNEL_SOURCE'   , 'expression' ),
            ('STPY_{}_INITIAL_DIVIDER' , 'expression' ),
            ('STPY_{}_INITIAL_DATATIME', 'expression' ),
            ('STPY_{}_FULL_DIVIDER'    , 'expression' ),
            ('STPY_{}_FULL_DATATIME'   , 'expression' ),
            ('{}_RESET'                , 'cmsis_tuple'),
            ('{}_ENABLE'               , 'cmsis_tuple'),
            ('{}_KERNEL_SOURCE'        , 'cmsis_tuple'),
            ('{}'                      , 'interrupt'  ),
        ),
    )

*/

typedef u8 Sector[512];

enum SDDriverState : u32
{
    SDDriverState_disabled,
    SDDriverState_initer,
    SDDriverState_active,
    SDDriverState_error,
};

enum SDDriverError : u32
{
    SDDriverError_none,
    SDDriverError_card_likely_unmounted,
    SDDriverError_unsupported_card,
    SDDriverError_maybe_bus_problem,
    SDDriverError_voltage_check_failed,
    SDDriverError_could_not_ready_card,
    SDDriverError_card_glitch,
};

enum SDOperation : u32
{
    SDOperation_single_read  = SDCmd_READ_MULTIPLE_BLOCK,
    SDOperation_single_write = SDCmd_WRITE_MULTIPLE_BLOCK,
};

enum SDTaskState : u32
{
    SDTaskState_unscheduled,
    SDTaskState_booked,
    SDTaskState_processing,
    SDTaskState_done,
    SDTaskState_error,
};

struct SDDriver
{
    volatile enum SDDriverState state;
    struct SDIniter             initer;
    struct SDCmder              cmder;
    volatile enum SDDriverError error;

    struct
    {
        volatile enum SDTaskState state;
        volatile enum SDOperation operation;
        Sector* volatile          sector;
        volatile u32              address;
    } task;
};

static struct SDDriver _SD_drivers[SDHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



#if SD_PROFILER_ENABLE

    struct SDProfiler
    {
        u64 amount_of_bytes_successfully_read;
        u64 amount_of_time_reading_us;
        u64 amount_of_bytes_successfully_written;
        u64 amount_of_time_writing_us;
        u32 starting_timestamp_us;
    };

    struct SDProfiler _SD_profiler = {0};



    static void
    SD_profiler_report(void)
    {

        stlink_tx
        (
            "Average read  throughput : %.2f KiB/s" "\n"
            "Average write throughput : %.2f KiB/s" "\n",
            (f32) _SD_profiler.amount_of_bytes_successfully_read    / 1024.0f / ((f32) _SD_profiler.amount_of_time_reading_us / 1'000'000.0f),
            (f32) _SD_profiler.amount_of_bytes_successfully_written / 1024.0f / ((f32) _SD_profiler.amount_of_time_writing_us / 1'000'000.0f)
        );

        if (_SD_profiler.amount_of_time_reading_us >= 5'000'000)
        {
            _SD_profiler.amount_of_bytes_successfully_read /= 2;
            _SD_profiler.amount_of_time_reading_us         /= 2;
        }

        if (_SD_profiler.amount_of_time_writing_us >= 5'000'000)
        {
            _SD_profiler.amount_of_bytes_successfully_written /= 2;
            _SD_profiler.amount_of_time_writing_us            /= 2;
        }

    }



    static void
    _SD_profiler_begin(enum SDOperation operation)
    {
        _SD_profiler.starting_timestamp_us = TIMEKEEPING_COUNTER();
    }



    static void
    _SD_profiler_end(enum SDOperation operation)
    {

        u32 ending_timestamp_us = TIMEKEEPING_COUNTER();
        u32 elapsed_us          = ending_timestamp_us - _SD_profiler.starting_timestamp_us;

        static_assert(sizeof(TIMEKEEPING_COUNTER_TYPE) == sizeof(u32));

        switch (operation)
        {

            case SDOperation_single_read:
            {
                _SD_profiler.amount_of_bytes_successfully_read += sizeof(Sector);
                _SD_profiler.amount_of_time_reading_us         += elapsed_us;
            } break;

            case SDOperation_single_write:
            {
                _SD_profiler.amount_of_bytes_successfully_written += sizeof(Sector);
                _SD_profiler.amount_of_time_writing_us            += elapsed_us;
            } break;

            default: bug;

        }

    }

#else

    #define _SD_profiler_begin(...)
    #define _SD_profiler_end(...)

#endif



////////////////////////////////////////////////////////////////////////////////



static useret enum SDDoResult : u32
{
    SDDoResult_success,
    SDDoResult_task_error,
    SDDoResult_card_likely_unmounted,
    SDDoResult_unsupported_card,
    SDDoResult_maybe_bus_problem,
    SDDoResult_voltage_check_failed,
    SDDoResult_could_not_ready_card,
    SDDoResult_card_glitch,
    SDDoResult_bug = BUG_CODE,
}
SD_do
(
    enum SDHandle    handle,
    enum SDOperation operation,
    Sector*          sector,
    u32              address
)
{

    _EXPAND_HANDLE

    if (!sector)
        bug; // Source/destination not provided..?

    if (driver->task.state != SDTaskState_unscheduled)
        bug; // Unexpected state...



    // Make sure the SD driver is still not initializing.

    for (b32 yield = false; !yield;)
    {
        switch (driver->state)
        {

            case SDDriverState_initer:
            {
                NVIC_SET_PENDING(SDx); // The SD card is still being initialized...
            } break;

            case SDDriverState_active:
            {
                yield = true; // The SD driver is ready to handle tasks now.
            } break;

            case SDDriverState_error:
            {
                yield = true; // No point in further waiting.
            } break;

            case SDDriverState_disabled : bug;
            default                     : bug;

        }
    }



    // Fill out the SD card operation the user would like to do.

    _SD_profiler_begin(operation);

    {

        driver->task.operation = operation;
        driver->task.sector    = sector;
        driver->task.address   = address;

        // Release memory.

        driver->task.state = SDTaskState_booked;

        NVIC_SET_PENDING(SDx); // Alert SD driver of the new task.

    }



    // Block until the task has been handled.

    while (true)
    {
        switch (driver->state)
        {

            case SDDriverState_active: switch (driver->task.state)
            {

                case SDTaskState_booked:
                case SDTaskState_processing:
                {
                    NVIC_SET_PENDING(SDx); // Make sure the SD driver knows what it should be doing.
                } break;

                case SDTaskState_done:
                {

                    // Successfully executed the SD operation!

                    _SD_profiler_end(operation);

                    driver->task.state = SDTaskState_unscheduled;
                    return SDDoResult_success;

                } break;

                case SDTaskState_error:
                {

                    // Something went wrong with the task!

                    driver->task.state = SDTaskState_unscheduled;
                    return SDDoResult_task_error;

                } break;

                case SDTaskState_unscheduled : bug;
                default                      : bug;

            } break;

            case SDDriverState_error: switch (driver->error)
            {
                case SDDriverError_card_likely_unmounted : return SDDoResult_card_likely_unmounted;
                case SDDriverError_unsupported_card      : return SDDoResult_unsupported_card;
                case SDDriverError_maybe_bus_problem     : return SDDoResult_maybe_bus_problem;
                case SDDriverError_voltage_check_failed  : return SDDoResult_voltage_check_failed;
                case SDDriverError_could_not_ready_card  : return SDDoResult_could_not_ready_card;
                case SDDriverError_card_glitch           : return SDDoResult_card_glitch;
                case SDDriverError_none                  : bug;
                default                                  : bug;
            } break;

            case SDDriverState_disabled : bug;
            case SDDriverState_initer   : bug;
            default                     : bug;

        }
    }

}



static void
SD_reinit(enum SDHandle handle)
{

    _EXPAND_HANDLE



    // Reset-cycle the peripheral.

    CMSIS_PUT(SDx_RESET, true );
    CMSIS_PUT(SDx_RESET, false);

    NVIC_DISABLE(SDx);

    *driver = (struct SDDriver) {0};



    // Set the kernel clock source for the peripheral.

    CMSIS_PUT(SDx_KERNEL_SOURCE, STPY_SDx_KERNEL_SOURCE);



    // Enable the peripheral.

    CMSIS_PUT(SDx_ENABLE, true);



    // Configure the peripheral.

    CMSIS_SET(SDx, POWER  , PWRCTRL , 0b11                     ); // Power the clock.
    CMSIS_SET(SDx, DTIMER , DATATIME, STPY_SDx_INITIAL_DATATIME); // Max timeout period.
    CMSIS_SET
    (
        SDx    , CLKCR                   ,
        HWFC_EN, true                    , // Allow output clock to be halted to prevent FIFO overrun/underrun.
        CLKDIV , STPY_SDx_INITIAL_DIVIDER, // Divide the kernel clock down to the desired bus frequency.
        NEGEDGE, true                    , // Sample on rise, setup on fall. @/pg 307/fig 6.6.6/`SD`.
    );

    CMSIS_SET
    (
        SDx         , MASK, // Literally enable all interrupts; the SD-cmder will handle them.
        IDMABTCIE   , true, // "
        CKSTOPIE    , true, // "
        VSWENDIE    , true, // "
        ACKTIMEOUTIE, true, // "
        ACKFAILIE   , true, // "
        SDIOITIE    , true, // "
        BUSYD0ENDIE , true, // "
        DABORTIE    , true, // "
        DBCKENDIE   , true, // "
        DHOLDIE     , true, // "
        DATAENDIE   , true, // "
        CMDSENTIE   , true, // "
        CMDRENDIE   , true, // "
        RXOVERRIE   , true, // "
        TXUNDERRIE  , true, // "
        DTIMEOUTIE  , true, // "
        CTIMEOUTIE  , true, // "
        DCRCFAILIE  , true, // "
        CCRCFAILIE  , true, // "
    );



    // Further SD card initialization is done within the interrupt handler.

    driver->state = SDDriverState_initer;

    NVIC_ENABLE(SDx);
    NVIC_SET_PENDING(SDx);

}



////////////////////////////////////////////////////////////////////////////////



static useret enum SDUpdateOnceResult : u32
{
    SDUpdateOnceResult_again,
    SDUpdateOnceResult_yield,
    SDUpdateOnceResult_bug = BUG_CODE,
}
_SD_update_once(enum SDHandle handle)
{

    _EXPAND_HANDLE

    switch (driver->state)
    {



        ////////////////////////////////////////
        //
        // The SD card needs to be initialized.
        //
        ////////////////////////////////////////

        case SDDriverState_initer:
        {

            enum SDCmderUpdateResult cmder_result = SDCmder_update(SDx, &driver->cmder);

            switch (cmder_result)
            {

                case SDCmderUpdateResult_yield:
                {
                    return SDUpdateOnceResult_yield; // The SD interface still busy...
                } break;

                case SDCmderUpdateResult_ready_for_next_command:
                {

                    enum SDIniterUpdateResult initer_result = SDIniter_update(&driver->initer);

                    switch (initer_result)
                    {

                        // SD-inter provides SD-cmder with the next command to execute.

                        case SDIniterUpdateResult_execute_command:
                        {

                            driver->cmder =
                                (struct SDCmder)
                                {
                                    .state      = SDCmderState_scheduled_command,
                                    .cmd        = driver->initer.command.cmd,
                                    .argument   = driver->initer.command.argument,
                                    .block_data = driver->initer.command.data,
                                    .total_size = driver->initer.command.size,
                                    .block_size = driver->initer.command.size,
                                    .rca        = driver->initer.rca,
                                };

                            return SDUpdateOnceResult_again;

                        } break;



                        // The SD-initer says we can increase the bus width width now.
                        // At this point, we can also increase the bus speed.

                        case SDIniterUpdateResult_user_set_bus_width:
                        {

                            CMSIS_SET(SDx, DTIMER, DATATIME, STPY_SDx_FULL_DATATIME); // New max timeout period.
                            CMSIS_SET
                            (
                                SDx   , CLKCR                ,
                                CLKDIV, STPY_SDx_FULL_DIVIDER, // Divide the kernel clock down to the desired bus frequency.
                                WIDBUS, 0b01                 , // Set peripheral to now use 4-bit wide bus.
                            );

                            driver->initer.feedback =
                                (struct SDIniterFeedback)
                                {
                                    .failed = false,
                                };

                            return SDUpdateOnceResult_again;

                        } break;



                        // Hip hip hooray! We've finished initializing the SD card!

                        case SDIniterUpdateResult_done:
                        {
                            driver->state = SDDriverState_active;
                            return SDUpdateOnceResult_again;
                        } break;



                        // The SD-initer encountered an issue trying to initialize the SD card.

                        {
                            case SDIniterUpdateResult_card_likely_unmounted : driver->error = SDDriverError_card_likely_unmounted; goto SD_INITER_ERROR;
                            case SDIniterUpdateResult_unsupported_card      : driver->error = SDDriverError_unsupported_card;      goto SD_INITER_ERROR;
                            case SDIniterUpdateResult_maybe_bus_problem     : driver->error = SDDriverError_maybe_bus_problem;     goto SD_INITER_ERROR;
                            case SDIniterUpdateResult_voltage_check_failed  : driver->error = SDDriverError_voltage_check_failed;  goto SD_INITER_ERROR;
                            case SDIniterUpdateResult_could_not_ready_card  : driver->error = SDDriverError_could_not_ready_card;  goto SD_INITER_ERROR;
                            case SDIniterUpdateResult_card_glitch           : driver->error = SDDriverError_card_glitch;           goto SD_INITER_ERROR;
                            SD_INITER_ERROR:;

                            driver->state = SDDriverState_error;

                            return SDUpdateOnceResult_again;

                        } break;



                        case SDIniterUpdateResult_bug : bug;
                        default                       : bug;

                    }

                } break;

                case SDCmderUpdateResult_waiting_for_user_data:
                {
                    sorry
                } break;

                case SDCmderUpdateResult_command_attempted:
                {

                    driver->initer.feedback = // Provide the SD-initer with the results from SD-cmder.
                        (struct SDIniterFeedback)
                        {
                            .failed   = !!driver->cmder.error,
                            .response =
                                {
                                    SDx->RESP1,
                                    SDx->RESP2,
                                    SDx->RESP3,
                                    SDx->RESP4,
                                },
                        };

                    return SDUpdateOnceResult_again;

                } break;

                case SDCmderUpdateResult_card_glitch:
                {
                    driver->error = SDDriverError_card_glitch;
                    driver->state = SDDriverState_error;
                    return SDUpdateOnceResult_again;
                } break;

                case SDCmderUpdateResult_bug : bug;
                default                      : bug;

            }

        } break;



        ////////////////////////////////////////
        //
        // The SD card is ready to be used.
        //
        ////////////////////////////////////////

        case SDDriverState_active:
        {

            enum SDCmderUpdateResult cmder_result = SDCmder_update(SDx, &driver->cmder);

            switch (cmder_result)
            {

                case SDCmderUpdateResult_yield:
                {
                    return SDUpdateOnceResult_yield; // The SD interface still busy...
                } break;

                case SDCmderUpdateResult_ready_for_next_command: switch (driver->task.state)
                {

                    case SDTaskState_unscheduled:
                    case SDTaskState_done:
                    case SDTaskState_error:
                    {
                        return SDUpdateOnceResult_yield; // Nothing for the SD driver to do yet.
                    } break;

                    case SDTaskState_booked:
                    {

                        if (!driver->task.operation)
                            bug; // Unspecified SD operation..?

                        if (!driver->task.sector)
                            bug; // No source/destination..?



                        // Execute the command to carry out the user's desired operation.

                        driver->cmder =
                            (struct SDCmder)
                            {
                                .state      = SDCmderState_scheduled_command,
                                .cmd        = (enum SDCmd) driver->task.operation,
                                .argument   = driver->task.address,
                                .block_data = *driver->task.sector,
                                .total_size = sizeof(*driver->task.sector) * 65535, // TODO.
                                .block_size = sizeof(*driver->task.sector),
                                .rca        = driver->initer.rca,
                            };

                        driver->task.state = SDTaskState_processing;

                        return SDUpdateOnceResult_again;

                    } break;

                    case SDTaskState_processing : bug;
                    default                     : bug;

                } break;



                case SDCmderUpdateResult_waiting_for_user_data: switch (driver->task.state)
                {

                    case SDTaskState_unscheduled:
                    {
                        return SDUpdateOnceResult_yield;
                    } break;

                    case SDTaskState_booked:
                    {

                        if (!driver->task.operation)
                            bug; // Unspecified SD operation..?

                        if (!driver->task.sector)
                            bug; // No source/destination..?



                        if
                        (
                            (enum SDCmd) driver->task.operation == driver->cmder.cmd &&
                            driver->task.address   == driver->cmder.argument
                        )
                        {
                            driver->cmder.block_data = *driver->task.sector;
                            driver->task.state       = SDTaskState_processing;
                        }
                        else
                        {
                            driver->cmder.now_stop_transferring = true;
                        }

                        return SDUpdateOnceResult_again;

                    } break;

                    case SDTaskState_processing:
                    {
                        driver->task.state      = SDTaskState_done;
                        driver->cmder.argument += 1;
                        return SDUpdateOnceResult_yield;
                    } break;

                    case SDTaskState_done:
                    {
                        return SDUpdateOnceResult_yield;
                    } break;

                    case SDTaskState_error:
                    {
                        return SDUpdateOnceResult_yield;
                    } break;

                    default: bug;

                } break;


                //case SDCmderUpdateResult_waiting_for_user_data:
                //{
                //    driver->cmder.now_stop_transferring = true;
                //    return SDUpdateOnceResult_again;
                //} break;

                case SDCmderUpdateResult_command_attempted:
                {

                    //if (driver->task.state != SDTaskState_processing)
                    //    bug; // There should've been an SD operation that we were doing...

                    //switch (driver->cmder.error)
                    //{



                    //    // The SD task was carried out successfully!

                    //    case SDCmderError_none:
                    //    {
                    //        driver->task.state = SDTaskState_done;
                    //        return SDUpdateOnceResult_again;
                    //    } break;



                    //    // Something weird happened; indicate it to the user.

                    //    case SDCmderError_command_timeout:
                    //    case SDCmderError_data_timeout:
                    //    case SDCmderError_bad_crc:
                    //    {
                    //        driver->task.state = SDTaskState_error;
                    //        return SDUpdateOnceResult_again;
                    //    } break;



                    //    default: bug;

                    //}

                    if (driver->task.state == SDTaskState_processing)
                    {
                        driver->error = SDDriverError_card_glitch;
                        driver->state = SDDriverState_error;
                        return SDUpdateOnceResult_again;
                    }
                    else
                    {
                        return SDUpdateOnceResult_again;
                    }

                } break;

                case SDCmderUpdateResult_card_glitch:
                {
                    driver->error = SDDriverError_card_glitch;
                    driver->state = SDDriverState_error;
                    return SDUpdateOnceResult_again;
                } break;

                case SDCmderUpdateResult_bug : bug;
                default                      : bug;

            }

        } break;



        ////////////////////////////////////////
        //
        // The SD driver is stuck in the error condition
        // until the user reinitializes everything.
        //
        ////////////////////////////////////////

        case SDDriverState_error:
        {

            if (!driver->error)
                bug; // Error not set..?

            NVIC_DISABLE(SDx);

            return SDUpdateOnceResult_yield;

        } break;



        case SDDriverState_disabled : bug;
        default                     : bug;

    }

}



static void
_SD_driver_interrupt(enum SDHandle handle)
{

    _EXPAND_HANDLE

    for (b32 yield = false; !yield;)
    {

        enum SDUpdateOnceResult result = _SD_update_once(handle);

        switch (result)
        {

            case SDUpdateOnceResult_again:
            {
                // The state-machine will be updated again.
            } break;

            case SDUpdateOnceResult_yield:
            {
                yield = true; // We can stop updating the state-machine for now.
            } break;

            case SDUpdateOnceResult_bug:
            default:
            {

                // Something weird happen;
                // we need to shut the SD peripheral down
                // and make the user reset everything.

                CMSIS_PUT(SDx_RESET, true);
                NVIC_DISABLE(SDx);

                *driver = (struct SDDriver) {0};

                yield = true;

            } break;

        }

    }

}
