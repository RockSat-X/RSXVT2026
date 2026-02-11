// TODO Ensure there's an SD driver?

#define FFCONF_DEF         80386
#define FF_FS_READONLY     0
#define FF_FS_MINIMIZE     0
#define FF_USE_FIND        0
#define FF_USE_MKFS        0
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



extern DSTATUS
disk_initialize(BYTE pdrv)
{

    sorry

#if 0
    if (pdrv != 0)
        sorry

    while (_SD_drivers[0].state == SDDriverState_initer); // TODO What if no SD_reinit?

    switch (_SD_drivers[0].state)
    {
        case SDDriverState_active:
        {
            return 0;
        } break;

        case SDDriverState_error:
        {
            return STA_NOINIT;
        } break;

        case SDDriverState_initer : sorry
        default                   : sorry
    }

#endif
}



extern DSTATUS
disk_status(BYTE pdrv)
{

    sorry

#if 0
    if (pdrv != 0)
        sorry

    switch (_SD_drivers[0].state)
    {

        case SDDriverState_initer:
        {
            sorry
        } break;

        case SDDriverState_active:
        {
            return 0;
        } break;

        case SDDriverState_error:
        {
            sorry
        } break;

        default: sorry;

    }
#endif

}



extern DRESULT
disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{

    sorry

#if 0
    if (pdrv != 0)
        sorry

    for (UINT index = 0; index < count; index += 1)
    {
        enum SDDo do_result = {0};

        do
        {
            do_result =
                SD_do
                (
                    0,
                    SDOperation_single_write,
                    &((struct Sector*) buff)[index],
                    sector + index
                );
        }
        while (do_result == SDDo_task_in_progress);

        switch (do_result)
        {

            case SDDo_task_completed:
            {
                // The sector-write was a success!
            } break;

            case SDDo_task_error:
            {
                sorry
            } break;

            case SDDo_driver_error:
            {
                sorry
            } break;

            case SDDo_task_in_progress : sorry
            case SDDo_bug              : sorry
            default                    : sorry
        }

    }

    return RES_OK;
#endif

}



extern DRESULT
disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{

    sorry

#if 0

    if (pdrv != 0)
        sorry

    for (UINT index = 0; index < count; index += 1)
    {
        enum SDDo do_result = {0};

        do
        {
            do_result =
                SD_do
                (
                    0,
                    SDOperation_single_read,
                    &((struct Sector*) buff)[index],
                    sector + index
                );
        }
        while (do_result == SDDo_task_in_progress);

        switch (do_result)
        {

            case SDDo_task_completed:
            {
                // The sector-read was a success!
            } break;

            case SDDo_task_error:
            {
                sorry
            } break;

            case SDDo_driver_error:
            {
                sorry
            } break;

            case SDDo_task_in_progress : sorry
            case SDDo_bug              : sorry
            default                    : sorry
        }

    }

    return RES_OK;

#endif

}



extern DRESULT
disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{

    sorry

#if 0

    if (pdrv != 0)
        sorry

    switch (cmd)
    {

        case CTRL_SYNC:
        {
            return RES_OK; // TODO Document.
        } break;

        case GET_SECTOR_COUNT:
        {
            sorry
        } break;

        case GET_SECTOR_SIZE:
        {
            sorry
        } break;

        case GET_BLOCK_SIZE:
        {
            sorry
        } break;

        case CTRL_TRIM:
        {
            sorry
        } break;

        default:
        {
            sorry
        } break;

    }

#endif

}



static void
_FILESYSTEM_fctprintf_callback(char character, void* void_fp)
{

    sorry

#if 0

    UINT bytes_written = {0};

    FRESULT result =
        f_write
        (
            (FIL*) void_fp,
            &character,
            sizeof(character),
            &bytes_written
        );

    if (result != FR_OK)
    {
        sorry
    }

#endif

}



extern int __attribute__((format(printf, 2, 3)))
f_printf(FIL* fp, const TCHAR* str, ...)
{

    sorry

#if 0

    va_list arguments = {0};
    va_start(arguments);

    int result = vfctprintf // TODO Define `vfctprintf` in `system.h`?
    (
        &_FILESYSTEM_fctprintf_callback,
        fp,
        str,
        arguments
    );

    va_end(arguments);

    return result;

#endif

}
