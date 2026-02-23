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
        b16           consecutive_caching; // @/`SD Consecutive Caching`.
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

    _Atomic enum SDDriverState atomic_state;
    struct SDIniter            initer;
    struct SDCmder             cmder;
    enum SDDriverError         error;

    struct
    {
        _Atomic enum SDDriverJobState atomic_state;
        b16                           writing;
        b16                           consecutive_caching;
        u32                           address;
        Sector*                       sector;
        i32                           count;
    } job;

    i32 consecutive_sector_transfer_count;

};

static struct SDDriver _SD_drivers[SDHandle_COUNT] = {0};



////////////////////////////////////////////////////////////////////////////////



#ifndef SD_PROFILER_ENABLE
#define SD_PROFILER_ENABLE false
#endif
#if SD_PROFILER_ENABLE



    // The SD profiler currently only supports
    // a single SD driver instance at a time.
    // Multiple SD profilers can be implemented,
    // but it's an unlikely use-case.

    static_assert(SDHandle_COUNT == 1);



    struct SDProfiler
    {
        i64 amount_of_bytes_successfully_read;
        i64 amount_of_time_reading_us;
        i64 amount_of_bytes_successfully_written;
        i64 amount_of_time_writing_us;
        u32 starting_timestamp_us;
        struct
        {
            i32 count_still_initializing;
            i32 count_working;
            i32 count_success;
            i32 count_transfer_error;
            i32 count_card_likely_unmounted;
            i32 count_unsupported_card;
            i32 count_maybe_bus_problem;
            i32 count_voltage_check_failed;
            i32 count_could_not_ready_card;
            i32 count_card_glitch;
            i32 count_bug;
        };
        char report_buffer[1024];
    };

    struct SDProfiler _SD_profiler = {0};



    // The SD profiler is good for gauging the approximate maximum
    // read/write throughput of the SD card, but it should not be
    // used for determining the actual *file* read/write speed.
    //
    // This is because the profiler is only measuring how long it
    // takes for read/write of sector(s) to be done, so stuff like
    // the SD card's garbage collection throttling the write speed can
    // be observed, but for something involving the file-system, a
    // profiler working at the file-system abstraction is needed.
    //
    // This SD driver profiler is not differentiating read/writes
    // that is for actual user data versus read/writes that is just
    // for getting the file-system infrastructure to work.

    static useret char*
    SD_profiler_compile_report(void)
    {

        snprintf_
        (
            _SD_profiler.report_buffer,
            sizeof(_SD_profiler.report_buffer),
            "============================="            "\n"
            "count_still_initializing    : %d"         "\n"
            "count_working               : %d"         "\n"
            "count_success               : %d"         "\n"
            "count_transfer_error        : %d"         "\n"
            "count_card_likely_unmounted : %d"         "\n"
            "count_unsupported_card      : %d"         "\n"
            "count_maybe_bus_problem     : %d"         "\n"
            "count_voltage_check_failed  : %d"         "\n"
            "count_could_not_ready_card  : %d"         "\n"
            "count_card_glitch           : %d"         "\n"
            "count_bug                   : %d"         "\n"
            "Average read  throughput    : %.2f KiB/s" "\n"
            "Average write throughput    : %.2f KiB/s" "\n"
            "\n",
            _SD_profiler.count_still_initializing,
            _SD_profiler.count_working,
            _SD_profiler.count_success,
            _SD_profiler.count_transfer_error,
            _SD_profiler.count_card_likely_unmounted,
            _SD_profiler.count_unsupported_card,
            _SD_profiler.count_maybe_bus_problem,
            _SD_profiler.count_voltage_check_failed,
            _SD_profiler.count_could_not_ready_card,
            _SD_profiler.count_card_glitch,
            _SD_profiler.count_bug,
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

        return _SD_profiler.report_buffer;

    }

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
SD_do_(struct SDDoJob* job)
{

    if (!job)
        bug;

    if (job->count <= -1)
        bug; // Non-sensical amount of sectors to transfer...

    if (!iff(job->count, job->sector))
        bug; // Either forgot to provide the source/destination or for some reason isn't transferring any data..?

    if (!implies(job->count == 0, !job->consecutive_caching))
        bug; // Syncing should be the only reason why user should be trying to transfer zero data...



    enum SDHandle handle = job->handle;

    _EXPAND_HANDLE



    // We get the current status of the SD driver.

    enum SDDriverState observed_driver_state =
        atomic_load_explicit
        (
            &driver->atomic_state,
            memory_order_acquire // Acquire for `driver->error` and `driver->job.atomic_state`.
        );

    enum SDDriverJobState observed_job_state =
        atomic_load_explicit
        (
            &driver->job.atomic_state,
            memory_order_relaxed // No synchronization needed.
        );



    // See if the user's job can be processed or is done processing.

    switch (observed_driver_state)
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

            case SDDoJobState_ready_to_be_processed: switch (observed_job_state)
            {

                case SDDriverJobState_ready_for_user:
                {

                    // Fill out the job details.

                    driver->job.writing             = job->writing;
                    driver->job.consecutive_caching = job->consecutive_caching;
                    driver->job.sector              = job->sector;
                    driver->job.address             = job->address;
                    driver->job.count               = job->count;



                    // Release the job details to the SD driver to now handle.

                    atomic_store_explicit
                    (
                        &driver->job.atomic_state,
                        SDDriverJobState_ready_to_be_processed,
                        memory_order_release
                    );

                    NVIC_SET_PENDING(SDx);



                    // The user's job is now in the works.

                    job->state = SDDoJobState_processing;
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
                    driver->job.writing             != job->writing             ||
                    driver->job.consecutive_caching != job->consecutive_caching ||
                    driver->job.sector              != job->sector              ||
                    driver->job.address             != job->address             ||
                    driver->job.count               != job->count
                )
                    bug; // The job that the SD driver is doing doesn't match the user's job...

                switch (observed_job_state)
                {

                    case SDDriverJobState_ready_to_be_processed:
                    case SDDriverJobState_processing:
                    {
                        NVIC_SET_PENDING(SDx); // Ensure the SD driver is working on the job.
                        return SDDoResult_working;
                    } break;

                    case SDDriverJobState_success:
                    {

                        atomic_store_explicit
                        (
                            &driver->job.atomic_state,
                            SDDriverJobState_ready_for_user,
                            memory_order_release // Acknowledge the SD driver's successful completion of the job.
                        );

                        job->state = SDDoJobState_attempted;
                        return SDDoResult_success;

                    } break;

                    case SDDriverJobState_encountered_issue:
                    {

                        atomic_store_explicit
                        (
                            &driver->job.atomic_state,
                            SDDriverJobState_ready_for_user,
                            memory_order_release // Acknowledge the SD driver's failure of the job.
                        );

                        job->state = SDDoJobState_attempted;
                        return SDDoResult_transfer_error;

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

static useret enum SDDoResult
SD_do(struct SDDoJob* job)
{



    // Profiler prologue.

    #if SD_PROFILER_ENABLE
    {
        if (job->state == SDDoJobState_ready_to_be_processed)
        {
            _SD_profiler.starting_timestamp_us = TIMEKEEPING_COUNTER();
        }
    }
    #endif



    // Do stuff.

    enum SDDoResult result = SD_do_(job);



    // Profiler epilogue.

    #if SD_PROFILER_ENABLE
    {

        u32 ending_timestamp_us = TIMEKEEPING_COUNTER();
        u32 elapsed_us          = ending_timestamp_us - _SD_profiler.starting_timestamp_us;

        static_assert(sizeof(TIMEKEEPING_COUNTER_TYPE) == sizeof(u32));

        switch (result)
        {

            case SDDoResult_still_initializing    : _SD_profiler.count_still_initializing    += 1; break;
            case SDDoResult_working               : _SD_profiler.count_working               += 1; break;
            case SDDoResult_success               : _SD_profiler.count_success               += 1; break;
            case SDDoResult_transfer_error        : _SD_profiler.count_transfer_error        += 1; break;
            case SDDoResult_card_likely_unmounted : _SD_profiler.count_card_likely_unmounted += 1; break;
            case SDDoResult_unsupported_card      : _SD_profiler.count_unsupported_card      += 1; break;
            case SDDoResult_maybe_bus_problem     : _SD_profiler.count_maybe_bus_problem     += 1; break;
            case SDDoResult_voltage_check_failed  : _SD_profiler.count_voltage_check_failed  += 1; break;
            case SDDoResult_could_not_ready_card  : _SD_profiler.count_could_not_ready_card  += 1; break;
            case SDDoResult_card_glitch           : _SD_profiler.count_card_glitch           += 1; break;
            case SDDoResult_bug                   : _SD_profiler.count_bug                   += 1; break;
            default                               : _SD_profiler.count_bug                   += 1; break;

        }

        if (result == SDDoResult_success)
        {
            if (job->writing)
            {
                _SD_profiler.amount_of_bytes_successfully_written += sizeof(Sector) * job->count;
                _SD_profiler.amount_of_time_writing_us            += elapsed_us;
            }
            else
            {
                _SD_profiler.amount_of_bytes_successfully_read += sizeof(Sector) * job->count;
                _SD_profiler.amount_of_time_reading_us         += elapsed_us;
            }
        }

    }
    #endif



    return result;

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

    atomic_store_explicit
    (
        &driver->atomic_state,
        SDDriverState_initer,
        memory_order_relaxed
    );

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

    enum SDDriverState observed_driver_state =
        atomic_load_explicit
        (
            &driver->atomic_state,
            memory_order_relaxed
        );

    enum SDDriverJobState observed_job_state =
        atomic_load_explicit
        (
            &driver->job.atomic_state,
            memory_order_acquire // Ensure job detail is filled out completely.
        );

    switch (observed_driver_state)
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

                            atomic_store_explicit
                            (
                                &driver->atomic_state,
                                SDDriverState_active,
                                memory_order_relaxed
                            );

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

                            atomic_store_explicit
                            (
                                &driver->atomic_state,
                                SDDriverState_error,
                                memory_order_relaxed
                            );

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

                    atomic_store_explicit
                    (
                        &driver->atomic_state,
                        SDDriverState_error,
                        memory_order_release // Have the write for the error code be done.
                    );

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



                case SDCmderUpdateResult_ready_for_next_command: switch (observed_job_state)
                {

                    case SDDriverJobState_ready_for_user:
                    case SDDriverJobState_success:
                    case SDDriverJobState_encountered_issue:
                    {
                        return SDUpdateOnceResult_yield; // Nothing for the SD-cmder to do yet.
                    } break;

                    case SDDriverJobState_ready_to_be_processed: // There's an SD transfer we can start doing for the user.
                    {



                        // Determine the amount of blocks to transfer for this command.

                        i32 total_blocks_to_transfer = {0};

                        if (driver->job.consecutive_caching)
                        {
                            // An upper limit of 2^30 sectors is 512 GiB,
                            // which for all intents and purposes,
                            // is an amount of data that the user will never practically reach.
                            total_blocks_to_transfer = (1 << 30);
                        }
                        else
                        {
                             total_blocks_to_transfer = driver->job.count;
                        }



                        // Schedule a command if there's an actual data transfer to be done.

                        if (total_blocks_to_transfer)
                        {

                            enum SDCmd cmd = {0};

                            if (driver->job.consecutive_caching || total_blocks_to_transfer >= 2)
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
                            else
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

                            driver->consecutive_sector_transfer_count = 0;



                            // The SD driver has now picked up the user's job.

                            atomic_store_explicit
                            (
                                &driver->job.atomic_state,
                                SDDriverJobState_processing,
                                memory_order_relaxed // No synchronization needed.
                            );

                            return SDUpdateOnceResult_again;

                        }
                        else // No data transfer associated with this job; we're already done!
                        {

                            atomic_store_explicit
                            (
                                &driver->job.atomic_state,
                                SDDriverJobState_success,
                                memory_order_relaxed // No synchronization needed.
                            );

                            return SDUpdateOnceResult_again;

                        }

                    } break;

                    case SDDriverJobState_processing : bug;
                    default                          : bug;

                } break;



                case SDCmderUpdateResult_need_user_to_provide_next_data_block: switch (observed_job_state)
                {



                    // The SD-cmder is currently doing consecutive read/write
                    // transfer right now, and it just finished transferring
                    // the sector of the current job that we were processing.

                    case SDDriverJobState_processing:
                    {

                        if (driver->job.address != driver->cmder.argument + (u32) driver->consecutive_sector_transfer_count)
                            bug; // SD-cmder's sector address not matching up with current job..?

                        if (!driver->job.consecutive_caching)
                            bug; // Non-consecutive-caching transfers should've ended the data-transfer...

                        atomic_store_explicit
                        (
                            &driver->job.atomic_state,
                            SDDriverJobState_success,
                            memory_order_relaxed // No synchronization needed.
                        );

                        driver->consecutive_sector_transfer_count += driver->job.count;

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
                                driver->cmder.argument + (u32) driver->consecutive_sector_transfer_count
                            );

                        b32 same_transfer_direction =
                            (
                                driver->job.writing
                                    ? SDCmd_WRITE_MULTIPLE_BLOCK
                                    : SDCmd_READ_MULTIPLE_BLOCK
                            ) ==  driver->cmder.cmd;

                        b32 should_process_job =
                            (
                                is_consecutive
                                && same_transfer_direction
                                && driver->job.count >= 1
                            );

                        if (should_process_job)
                        {

                            driver->cmder.data_block_pointer = *driver->job.sector;
                            driver->cmder.data_block_count   = driver->job.count;

                            atomic_store_explicit
                            (
                                &driver->job.atomic_state,
                                SDDriverJobState_processing,
                                memory_order_relaxed // No synchronization needed.
                            );

                        }



                        // Determine whether or not SD-cmder should wrap up the command.

                        if (!should_process_job || !driver->job.consecutive_caching) // @/`SD Consecutive Caching`.
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



                case SDCmderUpdateResult_command_attempted: switch (observed_job_state)
                {



                    // The SD-cmder's data transfer ended while we were handling a job.
                    // This could either be due to the fact that the job's sector was the
                    // last one in the data transfer anyways, or because an error occured.

                    case SDDriverJobState_processing: switch (driver->cmder.error)
                    {

                        case SDCmderError_none:
                        {

                            if (driver->job.address != driver->cmder.argument + (u32) driver->consecutive_sector_transfer_count)
                                bug; // SD-cmder's sector address not matching up with current job..?



                            // The job's sector was indeed the last one in the data transfer;
                            // it just completed successfully is all.

                            atomic_store_explicit
                            (
                                &driver->job.atomic_state,
                                SDDriverJobState_success,
                                memory_order_relaxed // No synchronization needed.
                            );

                            return SDUpdateOnceResult_again;

                        } break;

                        case SDCmderError_command_timeout:
                        case SDCmderError_data_timeout:
                        case SDCmderError_bad_crc:
                        {

                            atomic_store_explicit
                            (
                                &driver->job.atomic_state,
                                SDDriverJobState_encountered_issue,
                                memory_order_relaxed // No synchronization needed.
                            );

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

                            // Previous transfer couldn't conclude correctly for some reason.
                            // An example of this would be doing consecutive transfers, and the user
                            // wants to synchronize the SD card, so we then send the `STOP_TRANSMISSION`,
                            // but it then that command failed.

                            atomic_store_explicit
                            (
                                &driver->job.atomic_state,
                                SDDriverJobState_encountered_issue,
                                memory_order_relaxed // No synchronization needed.
                            );

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

                    atomic_store_explicit
                    (
                        &driver->atomic_state,
                        SDDriverState_error,
                        memory_order_release // Have the write for the error code be done.
                    );

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



// @/`SD Consecutive Caching`:
//
// For maximum throughput, the user should do read/write transfers of sectors
// that are all consecutive in their addresses. This is done by setting the
// field `.consecutive_caching` to `true`.
//
// If `.consecutive_caching` is `false`, then the SD driver will transfer just
// a single sector. In fact, if the user is reading/writing sectors that are
// random-access (i.e. jumping all around the address space), then this will
// be the fastest way to do so.
//
// In most scenarios, however, the user (often through the file-system driver) is
// reading/writing many sectors that are one after another. Rather than issuing
// a new read/write command to transfer a single sector, a single command
// can be issued and many sectors can be transferred all at once without any
// further overhead from reissuing another command. To do this,
// `.consecutive_caching` should be set to `true`.
//
// The caveat, however, is the fact that the SD card may not commit all data
// it receives (that is, when writing sectors) until the data transfer is
// concluded properly. This means if the card is ejected, data might be lost,
// even if the user has stopped writing any new data for a while. This is the
// danger of `.consecutive_caching`. To have the data be all properly flushed
// and synced, the user should schedule another job for the SD driver with
// `.consecutive_caching` set to `false` and `.count` to zero.
//
// It should be noted that syncing of the data does not need to be done for
// read transfers, or for any past transfers that already had
// `.consecutive_caching` set to `false`.
