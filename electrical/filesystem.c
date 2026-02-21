////////////////////////////////////////////////////////////////////////////////
//
// Some FatFs configurations.
//



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
//
// Routine for FatFs to initialize the storage medium.
//



static enum DiskInitializeImplementationResult : u32
{
    DiskInitializeImplementationResult_ready,
    DiskInitializeImplementationResult_yield,
    DiskInitializeImplementationResult_driver_error,
    DiskInitializeImplementationResult_bug = BUG_CODE,
}
disk_initialize_implementation(BYTE pdrv)
{

    if (pdrv) // "Always zero at single drive system."
        bug;  // We currently don't support multiple file-systems.

    switch (_SD_drivers[pdrv].state)
    {

        case SDDriverState_initer:
        {
            return DiskInitializeImplementationResult_yield;
        } break;

        case SDDriverState_active:
        {
            return DiskInitializeImplementationResult_ready;
        } break;

        case SDDriverState_disabled:
        case SDDriverState_error:
        {
            return DiskInitializeImplementationResult_driver_error;
        } break;

        default: bug;

    }

}

extern DSTATUS
disk_initialize(BYTE pdrv)
{
    while (true)
    {

        enum DiskInitializeImplementationResult result = disk_initialize_implementation(pdrv);

        switch (result)
        {

            case DiskInitializeImplementationResult_ready:
            {
                return 0; // We're all good to go!
            } break;

            case DiskInitializeImplementationResult_yield:
            {
                // We'll keep on spin-locking until the SD driver is ready...
            } break;

            case DiskInitializeImplementationResult_driver_error:
            case DiskInitializeImplementationResult_bug:
            default:
            {
                return STA_NOINIT | STA_NODISK; // User needs to reinitialize the SD driver.
            } break;

        }

    }
}



////////////////////////////////////////////////////////////////////////////////



static enum DiskStatusImplementationResult : u32
{
    DiskStatusImplementationResult_still_initializing,
    DiskStatusImplementationResult_ready,
    DiskStatusImplementationResult_driver_error,
    DiskStatusImplementationResult_bug = BUG_CODE,
}
disk_status_implementation(BYTE pdrv)
{

    if (pdrv) // "Always zero at single drive system."
        bug;  // We currently don't support multiple file-systems.

    switch (_SD_drivers[pdrv].state)
    {

        case SDDriverState_initer:
        {
            return DiskStatusImplementationResult_still_initializing;
        } break;

        case SDDriverState_active:
        {
            return DiskStatusImplementationResult_ready;
        } break;

        case SDDriverState_disabled:
        case SDDriverState_error:
        {
            return DiskStatusImplementationResult_driver_error;
        } break;

        default: bug;

    }

}

extern DSTATUS
disk_status(BYTE pdrv)
{

    enum DiskStatusImplementationResult result = disk_status_implementation(pdrv);

    switch (result)
    {

        case DiskStatusImplementationResult_still_initializing:
        {
            return STA_NOINIT;
        } break;

        case DiskStatusImplementationResult_ready:
        {
            return 0;
        } break;

        case DiskStatusImplementationResult_driver_error:
        case DiskStatusImplementationResult_bug:
        default:
        {
            return STA_NOINIT | STA_NODISK; // User needs to reinitialize the SD driver.
        } break;

    }

}



////////////////////////////////////////////////////////////////////////////////



static enum DiskTransferImplementationResult : u32
{
    DiskTransferImplementationResult_success,
    DiskTransferImplementationResult_still_initializing,
    DiskTransferImplementationResult_transfer_error,
    DiskTransferImplementationResult_driver_error,
    DiskTransferImplementationResult_bug = BUG_CODE,
}
disk_transfer_implementation(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count, b32 writing)
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
                return DiskTransferImplementationResult_still_initializing;
            } break;

            case SDDoResult_working:
            {
                // The job is being handled...
            } break;

            case SDDoResult_success:
            {
                return DiskTransferImplementationResult_success;
            } break;

            case SDDoResult_transfer_error:
            {
                return DiskTransferImplementationResult_transfer_error;
            } break;

            case SDDoResult_card_likely_unmounted:
            case SDDoResult_unsupported_card:
            case SDDoResult_maybe_bus_problem:
            case SDDoResult_voltage_check_failed:
            case SDDoResult_could_not_ready_card:
            case SDDoResult_card_glitch:
            {
                return DiskTransferImplementationResult_driver_error;
            } break;

            case SDDoResult_bug : bug;
            default             : bug;

        }

    }

}

extern DRESULT
disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{

    enum DiskTransferImplementationResult result = disk_transfer_implementation(pdrv, buff, sector, count, true);

    switch (result)
    {

        case DiskTransferImplementationResult_still_initializing:
        {
            return RES_NOTRDY;
        } break;

        case DiskTransferImplementationResult_success:
        {
            return RES_OK;
        } break;

        case DiskTransferImplementationResult_transfer_error:
        case DiskTransferImplementationResult_driver_error:
        case DiskTransferImplementationResult_bug:
        default:
        {
            return RES_ERROR;
        } break;

    }

}

extern DRESULT
disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{

    enum DiskTransferImplementationResult result = disk_transfer_implementation(pdrv, buff, sector, count, false);

    switch (result)
    {

        case DiskTransferImplementationResult_still_initializing:
        {
            return RES_NOTRDY;
        } break;

        case DiskTransferImplementationResult_success:
        {
            return RES_OK;
        } break;

        case DiskTransferImplementationResult_transfer_error:
        case DiskTransferImplementationResult_driver_error:
        case DiskTransferImplementationResult_bug:
        default:
        {
            return RES_ERROR;
        } break;

    }

}



////////////////////////////////////////////////////////////////////////////////



static enum DiskIOCTLImplementationResult : u32
{
    DiskIOCTLImplementationResult_success,
    DiskIOCTLImplementationResult_still_initializing,
    DiskIOCTLImplementationResult_transfer_error,
    DiskIOCTLImplementationResult_driver_error,
    DiskIOCTLImplementationResult_unsupported_command,
    DiskIOCTLImplementationResult_bug = BUG_CODE,
}
disk_ioctl_implementation(BYTE pdrv, BYTE cmd, void* buff)
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
                        return DiskIOCTLImplementationResult_still_initializing;
                    } break;

                    case SDDoResult_working:
                    {
                        // Still busy syncing...
                    } break;

                    case SDDoResult_success:
                    {
                        return DiskIOCTLImplementationResult_success;
                    } break;

                    case SDDoResult_transfer_error:
                    {
                        return DiskIOCTLImplementationResult_transfer_error;
                    } break;

                    case SDDoResult_card_likely_unmounted:
                    case SDDoResult_unsupported_card:
                    case SDDoResult_maybe_bus_problem:
                    case SDDoResult_voltage_check_failed:
                    case SDDoResult_could_not_ready_card:
                    case SDDoResult_card_glitch:
                    case SDDoResult_bug:
                    {
                        return DiskIOCTLImplementationResult_driver_error;
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

            return DiskIOCTLImplementationResult_success;

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

            return DiskIOCTLImplementationResult_success;

        } break;



        case GET_SECTOR_SIZE: // When the size of the medium's sector can vary, which we don't support.
        case CTRL_TRIM:       // Stuff for ATA-TRIM, which we don't support.
        default:
        {

            static_assert(FF_MAX_SS == FF_MIN_SS); // @/url:`elm-chan.org/fsw/ff/doc/appnote.html`.
            static_assert(FF_USE_TRIM != 1      ); // "

            return DiskIOCTLImplementationResult_unsupported_command;

        } break;

    }

}

extern DRESULT
disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{

    enum DiskIOCTLImplementationResult result = disk_ioctl_implementation(pdrv, cmd, buff);

    switch (result)
    {

        case DiskIOCTLImplementationResult_success:
        {
            return RES_OK;
        } break;

        case DiskIOCTLImplementationResult_still_initializing:
        {
            return RES_NOTRDY;
        } break;

        case DiskIOCTLImplementationResult_transfer_error:
        case DiskIOCTLImplementationResult_driver_error:
        {
            return RES_ERROR;
        } break;

        case DiskIOCTLImplementationResult_unsupported_command:
        {
            return RES_PARERR;
        } break;

        case DiskIOCTLImplementationResult_bug:
        default:
        {
            return RES_ERROR;
        } break;

    }

}
