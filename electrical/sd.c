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

enum SDDriverJobState : u32
{
    SDDriverJobState_ready_for_user,
    SDDriverJobState_ready_to_be_processed,
    SDDriverJobState_processing,
    SDDriverJobState_success,
    SDDriverJobState_encountered_issue,
};

enum SDDoJobState : u32
{
    SDDoJobState_ready_to_be_processed,
    SDDoJobState_processing,
    SDDoJobState_attempted,
};

struct SDDoJob
{
    struct
    {
        enum SDHandle handle;
        b16           writing;
        b16           random_access; // @/`SD Random-Access`.
        u32           address;
        Sector*       sector;
        i32           count;
    };
    struct
    {
        enum SDDoJobState state;
    };
};

struct SDDriver
{

    volatile enum SDDriverState state;
    struct SDIniter             initer;
    struct SDCmder              cmder;
    volatile enum SDDriverError error;

    struct
    {
        volatile enum SDDriverJobState state;
        volatile b16                   writing;
        volatile b16                   random_access;
        volatile u32                   address;
        Sector* volatile               sector;
        volatile i32                   count;
    } job;

    i32 consective_sector_transfer_count;

};

static struct SDDriver _SD_drivers[SDHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



#if SD_PROFILER_ENABLE

    struct SDProfiler
    {
        i64 amount_of_bytes_successfully_read;
        i64 amount_of_time_reading_us;
        i64 amount_of_bytes_successfully_written;
        i64 amount_of_time_writing_us;
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
    _SD_profiler_begin(void)
    {
        _SD_profiler.starting_timestamp_us = TIMEKEEPING_COUNTER();
    }



    static void
    _SD_profiler_end(b32 writing, i32 count)
    {

        u32 ending_timestamp_us = TIMEKEEPING_COUNTER();
        u32 elapsed_us          = ending_timestamp_us - _SD_profiler.starting_timestamp_us;

        static_assert(sizeof(TIMEKEEPING_COUNTER_TYPE) == sizeof(u32));

        if (writing)
        {
            _SD_profiler.amount_of_bytes_successfully_read += sizeof(Sector) * count;
            _SD_profiler.amount_of_time_reading_us         += elapsed_us;
        }
        else
        {
            _SD_profiler.amount_of_bytes_successfully_written += sizeof(Sector) * count;
            _SD_profiler.amount_of_time_writing_us            += elapsed_us;
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
    SDDoResult_transfer_error,
    SDDoResult_still_initializing,
    SDDoResult_working,
    SDDoResult_card_likely_unmounted,
    SDDoResult_unsupported_card,
    SDDoResult_maybe_bus_problem,
    SDDoResult_voltage_check_failed,
    SDDoResult_could_not_ready_card,
    SDDoResult_card_glitch,
    SDDoResult_bug = BUG_CODE,
}
SD_do(struct SDDoJob* job)
{

    if (!job)
        bug;

    if (!job->sector)
        bug; // Source/destination not provided..?

    if (job->count <= 0)
        bug; // Non-sensical amount of sectors to transfer...



    enum SDHandle handle = job->handle;

    _EXPAND_HANDLE



    switch (driver->state)
    {



        // The SD driver is still initializing the SD card.

        case SDDriverState_initer: switch (job->state)
        {
            case SDDoJobState_ready_to_be_processed : return SDDoResult_still_initializing;
            case SDDoJobState_processing            : bug; // There shouldn't be a job already..?
            case SDDoJobState_attempted             : bug; // "
            default                                 : bug;
        } break;



        // The SD driver can work / is working on the user's job.

        case SDDriverState_active: switch (job->state)
        {



            // The user's job hasn't been assigned to the SD driver yet.

            case SDDoJobState_ready_to_be_processed: switch (driver->job.state)
            {

                case SDDriverJobState_ready_for_user:
                {

                    _SD_profiler_begin();

                    job->state = SDDoJobState_processing;

                    driver->job.writing       = job->writing;
                    driver->job.random_access = job->random_access;
                    driver->job.sector        = job->sector;
                    driver->job.address       = job->address;
                    driver->job.count         = job->count;

                    // Release memory.

                    driver->job.state = SDDriverJobState_ready_to_be_processed;

                    NVIC_SET_PENDING(SDx); // Alert SD driver of the new job.

                    return SDDoResult_working;

                } break;

                case SDDriverJobState_ready_to_be_processed : bug; // The SD driver already has a job that the user didn't let conclude..?
                case SDDriverJobState_processing            : bug; // "
                case SDDriverJobState_success               : bug; // "
                case SDDriverJobState_encountered_issue     : bug; // "
                default                                     : bug;

            } break;



            // The user's job should be currently executed by the SD driver.

            case SDDoJobState_processing:
            {

                if
                (
                    driver->job.writing       != job->writing       ||
                    driver->job.random_access != job->random_access ||
                    driver->job.sector        != job->sector        ||
                    driver->job.address       != job->address       ||
                    driver->job.count         != job->count
                )
                    bug; // The job that the SD driver is doing doesn't match the user's job...

                switch (driver->job.state)
                {

                    case SDDriverJobState_ready_to_be_processed:
                    case SDDriverJobState_processing:
                    {
                        NVIC_SET_PENDING(SDx); // Ensure the SD driver is working on the job.
                        return SDDoResult_working;
                    } break;

                    case SDDriverJobState_success:
                    {

                        _SD_profiler_end(job->writing, job->count);

                        driver->job.state = SDDriverJobState_ready_for_user; // Acknowledge the SD driver's completion of the job.
                        job->state        = SDDoJobState_attempted;
                        return SDDoResult_success;                           // The user's job completed without any problems.

                    } break;

                    case SDDriverJobState_encountered_issue:
                    {

                        driver->job.state = SDDriverJobState_ready_for_user; // Acknowledge the SD driver's completion of the job.
                        job->state        = SDDoJobState_attempted;
                        return SDDoResult_transfer_error;                    // Something bad happened...

                    } break;

                    case SDDriverJobState_ready_for_user : bug;
                    default                              : bug;

                }

            } break;



            case SDDoJobState_attempted : bug;
            default                     : bug;

        } break;



        // The SD driver is currently lcoked up.

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
        default                     : bug;

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
        PWRSAV , true                    , // Clock the bus only when it's in use.
    );

    CMSIS_SET
    (
        SDx         , MASK , // Enable most of the interrupts; the SD-cmder will handle them.
        IDMABTCIE   , true , // "
        CKSTOPIE    , true , // "
        VSWENDIE    , true , // "
        ACKTIMEOUTIE, true , // "
        ACKFAILIE   , true , // "
        SDIOITIE    , true , // "
        BUSYD0ENDIE , true , // "
        DABORTIE    , true , // "
        DBCKENDIE   , true , // "
        DHOLDIE     , true , // "
        DATAENDIE   , true , // "
        CMDSENTIE   , true , // "
        CMDRENDIE   , true , // "
        RXOVERRIE   , true , // "
        TXUNDERRIE  , true , // "
        DTIMEOUTIE  , true , // "
        CTIMEOUTIE  , true , // "
        DCRCFAILIE  , true , // "
        CCRCFAILIE  , true , // "
        TXFIFOEIE   , false, // "FIFO interrupt flags must be masked in SDMMC_MASKR when using IDMA mode." @/pg 1010/sec 24.10.11/`H533rm`.
        RXFIFOFIE   , false, // "
        RXFIFOHFIE  , false, // "
        TXFIFOHEIE  , false, // "
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
                                    .state                    = SDCmderState_scheduled_command,
                                    .cmd                      = driver->initer.command.cmd,
                                    .argument                 = driver->initer.command.argument,
                                    .total_blocks_to_transfer = !!driver->initer.command.size,
                                    .bytes_per_block          = driver->initer.command.size,
                                    .data_block_pointer       = driver->initer.command.data,
                                    .data_block_count         = 1,
                                    .rca                      = driver->initer.rca,
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



                case SDCmderUpdateResult_need_user_to_provide_next_data_block : bug;
                case SDCmderUpdateResult_bug                                  : bug;
                default                                                       : bug;

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



                case SDCmderUpdateResult_ready_for_next_command: switch (driver->job.state)
                {

                    case SDDriverJobState_ready_for_user:
                    case SDDriverJobState_success:
                    case SDDriverJobState_encountered_issue:
                    {
                        return SDUpdateOnceResult_yield; // Nothing for the SD-cmder to do yet.
                    } break;

                    case SDDriverJobState_ready_to_be_processed: // There's an SD transfer we can start doing for the user.
                    {

                        if (!driver->job.sector)
                            bug; // No source/destination..?



                        // Determine the amount of blocks to transfer for this command.

                        i32 total_blocks_to_transfer = {0};

                        if (driver->job.random_access)
                        {
                             total_blocks_to_transfer = driver->job.count;
                        }
                        else
                        {
                            // An upper limit of 2^30 sectors is 512 GiB,
                            // which for all intents and purposes,
                            // is an amount of data that the user will never practically reach.
                            total_blocks_to_transfer = (1 << 30);
                        }



                        // Determine the actual SD command to be done.

                        enum SDCmd cmd = {0};

                        if (driver->job.random_access && total_blocks_to_transfer == 1)
                        {
                            if (driver->job.writing)
                            {
                                 cmd = SDCmd_WRITE_BLOCK;
                            }
                            else
                            {
                                 cmd = SDCmd_READ_SINGLE_BLOCK;
                            }
                        }
                        else
                        {
                            if (driver->job.writing)
                            {
                                 cmd = SDCmd_WRITE_MULTIPLE_BLOCK;
                            }
                            else
                            {
                                 cmd = SDCmd_READ_MULTIPLE_BLOCK;
                            }
                        }



                        // Provide SD-cmder with the parameters of the command to carry out.

                        driver->cmder =
                            (struct SDCmder)
                            {
                                .state                    = SDCmderState_scheduled_command,
                                .cmd                      = cmd,
                                .argument                 = driver->job.address,
                                .total_blocks_to_transfer = total_blocks_to_transfer,
                                .bytes_per_block          = sizeof(*driver->job.sector),
                                .data_block_pointer       = *driver->job.sector,
                                .data_block_count         = driver->job.count,
                                .rca                      = driver->initer.rca,
                            };

                        driver->consective_sector_transfer_count = 0;

                        driver->job.state = SDDriverJobState_processing;

                        return SDUpdateOnceResult_again;

                    } break;

                    case SDDriverJobState_processing : bug;
                    default                          : bug;

                } break;



                case SDCmderUpdateResult_need_user_to_provide_next_data_block: switch (driver->job.state)
                {



                    // The SD-cmder is currently doing consecutive read/write
                    // transfer right now, and it just finished transferring
                    // the sector of the current job that we were processing.

                    case SDDriverJobState_processing:
                    {

                        if (driver->job.address != driver->cmder.argument + (u32) driver->consective_sector_transfer_count)
                            bug; // SD-cmder's sector address not matching up with current job..?

                        if (driver->job.random_access)
                            bug; // Random-access transfers shouldn't have continued the data-transfer...

                        driver->job.state                         = SDDriverJobState_success;
                        driver->consective_sector_transfer_count += driver->job.count;

                        return SDUpdateOnceResult_again;

                    } break;



                    // The SD-cmder is currently doing consecutive read/write
                    // transfer right now, and it needs to be given the
                    // source/destination for the next sector. There's no job available
                    // currently, however, so we'll have to wait until the user provides
                    // us with a new job which will have that source/destination pointer.

                    case SDDriverJobState_success:
                    case SDDriverJobState_ready_for_user:
                    {
                        return SDUpdateOnceResult_yield;
                    } break;



                    // There's a new job from the user; we'll see whether or not we should
                    // continue with the current SD command's data transfer. If so, it'll be
                    // a nice performance boost because we don't have to issue a new command.

                    case SDDriverJobState_ready_to_be_processed:
                    {

                        if
                        (
                            driver->cmder.cmd != SDCmd_READ_MULTIPLE_BLOCK &&
                            driver->cmder.cmd != SDCmd_WRITE_MULTIPLE_BLOCK
                        )
                            bug; // Unexpected SD-cmder command...



                        // Determine whether or not we should pass SD-cmder the user's data-block pointer.

                        b32 is_consecutive =
                            (
                                driver->job.address ==
                                driver->cmder.argument + (u32) driver->consective_sector_transfer_count
                            );

                        b32 same_transfer_direction =
                            (
                                driver->job.writing
                                    ? SDCmd_WRITE_MULTIPLE_BLOCK
                                    : SDCmd_READ_MULTIPLE_BLOCK
                            ) ==  driver->cmder.cmd;

                        b32 should_process_job = is_consecutive && same_transfer_direction;

                        if (should_process_job)
                        {
                            driver->cmder.data_block_pointer = *driver->job.sector;
                            driver->cmder.data_block_count   = driver->job.count;
                            driver->job.state                = SDDriverJobState_processing;
                        }



                        // Determine whether or not SD-cmder should wrap up the command.

                        if (!should_process_job || driver->job.random_access) // @/`SD Random-Access`.
                        {
                            driver->cmder.stop_requesting_for_data_blocks = true;
                        }
                        else
                        {
                            // SD-cmder can keep expecting more consecutive data-block transfers.
                        }



                        return SDUpdateOnceResult_again;

                    } break;



                    case SDDriverJobState_encountered_issue : bug;
                    default                                 : bug;

                } break;



                case SDCmderUpdateResult_command_attempted: switch (driver->job.state)
                {



                    // The SD-cmder's data transfer ended while we were handling a job.
                    // This could either be due to the fact that the job's sector was the
                    // last one in the data transfer anyways, or because an error occured.

                    case SDDriverJobState_processing: switch (driver->cmder.error)
                    {

                        case SDCmderError_none:
                        {

                            if (driver->job.address != driver->cmder.argument + (u32) driver->consective_sector_transfer_count)
                                bug; // SD-cmder's sector address not matching up with current job..?



                            // The job's sector was indeed the last one in the data transfer;
                            // it just completed successfully is all.

                            driver->job.state = SDDriverJobState_success;
                            return SDUpdateOnceResult_again;

                        } break;

                        case SDCmderError_command_timeout:
                        case SDCmderError_data_timeout:
                        case SDCmderError_bad_crc:
                        {
                            driver->job.state = SDDriverJobState_encountered_issue; // Something bad happened and the data transfer had to be aborted.
                            return SDUpdateOnceResult_again;
                        } break;

                        default: bug;

                    } break;



                    case SDDriverJobState_ready_to_be_processed: switch (driver->cmder.error)
                    {

                        case SDCmderError_none:
                        {
                            return SDUpdateOnceResult_again; // The job will be handled on the next iteration.
                        } break;

                        case SDCmderError_command_timeout:
                        case SDCmderError_data_timeout:
                        case SDCmderError_bad_crc:
                        {
                            driver->job.state = SDDriverJobState_encountered_issue; // Previous transfer couldn't conclude correctly for some reason.
                            return SDUpdateOnceResult_again;
                        } break;

                        default: bug;

                    } break;



                    case SDDriverJobState_success           : bug;
                    case SDDriverJobState_ready_for_user    : bug;
                    case SDDriverJobState_encountered_issue : bug;
                    default                                 : bug;

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



////////////////////////////////////////////////////////////////////////////////



// @/`SD Random-Access`:
//
// For maximum throughput, hints can be given to the SD driver by indicating
// whether or not the user is wanting to do just a single read or write, or
// if there's going to be many consecutive read/writes.
//
// If the user sets `.random_access`, then the SD driver will assume that after
// the data transfer is done, the address of the next data transfer will be
// somewhere else entirely, and thus can plan for that accordingly.
//
// What this practically means is how the SD driver determines whether or not
// it should use `READ_SINGLE_BLOCK`/`WRITE_BLOCK` or `READ_MULTIPLE_BLOCK`/`WRITE_MULTIPLE_BLOCK`
// for the user's job. The latter pair of commands have the benefit of removing
// the overhead of having to repeatedly send SD commands, but are terrible for
// doing just a single sector read/write.
//
// All in all, `.random_access` is only a hint, and can be ignored entirely.
// It should not impact the correctness of the program, just performance.
// Because of zero-initialization, if `.random_access` is not specified, the
// SD driver assume all read/write transfers are consecutive.
//
// It should also be noted that "consecutive read/write transfers" means
// entirely consisting of reads or entirely consisting of writes;
// no mix and matching.
