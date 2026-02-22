#define LOG_ENTRY_MAX_SIZE   128
#define LOG_ENTRY_MAX_QUEUED 64



#include "log_driver_support.meta"
/* #meta

    for target in PER_TARGET():



        # See if the target utilizes the logger.

        log_driver = [
            driver
            for driver in target.drivers
            if driver['type'] == 'Log'
        ]

        if len(log_driver) == 0:
            log_driver = None
        else:
            log_driver, = log_driver


        Meta.define('TARGET_USES_LOG', log_driver is not None)



        # Determine the UXART handle of the logger.

        if log_driver is not None:

            uxart_driver, = [
                uxart_driver
                for uxart_driver in target.drivers
                if uxart_driver['type'  ] == 'UXART'
                if uxart_driver['handle'] == log_driver['uxart_handle']
            ]

            Meta.define('NVICInterrupt_LOG_UXART_INTERRUPT', f'NVICInterrupt_{uxart_driver['peripheral']}')
            Meta.define('LOG_UXART_HANDLE'                 , f'UXARTHandle_{uxart_driver['handle']}'      )

*/



typedef u8 LogEntry[LOG_ENTRY_MAX_SIZE];

struct LogDriver
{
    RingBuffer(LogEntry, LOG_ENTRY_MAX_QUEUED) entries;
    volatile i32                               bytes_of_current_entry_transmitted_so_far;
};

#if TARGET_USES_LOG

    static struct LogDriver _LOG_driver = // Very first log will be a delimiter to help know when it has started.
        {
            .entries.atomic_writer = 1,
            .entries.elements[0]   = { "\x1B[2J\n" ">>>> " STRINGIFY(TARGET_NAME) " <<<<\n" },

        };

#endif



////////////////////////////////////////////////////////////////////////////////



#if TARGET_USES_LOG

    #define log(FORMAT, ...)                                          \
        log_                                                          \
        (                                                             \
            "%u [" __FILE_NAME__ ":" STRINGIFY(__LINE__) "] " FORMAT, \
            _LOG_driver.entries.atomic_writer __VA_OPT__(,)           \
            __VA_ARGS__                                               \
        )

    static void __attribute__((format(printf, 1, 2)))
    log_(char* format, ...)
    {

        va_list arguments = {0};
        va_start(arguments);

        LogEntry* entry = RingBuffer_writing_pointer(&_LOG_driver.entries);

        if (!entry)
        {
            // No more space in the ring-buffer for another log entry.
            // Right before it filled up, however, we inserted an
            // overflow indicator log, so we don't have to worry
            // about it here.
        }
        else
        {

            // Format the log's message.

            u32 amount_in_queue   = RingBuffer_amount_in_queue(&_LOG_driver.entries);
            b32 will_be_maxed_out = amount_in_queue == countof(_LOG_driver.entries.elements) - 1;
            i32 max_write_length  = countof(*entry) - 4; // Not full buffer size so we can account for adding onto the end of the log.

            i32 entry_length =
                vsnprintf_
                (
                    (char*) *entry,
                    (u32) max_write_length,
                    format,
                    arguments
                );

            if (entry_length < 0)
            {
                sorry // TODO Debug error condition, otherwise we ignore?
            }
            else
            {

                // Indicate that this log entry's message was truncated.

                if (entry_length >= max_write_length)
                {
                    entry_length            = max_write_length - 1;
                    (*entry)[entry_length]  = '*';
                    entry_length           += 1;
                }



                // Indicate that this log entry is
                // the last one to make the queue full.
                // This means there might be some logs
                // dropped from here on out.

                if (will_be_maxed_out)
                {

                    (*entry)[entry_length]  = '\n';
                    entry_length           += 1;

                    (*entry)[entry_length]  = '*';
                    entry_length           += 1;

                }



                // Guarantee that the entry ends with new-line and is null-terminated.

                (*entry)[entry_length]  = '\n';
                entry_length           += 1;
                (*entry)[entry_length]  = '\0';



                // Submit the log and have the handler transmit it out.

                if (!RingBuffer_push(&_LOG_driver.entries, nullptr))
                {
                    sorry // TODO Debug error condition, otherwise we ignore?
                }
                else
                {
                    NVIC_SET_PENDING(LOG_UXART_INTERRUPT);
                }

            }

        }

        va_end(arguments);

    }

#else

    #define log(...) // Here so that code that do logging won't break.

#endif
