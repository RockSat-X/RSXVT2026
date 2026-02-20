#define FFCONF_DEF         80386
#define FF_FS_READONLY     0
#define FF_FS_MINIMIZE     0
#define FF_USE_FIND        0
#define FF_USE_MKFS        1
#define FF_USE_FASTSEEK    0
#define FF_USE_EXPAND      0
#define FF_USE_CHMOD       0
#define FF_USE_LABEL       0
#define FF_USE_FORWARD     0
#define FF_USE_STRFUNC     0
#define FF_PRINT_LLI       0
#define FF_PRINT_FLOAT     0
#define FF_STRF_ENCODE     0
#define FF_CODE_PAGE       932
#define FF_USE_LFN         0
#define FF_MAX_LFN         255
#define FF_LFN_UNICODE     0
#define FF_LFN_BUF         255
#define FF_SFN_BUF         12
#define FF_FS_RPATH        0
#define FF_PATH_DEPTH      10
#define FF_VOLUMES         1
#define FF_STR_VOLUME_ID   0
#define FF_VOLUME_STRS     "RAM","NAND","CF","SD","SD2","USB","USB2","USB3"
#define FF_MULTI_PARTITION 0
#define FF_MIN_SS          512
#define FF_MAX_SS          512
#define FF_LBA64           0
#define FF_MIN_GPT         0x10000000
#define FF_USE_TRIM        0
#define FF_FS_TINY         0
#define FF_FS_EXFAT        0
#define FF_FS_NORTC        1
#define FF_NORTC_MON       1
#define FF_NORTC_MDAY      1
#define FF_NORTC_YEAR      2025
#define FF_FS_CRTIME       0
#define FF_FS_NOFSINFO     0
#define FF_FS_LOCK         0
#define FF_FS_REENTRANT    0
#define FF_FS_TIMEOUT      1000



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-default"
#include <deps/FatFs/source/ff.c>
#pragma GCC diagnostic pop



////////////////////////////////////////////////////////////////////////////////



extern DSTATUS
disk_initialize(BYTE pdrv)
{

    if (pdrv)  // "Always zero at single drive system."
        panic; // We currently don't support multiple file-systems.

    while (true)
    {
        switch (_SD_drivers[pdrv].state)
        {

            case SDDriverState_initer:
            {
                // Keep waiting for the SD driver to finish initializing...
            } break;

            case SDDriverState_active:
            {
                return 0; // The SD driver is ready for transfers.
            } break;

            case SDDriverState_disabled:
            case SDDriverState_error:
            {
                return STA_NOINIT; // The user needs to reinitialize the SD driver...
            } break;

            default: panic;

        }
    }

}



////////////////////////////////////////////////////////////////////////////////



extern DSTATUS
disk_status(BYTE pdrv)
{

    if (pdrv)  // "Always zero at single drive system."
        panic; // We currently don't support multiple file-systems.

    switch (_SD_drivers[pdrv].state)
    {

        case SDDriverState_initer:
        {
            return STA_NOINIT; // "Indicates that the device has not been initialized and not ready to work."
        } break;

        case SDDriverState_active:
        {
            return 0; // Everything's all good.
        } break;

        case SDDriverState_disabled:
        case SDDriverState_error:
        {
            return STA_NOINIT; // Something went wrong with the SD driver...
        } break;

        default: panic;

    }

}



////////////////////////////////////////////////////////////////////////////////



static DRESULT
disk_transfer(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count, b32 writing)
{

    if (pdrv)  // "Always zero at single drive system."
        panic; // We currently don't support multiple file-systems.



    struct SDDoJob job =
        {
            .handle        = (enum SDHandle) { 0 }, // We're assuming the first SD driver handle.
            .writing       = !!writing,
            .random_access = false, // TODO Have a heuristic?
            .sector        = (Sector*) buff,
            .address       = sector,
            .count         = (i32) count,
        };

    while (true)
    {

        enum SDDoResult do_result = SD_do(&job);

        switch (do_result)
        {

            case SDDoResult_still_initializing:
            {

                // `RES_NOTRDY` : "The device has not been initialized."
                //
                // Although it seems like FatFs does not make use of `RES_NOTRDY`,
                // so we could probably also return `RES_ERROR` here too since I expect
                // `disk_initialize` to have the SD driver be fully initialized by then.
                //
                // @/url:`elm-chan.org/fsw/ff/doc/dread.html`.

                return RES_NOTRDY;

            } break;



            case SDDoResult_working:
            {
                // The job is being handled...
            } break;



            case SDDoResult_success:
            {
                return RES_OK; // Yippee!
            } break;



            case SDDoResult_transfer_error:
            case SDDoResult_card_likely_unmounted:
            case SDDoResult_unsupported_card:
            case SDDoResult_maybe_bus_problem:
            case SDDoResult_voltage_check_failed:
            case SDDoResult_could_not_ready_card:
            case SDDoResult_card_glitch:
            case SDDoResult_bug:
            {
                return RES_ERROR; // "An unrecoverable hard error occured during the read operation."
            } break;



            default: panic;

        }

    }

}

extern DRESULT
disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
    return disk_transfer(pdrv, buff, sector, count, true);
}

extern DRESULT
disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
    return disk_transfer(pdrv, buff, sector, count, false);
}



////////////////////////////////////////////////////////////////////////////////



extern DRESULT
disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{

    if (pdrv)  // "Always zero at single drive system."
        panic; // We currently don't support multiple file-systems.

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
            return RES_OK; // All writes are completed within `disk_write`.
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

            return RES_OK;

        } break;



        case GET_SECTOR_SIZE:
        {
            sorry
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

            return RES_OK;

        } break;



        case CTRL_TRIM:
        {
            sorry
        } break;



        default:
        {
            return RES_PARERR; // "The command code or parameter is invalid."
        } break;

    }

}



////////////////////////////////////////////////////////////////////////////////



struct f_PrintfCallbackContext
{
    FIL*    fp;
    FRESULT result;
};

static void
f_printf_callback(char character, void* void_context)
{

    struct f_PrintfCallbackContext* context = void_context;

    if (context->result == FR_OK)
    {

        // "If the return value is equal to btw, the function return code should be FR_OK."
        //
        // That is, if `f_write` for some reason wrote less than it should,
        // this implies an error condition in the `FRESULT` return value.
        //
        // @/url:`elm-chan.org/fsw/ff/doc/write.html`.

        UINT bytes_written = {0};

        context->result =
            f_write
            (
                context->fp,
                &character,
                sizeof(character),
                &bytes_written
            );

    }
    else
    {
        // An error condition has happened;
        // no longer trying to do any writes.
    }

}

extern int __attribute__((format(printf, 2, 3)))
f_printf(FIL* fp, const TCHAR* fmt, ...)
{

    va_list arguments = {0};
    va_start(arguments);

    struct f_PrintfCallbackContext context =
        {
            .fp     = fp,
            .result = FR_OK,
        };

    int formatting_result =
        vfctprintf
        (
            &f_printf_callback,
            &context,
            fmt,
            arguments
        );

    va_end(arguments);

    if (context.result == FR_OK)
    {

        // "When the string was written successfuly,
        // it returns number of character encoding units written to the file."
        //
        // If `vfctprintf` itself failed, it would've returned a negative number.
        //
        // @/url:`elm-chan.org/fsw/ff/doc/printf.html`.

        return formatting_result;

    }
    else
    {

        // "When the function failed due to disk full or an error,
        // a negative value will be returned."
        // @/url:`elm-chan.org/fsw/ff/doc/printf.html`.

        return -1;

    }

}
