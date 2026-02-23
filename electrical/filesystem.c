////////////////////////////////////////////////////////////////////////////////



#define FFCONF_DEF         80386 // Current FatFs revision we're using.
#define FF_FS_READONLY     false // Allow the file-system to be mutated.
#define FF_FS_MINIMIZE     0     // "0: Basic functions are fully enabled."
#define FF_USE_FIND        false // For `f_findfirst` and `f_findnext`.
#define FF_USE_MKFS        true  // For `f_mkfs`.
#define FF_USE_EXPAND      false // For `f_expand`.
#define FF_USE_CHMOD       false // For `f_chmod`.
#define FF_USE_LABEL       false // For `f_getlabel` and `f_setlabel`.
#define FF_USE_FORWARD     false // For `f_forward`.
#define FF_USE_STRFUNC     false // For `f_gets`, `f_putc`, `f_puts`, and `f_printf`.
#define FF_USE_FASTSEEK    false // Whether or not to implement the fast-seek optimization.
#define FF_CODE_PAGE       437   // Set OEM code page to U.S (437).
#define FF_FS_RPATH        0     // For relative path features (`f_chdir`, `f_chdrive`, and `f_getcwd`).
#define FF_VOLUMES         1     // We currently only support mounting one file-system.
#define FF_STR_VOLUME_ID   false // For representing volume IDs as arbitrary strings.
#define FF_MULTI_PARTITION false // For supporting multiple volumes on the physical drive.
#define FF_MIN_SS          512   // Minimum sector size in bytes.
#define FF_MAX_SS          512   // Maximum sector size in bytes.
#define FF_LBA64           false // For representing sector addresses as 64-bit integers.
#define FF_USE_TRIM        false // For supporting ATA-TRIM.
#define FF_FS_TINY         false // Whether or not file objects should share a common sector buffer.
#define FF_FS_EXFAT        true  // Whether or not to use ExFAT.
#define FF_USE_LFN         true  // For long-file-name support, as required by exFAT.
#define FF_LFN_UNICODE     2     // Character encoding to be used for the API; value of 2 implies UTF-8.
#define FF_LFN_BUF         255   // Amount of bytes allocated in FILINFO for the long-file-name.
#define FF_SFN_BUF         12    // "                                    for the short-file-name.
#define FF_MAX_LFN         255   // Buffer size for processing long-file-names, where 255 provides full support.
#define FF_PATH_DEPTH      10    // For exFAT, the maximum nested directory chain to be supported.
#define FF_FS_NORTC        true  // Whether or not real-time-clock is unavailable.
#define FF_NORTC_MON       3     // " Default month.
#define FF_NORTC_MDAY      15    // " Default day.
#define FF_NORTC_YEAR      2005  // " Default year.
#define FF_FS_CRTIME       false // For having file creation time be included in FILINFO.
#define FF_FS_NOFSINFO     0b00  // Settings on how we should trust the FSINFO data concerning cluster allocation.
#define FF_FS_LOCK         false // Whether or not locks should be implemented to ensure consistent file operations.
#define FF_FS_REENTRANT    false // Whether or not FatFs should consider reentrancy of common file operations.

static_assert(FF_MIN_SS == FF_MAX_SS && FF_MIN_SS == sizeof(Sector));



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-default"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <deps/FatFs/source/ffunicode.c>
#include <deps/FatFs/source/ff.c>
#pragma GCC diagnostic pop



////////////////////////////////////////////////////////////////////////////////



struct FileSystemDriver
{
    FATFS fatfs;
    i32   file_number;
    char  file_name[16];
    FIL   file_info;
};

static struct FileSystemDriver _FILESYSTEM_driver = {0};



////////////////////////////////////////////////////////////////////////////////



#ifndef FILESYSTEM_PROFILER_ENABLE
#define FILESYSTEM_PROFILER_ENABLE false
#endif
#if FILESYSTEM_PROFILER_ENABLE

    struct FileSystemProfiler
    {

        i64 total_bytes_logged;
        i64 total_time_us;
        u32 previous_timestamp_us;

        struct
        {
            i32 bytes_written;
            i32 time_spent_us;
        } throughput_per_log;

        struct
        {
            i32 count_reinit_success;
            i32 count_save_success;
            i32 count_couldnt_ready_card;
            i32 count_missing_filesystem;
            i32 count_filesystem_full;
            i32 count_fatfs_internal_error;
            i32 count_transfer_error;
            i32 count_bug;

        };

        char report_buffer[1024];

    };

    static struct FileSystemProfiler _FILESYSTEM_profiler = {0};



    static useret char*
    FILESYSTEM_profiler_compile_report(void)
    {

        snprintf_
        (
            _FILESYSTEM_profiler.report_buffer,
            sizeof(_FILESYSTEM_profiler.report_buffer),
            "==== FileSystem Profiler (%s) =="        "\n"
            "reinit_success             : %d"         "\n"
            "save_success               : %d"         "\n"
            "couldnt_ready_card         : %d"         "\n"
            "missing_filesystem         : %d"         "\n"
            "filesystem_full            : %d"         "\n"
            "fatfs_internal_error       : %d"         "\n"
            "transfer_error             : %d"         "\n"
            "bug                        : %d"         "\n"
            "Total bytes logged         : %.2f MiB"   "\n"
            "Overall logging throughput : %.2f MiB/s" "\n"
            "Throughput per log         : %.2f MiB/s" "\n"
            "\n",
            _FILESYSTEM_driver.file_name,
            _FILESYSTEM_profiler.count_reinit_success,
            _FILESYSTEM_profiler.count_save_success,
            _FILESYSTEM_profiler.count_couldnt_ready_card,
            _FILESYSTEM_profiler.count_missing_filesystem,
            _FILESYSTEM_profiler.count_filesystem_full,
            _FILESYSTEM_profiler.count_fatfs_internal_error,
            _FILESYSTEM_profiler.count_transfer_error,
            _FILESYSTEM_profiler.count_bug,
            (f32) _FILESYSTEM_profiler.total_bytes_logged / (1024.0f * 1024.0f),
            (f32) _FILESYSTEM_profiler.total_bytes_logged / (1024.0f * 1024.0f) / ((f32) _FILESYSTEM_profiler.total_time_us / 1'000'000.0f),
            (f32) _FILESYSTEM_profiler.throughput_per_log.bytes_written / (1024.0f * 1024.0f) / ((f32) _FILESYSTEM_profiler.throughput_per_log.time_spent_us / 1'000'000.0f)
        );

        if (_FILESYSTEM_profiler.throughput_per_log.time_spent_us >= 5'000'000)
        {
            _FILESYSTEM_profiler.throughput_per_log.bytes_written /= 2;
            _FILESYSTEM_profiler.throughput_per_log.time_spent_us /= 2;
        }

        return _FILESYSTEM_profiler.report_buffer;

    }

#endif



////////////////////////////////////////////////////////////////////////////////



static useret enum FileSystemDiskInitializeImplementationResult : u32
{
    FileSystemDiskInitializeImplementationResult_ready,
    FileSystemDiskInitializeImplementationResult_yield,
    FileSystemDiskInitializeImplementationResult_driver_error,
    FileSystemDiskInitializeImplementationResult_bug = BUG_CODE,
}
FILESYSTEM_disk_initialize_implementation(BYTE pdrv)
{

    if (pdrv) // "Always zero at single drive system."
        bug;  // We currently don't support multiple file-systems.

    enum SDDriverState observed_state =
        atomic_load_explicit
        (
            &_SD_drivers[pdrv].atomic_state,
            memory_order_acquire // Might need to read the error code.
        );

    switch (observed_state)
    {

        case SDDriverState_initer:
        {
            return FileSystemDiskInitializeImplementationResult_yield;
        } break;

        case SDDriverState_active:
        {
            return FileSystemDiskInitializeImplementationResult_ready;
        } break;

        case SDDriverState_disabled:
        case SDDriverState_error:
        {
            return FileSystemDiskInitializeImplementationResult_driver_error;
        } break;

        default: bug;

    }

}

extern DSTATUS
disk_initialize(BYTE pdrv)
{
    while (true)
    {

        enum FileSystemDiskInitializeImplementationResult result = FILESYSTEM_disk_initialize_implementation(pdrv);

        switch (result)
        {

            case FileSystemDiskInitializeImplementationResult_ready:
            {
                return 0; // We're all good to go!
            } break;

            case FileSystemDiskInitializeImplementationResult_yield:
            {
                // We'll keep on spin-locking until the SD driver is ready...
            } break;

            case FileSystemDiskInitializeImplementationResult_driver_error:
            case FileSystemDiskInitializeImplementationResult_bug:
            default:
            {
                return STA_NOINIT | STA_NODISK; // User needs to reinitialize the SD driver.
            } break;

        }

    }
}



////////////////////////////////////////////////////////////////////////////////



static useret enum FileSystemDiskStatusImplementationResult : u32
{
    FileSystemDiskStatusImplementationResult_still_initializing,
    FileSystemDiskStatusImplementationResult_ready,
    FileSystemDiskStatusImplementationResult_driver_error,
    FileSystemDiskStatusImplementationResult_bug = BUG_CODE,
}
FILESYSTEM_disk_status_implementation(BYTE pdrv)
{

    if (pdrv) // "Always zero at single drive system."
        bug;  // We currently don't support multiple file-systems.

    enum SDDriverState observed_state =
        atomic_load_explicit
        (
            &_SD_drivers[pdrv].atomic_state,
            memory_order_acquire // Might need to read the error code.
        );

    switch (observed_state)
    {

        case SDDriverState_initer:
        {
            return FileSystemDiskStatusImplementationResult_still_initializing;
        } break;

        case SDDriverState_active:
        {
            return FileSystemDiskStatusImplementationResult_ready;
        } break;

        case SDDriverState_disabled:
        case SDDriverState_error:
        {
            return FileSystemDiskStatusImplementationResult_driver_error;
        } break;

        default: bug;

    }

}

extern DSTATUS
disk_status(BYTE pdrv)
{

    enum FileSystemDiskStatusImplementationResult result = FILESYSTEM_disk_status_implementation(pdrv);

    switch (result)
    {

        case FileSystemDiskStatusImplementationResult_still_initializing:
        {
            return STA_NOINIT;
        } break;

        case FileSystemDiskStatusImplementationResult_ready:
        {
            return 0;
        } break;

        case FileSystemDiskStatusImplementationResult_driver_error:
        case FileSystemDiskStatusImplementationResult_bug:
        default:
        {
            return STA_NOINIT | STA_NODISK; // User needs to reinitialize the SD driver.
        } break;

    }

}



////////////////////////////////////////////////////////////////////////////////



static useret enum FileSystemDiskTransferImplementationResult : u32
{
    FileSystemDiskTransferImplementationResult_success,
    FileSystemDiskTransferImplementationResult_still_initializing,
    FileSystemDiskTransferImplementationResult_transfer_error,
    FileSystemDiskTransferImplementationResult_driver_error,
    FileSystemDiskTransferImplementationResult_bug = BUG_CODE,
}
FILESYSTEM_disk_transfer_implementation(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count, b32 writing)
{

    if (pdrv) // "Always zero at single drive system."
        bug;  // We currently don't support multiple file-systems.

    struct SDDoJob job =
        {
            .handle              = (enum SDHandle) { 0 }, // We're assuming the first SD driver handle.
            .writing             = !!writing,
            .consecutive_caching = true,
            .sector              = (Sector*) buff,
            .address             = sector,
            .count               = (i32) count,
        };

    while (true)
    {

        enum SDDoResult do_result = SD_do(&job);

        switch (do_result)
        {

            case SDDoResult_still_initializing:
            {
                return FileSystemDiskTransferImplementationResult_still_initializing;
            } break;

            case SDDoResult_working:
            {
                // The job is being handled...
            } break;

            case SDDoResult_success:
            {
                return FileSystemDiskTransferImplementationResult_success;
            } break;

            case SDDoResult_transfer_error:
            {
                return FileSystemDiskTransferImplementationResult_transfer_error;
            } break;

            case SDDoResult_card_likely_unmounted:
            case SDDoResult_unsupported_card:
            case SDDoResult_maybe_bus_problem:
            case SDDoResult_voltage_check_failed:
            case SDDoResult_could_not_ready_card:
            case SDDoResult_card_glitch:
            {
                return FileSystemDiskTransferImplementationResult_driver_error;
            } break;

            case SDDoResult_bug : bug;
            default             : bug;

        }

    }

}

extern DRESULT
disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{

    enum FileSystemDiskTransferImplementationResult result = FILESYSTEM_disk_transfer_implementation(pdrv, buff, sector, count, true);

    switch (result)
    {

        case FileSystemDiskTransferImplementationResult_still_initializing:
        {
            return RES_NOTRDY;
        } break;

        case FileSystemDiskTransferImplementationResult_success:
        {
            return RES_OK;
        } break;

        case FileSystemDiskTransferImplementationResult_transfer_error:
        case FileSystemDiskTransferImplementationResult_driver_error:
        case FileSystemDiskTransferImplementationResult_bug:
        default:
        {
            return RES_ERROR;
        } break;

    }

}

extern DRESULT
disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{

    enum FileSystemDiskTransferImplementationResult result = FILESYSTEM_disk_transfer_implementation(pdrv, buff, sector, count, false);

    switch (result)
    {

        case FileSystemDiskTransferImplementationResult_still_initializing:
        {
            return RES_NOTRDY;
        } break;

        case FileSystemDiskTransferImplementationResult_success:
        {
            return RES_OK;
        } break;

        case FileSystemDiskTransferImplementationResult_transfer_error:
        case FileSystemDiskTransferImplementationResult_driver_error:
        case FileSystemDiskTransferImplementationResult_bug:
        default:
        {
            return RES_ERROR;
        } break;

    }

}



////////////////////////////////////////////////////////////////////////////////



static useret enum FileSystemDiskIOCTLImplementationResult : u32
{
    FileSystemDiskIOCTLImplementationResult_success,
    FileSystemDiskIOCTLImplementationResult_still_initializing,
    FileSystemDiskIOCTLImplementationResult_transfer_error,
    FileSystemDiskIOCTLImplementationResult_driver_error,
    FileSystemDiskIOCTLImplementationResult_unsupported_command,
    FileSystemDiskIOCTLImplementationResult_bug = BUG_CODE,
}
FILESYSTEM_disk_ioctl_implementation(BYTE pdrv, BYTE cmd, void* buff)
{

    if (pdrv) // "Always zero at single drive system."
        bug;  // We currently don't support multiple file-systems.

    switch (cmd)
    {


        // "Makes sure that the device has finished pending write process.
        // If the disk I/O layer or storage device has a write-back cache,
        // the dirty cache data must be committed to the medium immediately.
        // Nothing to do for this command if each write operation to the medium
        // is completed in the disk_write function."
        //
        // @/url:`elm-chan.org/fsw/ff/doc/dioctl.html`.

        case CTRL_SYNC:
        {

            struct SDDoJob job =
                {
                    .handle              = (enum SDHandle) { 0 }, // We're assuming the first SD driver handle.
                    .consecutive_caching = false,
                    .count               = 0,
                };

            while (true)
            {

                enum SDDoResult do_result = SD_do(&job);

                switch (do_result)
                {

                    case SDDoResult_still_initializing:
                    {
                        return FileSystemDiskIOCTLImplementationResult_still_initializing;
                    } break;

                    case SDDoResult_working:
                    {
                        // Still busy syncing...
                    } break;

                    case SDDoResult_success:
                    {
                        return FileSystemDiskIOCTLImplementationResult_success;
                    } break;

                    case SDDoResult_transfer_error:
                    {
                        return FileSystemDiskIOCTLImplementationResult_transfer_error;
                    } break;

                    case SDDoResult_card_likely_unmounted:
                    case SDDoResult_unsupported_card:
                    case SDDoResult_maybe_bus_problem:
                    case SDDoResult_voltage_check_failed:
                    case SDDoResult_could_not_ready_card:
                    case SDDoResult_card_glitch:
                    case SDDoResult_bug:
                    {
                        return FileSystemDiskIOCTLImplementationResult_driver_error;
                    } break;

                    default: bug;

                }

            }

        } break;



        // "Retrieves number of available sectors (the largest allowable LBA + 1)
        // on the drive into the LBA_t variable that pointed by buff.
        // This command is used by f_mkfs and f_fdisk function to determine
        // the size of volume/partition to be created."
        //
        // @/url:`elm-chan.org/fsw/ff/doc/dioctl.html`.

        case GET_SECTOR_COUNT:
        {

            *(LBA_t*) buff = (u32) _SD_drivers[pdrv].initer.capacity_sector_count;

            return FileSystemDiskIOCTLImplementationResult_success;

        } break;



        // "Retrieves erase block size in unit of sector of the flash memory media into
        // the DWORD variable that pointed by buff. The allowable value is 1 to 32768 in power of 2.
        // Return 1 if it is unknown or in non-flash memory media. This command is used by f_mkfs
        // function with block size not specified and it attempts to align the data area on the
        // suggested block boundary. Note that FatFs does not have FTL (flash translation layer),
        // so that either disk I/O layter or storage device must have an FTL in it."
        //
        // @/url:`elm-chan.org/fsw/ff/doc/dioctl.html`.

        case GET_BLOCK_SIZE:
        {

            *(DWORD*) buff = 1; // TODO Maybe determine the actual erase block size?

            return FileSystemDiskIOCTLImplementationResult_success;

        } break;



        case GET_SECTOR_SIZE: // When the size of the medium's sector can vary, which we don't support.
        case CTRL_TRIM:       // Stuff for ATA-TRIM, which we don't support.
        default:
        {

            static_assert(FF_MAX_SS == FF_MIN_SS); // @/url:`elm-chan.org/fsw/ff/doc/appnote.html`.
            static_assert(FF_USE_TRIM != 1      ); // "

            return FileSystemDiskIOCTLImplementationResult_unsupported_command;

        } break;

    }

}

extern DRESULT
disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{

    enum FileSystemDiskIOCTLImplementationResult result = FILESYSTEM_disk_ioctl_implementation(pdrv, cmd, buff);

    switch (result)
    {

        case FileSystemDiskIOCTLImplementationResult_success:
        {
            return RES_OK;
        } break;

        case FileSystemDiskIOCTLImplementationResult_still_initializing:
        {
            return RES_NOTRDY;
        } break;

        case FileSystemDiskIOCTLImplementationResult_transfer_error:
        case FileSystemDiskIOCTLImplementationResult_driver_error:
        {
            return RES_ERROR;
        } break;

        case FileSystemDiskIOCTLImplementationResult_unsupported_command:
        {
            return RES_PARERR;
        } break;

        case FileSystemDiskIOCTLImplementationResult_bug:
        default:
        {
            return RES_ERROR;
        } break;

    }

}



////////////////////////////////////////////////////////////////////////////////



static useret enum FileSystemReinitResult : u32
{
    FileSystemReinitResult_success,
    FileSystemReinitResult_couldnt_ready_card,
    FileSystemReinitResult_transfer_error,
    FileSystemReinitResult_missing_filesystem,
    FileSystemReinitResult_fatfs_internal_error,
    FileSystemReinitResult_bug = BUG_CODE,
}
FILESYSTEM_reinit_(enum SDHandle sd_handle)
{

    if (sd_handle != (enum SDHandle) {0})
        bug;  // We currently don't support multiple file-systems.



    // Reinitialize and recover the driver from any error conditions.

    SD_reinit(sd_handle);

    _FILESYSTEM_driver = (struct FileSystemDriver) {0};



    // Clear FatFs's state.

    memzero(&DirBuf);
    memzero(&FatFs);
    memzero(&Fsid);
    memzero(&LfnBuf);



    // Globals not defined by FatFs due to our settings:

    static_assert(FF_CODE_PAGE != 0); // `CodePage`, `DbcTbl`, `ExCvt`.
    static_assert(FF_FS_RPATH  == 0); // `CurrVol`.
    static_assert(!FF_FS_LOCK);       // `Files`.
    static_assert(!FF_FS_REENTRANT);  // `SysLock`, `SysLockVolume`, `Mutex`.



    // Format the SD card with the default file-system.

    #if 0
    {

        FRESULT formatting_result =
            f_mkfs
            (
                "",
                nullptr,
                (Sector) {0},
                sizeof(Sector)
            );

        switch (formatting_result)
        {
            case FR_OK                  : break;
            case FR_DISK_ERR            : return FileSystemReinitResult_transfer_error;
            case FR_NOT_READY           : return FileSystemReinitResult_couldnt_ready_card;
            case FR_INT_ERR             : return FileSystemReinitResult_fatfs_internal_error;
            case FR_WRITE_PROTECTED     : bug; // Shouldn't happen in practice.
            case FR_INVALID_DRIVE       : bug; // "
            case FR_MKFS_ABORTED        : bug; // "
            case FR_INVALID_PARAMETER   : bug; // "
            case FR_NOT_ENOUGH_CORE     : bug; // "
            case FR_NOT_ENABLED         : bug; // Not "valid" return value according to documentation.
            case FR_NO_FILESYSTEM       : bug; // "
            case FR_NO_FILE             : bug; // "
            case FR_NO_PATH             : bug; // "
            case FR_INVALID_NAME        : bug; // "
            case FR_DENIED              : bug; // "
            case FR_EXIST               : bug; // "
            case FR_INVALID_OBJECT      : bug; // "
            case FR_TIMEOUT             : bug; // "
            case FR_LOCKED              : bug; // "
            case FR_TOO_MANY_OPEN_FILES : bug; // "
            default                     : bug; // "
        }

    }
    #endif



    // Mount the file-system.

    FRESULT mounting_result =
        f_mount
        (
            &_FILESYSTEM_driver.fatfs,
            "", // "The string without drive number means the default drive."
            1   // "1: Force mounted the volume to check if it is ready to work."
        );

    switch (mounting_result)
    {
        case FR_OK                  : break;
        case FR_DISK_ERR            : return FileSystemReinitResult_transfer_error;
        case FR_NOT_READY           : return FileSystemReinitResult_couldnt_ready_card;
        case FR_NO_FILESYSTEM       : return FileSystemReinitResult_missing_filesystem;
        case FR_INT_ERR             : return FileSystemReinitResult_fatfs_internal_error;
        case FR_NOT_ENABLED         : bug; // Shouldn't happen in practice...
        case FR_INVALID_DRIVE       : bug; // "
        case FR_NO_FILE             : bug; // Not "valid" return value according to documentation.
        case FR_NO_PATH             : bug; // "
        case FR_INVALID_NAME        : bug; // "
        case FR_DENIED              : bug; // "
        case FR_EXIST               : bug; // "
        case FR_INVALID_OBJECT      : bug; // "
        case FR_WRITE_PROTECTED     : bug; // "
        case FR_MKFS_ABORTED        : bug; // "
        case FR_TIMEOUT             : bug; // "
        case FR_LOCKED              : bug; // "
        case FR_NOT_ENOUGH_CORE     : bug; // "
        case FR_TOO_MANY_OPEN_FILES : bug; // "
        case FR_INVALID_PARAMETER   : bug; // "
        default                     : bug; // "
    }



    // Create the log file.

    for (b32 yield = false; !yield;)
    {



        // Determine file name.

        i32 format_result =
            snprintf_
            (
                _FILESYSTEM_driver.file_name,
                countof(_FILESYSTEM_driver.file_name),
                "%03d.log", // @/`File Name for verifyLogs`.
                _FILESYSTEM_driver.file_number
            );

        if (format_result <= 0)
            bug; // Formatting error..?



        // Try making the file.

        FRESULT creation_result =
            f_open
            (
                &_FILESYSTEM_driver.file_info,
                _FILESYSTEM_driver.file_name,
                FA_WRITE | FA_CREATE_NEW
            );

        switch (creation_result)
        {



            // Made the file successfully; we now synchronize to ensure
            // that the file gets properly committed.

            case FR_OK:
            {

                FRESULT sync_result = f_sync(&_FILESYSTEM_driver.file_info);

                switch (sync_result)
                {

                    case FR_OK:
                    {
                        yield = true;
                    } break;

                    case FR_DISK_ERR            : return FileSystemReinitResult_transfer_error;
                    case FR_INT_ERR             : return FileSystemReinitResult_fatfs_internal_error;
                    case FR_INVALID_OBJECT      : bug; // Shouldn't happen in practice...
                    case FR_TIMEOUT             : bug; // We don't use any thread control features...
                    case FR_NOT_READY           : bug; // Not "valid" return value according to documentation.
                    case FR_NOT_ENABLED         : bug; // "
                    case FR_NO_FILESYSTEM       : bug; // "
                    case FR_INVALID_DRIVE       : bug; // "
                    case FR_NO_FILE             : bug; // "
                    case FR_NO_PATH             : bug; // "
                    case FR_INVALID_NAME        : bug; // "
                    case FR_DENIED              : bug; // "
                    case FR_EXIST               : bug; // "
                    case FR_WRITE_PROTECTED     : bug; // "
                    case FR_MKFS_ABORTED        : bug; // "
                    case FR_LOCKED              : bug; // "
                    case FR_NOT_ENOUGH_CORE     : bug; // "
                    case FR_TOO_MANY_OPEN_FILES : bug; // "
                    case FR_INVALID_PARAMETER   : bug; // "
                    default                     : bug; // "

                }

            } break;



            // There's already a file of the same name;
            // we'll just have to try again with the next name.

            case FR_EXIST:
            {
                _FILESYSTEM_driver.file_number += 1;
            } break;



            case FR_DISK_ERR            : return FileSystemReinitResult_transfer_error;
            case FR_INT_ERR             : return FileSystemReinitResult_fatfs_internal_error;
            case FR_NOT_READY           : bug; // The SD card should've been initialized by now...
            case FR_NOT_ENABLED         : bug; // Shouldn't happen in practice...
            case FR_NO_FILESYSTEM       : bug; // "
            case FR_INVALID_DRIVE       : bug; // "
            case FR_NO_FILE             : bug; // "
            case FR_NO_PATH             : bug; // "
            case FR_INVALID_NAME        : bug; // "
            case FR_DENIED              : bug; // "
            case FR_INVALID_OBJECT      : bug; // "
            case FR_WRITE_PROTECTED     : bug; // "
            case FR_LOCKED              : bug; // "
            case FR_NOT_ENOUGH_CORE     : bug; // "
            case FR_TOO_MANY_OPEN_FILES : bug; // "
            case FR_TIMEOUT             : bug; // We don't use any thread control features...
            case FR_MKFS_ABORTED        : bug; // Not "valid" return value according to documentation.
            case FR_INVALID_PARAMETER   : bug; // "
            default                     : bug; // "

        }

    }



    // We're ready to go!

    return FileSystemReinitResult_success;

}

static useret enum FileSystemReinitResult
FILESYSTEM_reinit(enum SDHandle sd_handle)
{



    // Do stuff.

    enum FileSystemReinitResult result = FILESYSTEM_reinit_(sd_handle);



    // Profiler epilogue.

    #if FILESYSTEM_PROFILER_ENABLE
    {
        switch (result)
        {
            case FileSystemReinitResult_success              : _FILESYSTEM_profiler.count_reinit_success       += 1; break;
            case FileSystemReinitResult_couldnt_ready_card   : _FILESYSTEM_profiler.count_couldnt_ready_card   += 1; break;
            case FileSystemReinitResult_transfer_error       : _FILESYSTEM_profiler.count_transfer_error       += 1; break;
            case FileSystemReinitResult_missing_filesystem   : _FILESYSTEM_profiler.count_missing_filesystem   += 1; break;
            case FileSystemReinitResult_fatfs_internal_error : _FILESYSTEM_profiler.count_fatfs_internal_error += 1; break;
            case FileSystemReinitResult_bug                  : _FILESYSTEM_profiler.count_bug                  += 1; break;
            default                                          : bug;
        }
    }
    #endif



    return result;

}



static useret enum FileSystemSaveResult : u32
{
    FileSystemSaveResult_success,
    FileSystemSaveResult_transfer_error,
    FileSystemSaveResult_filesystem_full,
    FileSystemSaveResult_fatfs_internal_error,
    FileSystemSaveResult_bug = BUG_CODE,
}
FILESYSTEM_save_(enum SDHandle sd_handle, Sector* data, i32 count)
{

    if (sd_handle != (enum SDHandle) {0})
        bug;  // We currently don't support multiple file-systems.

    if (!data)
        bug; // Missing data..?

    if (count <= 0)
        bug; // Why would user transfer no data..?



    UINT    bytes_expected = (u32) (sizeof(*data) * count);
    UINT    bytes_written  = {0};
    FRESULT write_result   =
        f_write
        (
          &_FILESYSTEM_driver.file_info,
          data,
          bytes_expected,
          &bytes_written
        );

    switch (write_result)
    {

        case FR_OK:
        {
            if (bytes_written == bytes_expected)
            {

                // Make sure all of the data has been committed to avoid data loss on media removal.

                FRESULT sync_result = f_sync(&_FILESYSTEM_driver.file_info);

                switch (sync_result)
                {

                    case FR_OK:
                    {
                        return FileSystemSaveResult_success;
                    } break;

                    case FR_DISK_ERR            : return FileSystemSaveResult_transfer_error;
                    case FR_INT_ERR             : return FileSystemSaveResult_fatfs_internal_error;
                    case FR_INVALID_OBJECT      : bug; // Shouldn't happen in practice...
                    case FR_TIMEOUT             : bug; // We don't use any thread control features...
                    case FR_NOT_READY           : bug; // Not "valid" return value according to documentation.
                    case FR_NOT_ENABLED         : bug; // "
                    case FR_NO_FILESYSTEM       : bug; // "
                    case FR_INVALID_DRIVE       : bug; // "
                    case FR_NO_FILE             : bug; // "
                    case FR_NO_PATH             : bug; // "
                    case FR_INVALID_NAME        : bug; // "
                    case FR_DENIED              : bug; // "
                    case FR_EXIST               : bug; // "
                    case FR_WRITE_PROTECTED     : bug; // "
                    case FR_MKFS_ABORTED        : bug; // "
                    case FR_LOCKED              : bug; // "
                    case FR_NOT_ENOUGH_CORE     : bug; // "
                    case FR_TOO_MANY_OPEN_FILES : bug; // "
                    case FR_INVALID_PARAMETER   : bug; // "
                    default                     : bug; // "

                }

            }
            else // The file data write was incomplete...
            {
                return FileSystemSaveResult_filesystem_full;
            }
        } break;

        case FR_DISK_ERR            : return FileSystemSaveResult_transfer_error;
        case FR_INT_ERR             : return FileSystemSaveResult_fatfs_internal_error;
        case FR_INVALID_OBJECT      : bug; // Shouldn't happen in practice...
        case FR_DENIED              : bug; // "
        case FR_TIMEOUT             : bug; // We don't use any thread control features...
        case FR_NOT_READY           : bug; // Not "valid" return value according to documentation.
        case FR_NOT_ENABLED         : bug; // "
        case FR_NO_FILESYSTEM       : bug; // "
        case FR_INVALID_DRIVE       : bug; // "
        case FR_NO_FILE             : bug; // "
        case FR_NO_PATH             : bug; // "
        case FR_INVALID_NAME        : bug; // "
        case FR_EXIST               : bug; // "
        case FR_WRITE_PROTECTED     : bug; // "
        case FR_MKFS_ABORTED        : bug; // "
        case FR_LOCKED              : bug; // "
        case FR_NOT_ENOUGH_CORE     : bug; // "
        case FR_TOO_MANY_OPEN_FILES : bug; // "
        case FR_INVALID_PARAMETER   : bug; // "
        default                     : bug; // "

    }

}

static useret enum FileSystemSaveResult
FILESYSTEM_save(enum SDHandle sd_handle, Sector* data, i32 count)
{



    // Profiler prologue.

    #if FILESYSTEM_PROFILER_ENABLE

        u32 starting_timestamp_us = TIMEKEEPING_COUNTER();

    #endif



    // Do stuff.

    enum FileSystemSaveResult result = FILESYSTEM_save_(sd_handle, data, count);



    // Profiler epilogue.

    #if FILESYSTEM_PROFILER_ENABLE
    {

        u32 ending_timestamp_us = TIMEKEEPING_COUNTER();
        u32 elapsed_us          = ending_timestamp_us - starting_timestamp_us;

        static_assert(sizeof(TIMEKEEPING_COUNTER_TYPE) == sizeof(u32));

        switch (result)
        {
            case FileSystemSaveResult_success              : _FILESYSTEM_profiler.count_save_success         += 1; break;
            case FileSystemSaveResult_filesystem_full      : _FILESYSTEM_profiler.count_filesystem_full      += 1; break;
            case FileSystemSaveResult_transfer_error       : _FILESYSTEM_profiler.count_transfer_error       += 1; break;
            case FileSystemSaveResult_fatfs_internal_error : _FILESYSTEM_profiler.count_fatfs_internal_error += 1; break;
            case FileSystemSaveResult_bug                  : _FILESYSTEM_profiler.count_bug                  += 1; break;
            default                                        : bug;
        }

        if (result == FileSystemSaveResult_success)
        {
            _FILESYSTEM_profiler.total_bytes_logged               += sizeof(*data) * count;
            _FILESYSTEM_profiler.throughput_per_log.bytes_written += sizeof(*data) * count;
            _FILESYSTEM_profiler.throughput_per_log.time_spent_us += (i32) elapsed_us;
        }

        _FILESYSTEM_profiler.total_time_us         += ending_timestamp_us - _FILESYSTEM_profiler.previous_timestamp_us;
        _FILESYSTEM_profiler.previous_timestamp_us  = ending_timestamp_us;

    }
    #endif



    return result;

}
