#include <string.h>



////////////////////////////////////////////////////////////////////////////////
//
// All ring-buffers are just byte arrays.
// To make it generic across different element types,
// an anonymous union is used that reinterprets
// the underlying byte array as an array of the desired type.
//
// For optimization reasons, the length of the
// ring-buffer must be a power of two. This makes
// it so the reader and writer indices do not have
// to be modulated after each increment.
//



struct RingBufferRaw
{
    volatile u16 reader;  // Max amount of elements will be 2^16.
    volatile u16 writer;  // "
    u8           bytes[];
};

#define RingBuffer(TYPE, LENGTH)                \
    union                                       \
    {                                           \
        static_assert(IS_POWER_OF_TWO(LENGTH)); \
        struct                                  \
        {                                       \
            volatile u16 reader;                \
            volatile u16 writer;                \
            TYPE         elements[(LENGTH)];    \
        };                                      \
        struct RingBufferRaw ring_buffer_raw;   \
    }



////////////////////////////////////////////////////////////////////////////////



#define RingBuffer_writing_pointer(RING_BUFFER) \
    (                                           \
        (typeof(&(RING_BUFFER)->elements[0]))   \
        RingBuffer_writing_pointer_             \
        (                                       \
            &(RING_BUFFER)->ring_buffer_raw,    \
            countof((RING_BUFFER)->elements),   \
            sizeof((RING_BUFFER)->elements[0])  \
        )                                       \
    )

static useret void*
RingBuffer_writing_pointer_
(
    struct RingBufferRaw* ring_buffer_raw,
    u32                   element_count,
    u32                   element_size
)
{

    void* writing_pointer = nullptr;

    if (ring_buffer_raw)
    {

        u32 observed_reader = ring_buffer_raw->reader;
        u32 observed_writer = ring_buffer_raw->writer;
        b32 got_space       = (u32) (observed_writer - observed_reader) < element_count;

        if (got_space)
        {

            u32 index  = observed_writer & (element_count - 1);
            u32 offset = index * element_size;

            writing_pointer = ring_buffer_raw->bytes + offset;

        }

    }

    return writing_pointer;

}



////////////////////////////////////////////////////////////////////////////////



#define RingBuffer_push(RING_BUFFER, SRC)                  \
    RingBuffer_push_                                       \
    (                                                      \
        &(RING_BUFFER)->ring_buffer_raw,                   \
        countof((RING_BUFFER)->elements),                  \
        sizeof((RING_BUFFER)->elements[0]),                \
        (u8*) (true ? (SRC) : &(RING_BUFFER)->elements[0]) \
    )

static useret b32
RingBuffer_push_
(
    struct RingBufferRaw* ring_buffer_raw,
    u32                   element_count,
    u32                   element_size,
    u8*                   src
)
{

    void* writing_pointer =
        RingBuffer_writing_pointer_
        (
            ring_buffer_raw,
            element_count,
            element_size
        );

    if (writing_pointer)
    {

        if (src)
        {
            memmove(writing_pointer, src, element_size);
        }
        else
        {
            // The data to fill in the new element was not provided.
            // This should be because the data for the new element
            // is already filled out in the ring-buffer.
        }

        ring_buffer_raw->writer += 1;

    }

    return !!writing_pointer;

}



////////////////////////////////////////////////////////////////////////////////



#define RingBuffer_reading_pointer(RING_BUFFER) \
    (                                           \
        (typeof(&(RING_BUFFER)->elements[0]))   \
        RingBuffer_reading_pointer_             \
        (                                       \
            &(RING_BUFFER)->ring_buffer_raw,    \
            countof((RING_BUFFER)->elements),   \
            sizeof((RING_BUFFER)->elements[0])  \
        )                                       \
    )

static useret void*
RingBuffer_reading_pointer_
(
    struct RingBufferRaw* ring_buffer_raw,
    u32                   element_count,
    u32                   element_size
)
{

    void* reading_pointer = nullptr;

    if (ring_buffer_raw)
    {

        u32 observed_reader = ring_buffer_raw->reader;
        u32 observed_writer = ring_buffer_raw->writer;
        b32 got_element     = observed_reader != observed_writer;

        if (got_element)
        {

            u32 index  = observed_reader & (element_count - 1);
            u32 offset = index * element_size;

            reading_pointer = ring_buffer_raw->bytes + offset;

        }

    }

    return reading_pointer;

}



////////////////////////////////////////////////////////////////////////////////



#define RingBuffer_pop(RING_BUFFER, DST)                   \
    RingBuffer_pop_                                        \
    (                                                      \
        &(RING_BUFFER)->ring_buffer_raw,                   \
        countof((RING_BUFFER)->elements),                  \
        sizeof((RING_BUFFER)->elements[0]),                \
        (u8*) (true ? (DST) : &(RING_BUFFER)->elements[0]) \
    )

static useret b32
RingBuffer_pop_
(
    struct RingBufferRaw* ring_buffer_raw,
    u32                   element_count,
    u32                   element_size,
    u8*                   dst
)
{

    void* reading_pointer =
        RingBuffer_reading_pointer_
        (
            ring_buffer_raw,
            element_count,
            element_size
        );

    if (reading_pointer)
    {

        if (dst)
        {
            memmove(dst, reading_pointer, element_size);
        }
        else
        {
            // The destination of where to write the
            // element to was not provided. This should
            // because either the user doesn't care about
            // the data, or because the user has already
            // used the data in the ring-buffer in-place.
        }

        ring_buffer_raw->reader += 1;

    }

    return !!reading_pointer;

}



////////////////////////////////////////////////////////////////////////////////



#define RingBuffer_pop_to_latest(RING_BUFFER, DST)         \
    RingBuffer_pop_to_latest_                              \
    (                                                      \
        &(RING_BUFFER)->ring_buffer_raw,                   \
        countof((RING_BUFFER)->elements),                  \
        sizeof((RING_BUFFER)->elements[0]),                \
        (u8*) (true ? (DST) : &(RING_BUFFER)->elements[0]) \
    )

static useret b32
RingBuffer_pop_to_latest_
(
    struct RingBufferRaw* ring_buffer_raw,
    u32                   element_count,
    u32                   element_size,
    u8*                   dst
)
{

    u32 observed_reader = ring_buffer_raw->reader;
    u32 observed_writer = ring_buffer_raw->writer;
    b32 got_element     = observed_reader != observed_writer;

    if (got_element)
    {

        if (dst)
        {

            u32   index           = ((u32) (observed_writer - 1)) & (element_count - 1);
            u32   offset          = index * element_size;
            void* reading_pointer = ring_buffer_raw->bytes + offset;

            memmove(dst, reading_pointer, element_size);

        }
        else
        {
            // The destination of where to write the
            // element to was not provided. This should
            // be probably because the user wants to flush
            // old data from the ring-buffer.

        }

        // The latest element will always be available.
        ring_buffer_raw->reader = (u16) (observed_writer - 1);

    }
    else
    {
        // There's no data available right now,
        // which typically means the first element
        // hasn't been pushed into the ring-buffer yet.
    }

    return got_element;

}
