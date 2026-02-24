////////////////////////////////////////////////////////////////////////////////
//
// This file implements a single-producer/single-consumer ring-buffer.
//
// All ring-buffers are just byte arrays. To make it generic across different
// element types, an anonymous union is used that reinterprets the underlying
// byte array as an array of the desired type.
//
// @/`Ring-Buffer Power-of-Two`:
// For optimization reasons, the length of the ring-buffer must be
// a power of two. This makes it so the reader and writer counters
// do not have to be modulated after each increment.
//
// Using `u16` for the reader/writers will limit us to a max of 2^15 = 32768 elements.
// If the user was using elements that were bytes, this would require 32 KiB of
// memory, which seems like more than necessary for most applications. Furthermore,
// with a reader and writer being `u16` each, this allows for natural word alignment
// without any padding.
//
// Note that the `RingBuffer` macro's union has the anonymous struct with the
// element array first in the `union` because for zero-initialization, this
// will guarantee the entire ring-buffer to be zero'ed out. If `ring_buffer_raw`
// was first instead, then that member will be the only one to get initialized,
// and that'd just be the reader/writer indices and not any of the elements.
//



struct RingBufferRaw
{
    volatile _Atomic u16 atomic_reader;
    volatile _Atomic u16 atomic_writer;
    u8                   bytes[];
};

#define RingBuffer(TYPE, LENGTH)                      \
    union                                             \
    {                                                 \
        static_assert(IS_POWER_OF_TWO(LENGTH));       \
        static_assert((LENGTH) < (1 << bitsof(u16))); \
        struct                                        \
        {                                             \
            volatile _Atomic u16 atomic_reader;       \
            volatile _Atomic u16 atomic_writer;       \
            TYPE                 elements[(LENGTH)];  \
        };                                            \
        struct RingBufferRaw ring_buffer_raw;         \
    }



////////////////////////////////////////////////////////////////////////////////



#define RingBuffer_amount_in_queue(RING_BUFFER) \
    RingBuffer_amount_in_queue_                 \
    (                                           \
        &(RING_BUFFER)->ring_buffer_raw         \
    )                                           \

static useret u32
RingBuffer_amount_in_queue_ // For both the producer and consumer.
(
    struct RingBufferRaw* ring_buffer_raw
)
{

    // Relaxed access here because no synchronization is needed;
    // just load in the reader/writer counters to guage the
    // ring-buffer's current capacity.

    u16 observed_reader = atomic_load_explicit(&ring_buffer_raw->atomic_reader, memory_order_relaxed);
    u16 observed_writer = atomic_load_explicit(&ring_buffer_raw->atomic_writer, memory_order_relaxed);



    // Gauging the ring-buffer's current capacity using a simple subtraction
    // is only possible when the ring-buffer's element capacity is a power-of-two.
    // The modulo wrapping behavior works out here.
    //
    // @/`Ring-Buffer Power-of-Two`.

    u32 amount_in_queue = (u16) { observed_writer - observed_reader };

    return amount_in_queue;

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
RingBuffer_writing_pointer_ // For the producer only.
(
    struct RingBufferRaw* ring_buffer_raw,
    u32                   element_count,
    u32                   element_size
)
{

    void* writing_pointer = nullptr;

    if (ring_buffer_raw)
    {

        // We perform an acquire of `atomic_reader` to be sure that the consumer has finished updating it.
        // Because this routine is only for the producer, no synchronization is needed with `atomic_writer`.

        u16 observed_reader = atomic_load_explicit(&ring_buffer_raw->atomic_reader, memory_order_acquire);
        u16 observed_writer = atomic_load_explicit(&ring_buffer_raw->atomic_writer, memory_order_relaxed);
        u32 amount_in_queue = (u16) { observed_writer - observed_reader };

        if (amount_in_queue < element_count)
        {
            u32 writing_index  = observed_writer & (element_count - 1); // @/`Ring-Buffer Power-of-Two`.
            u32 writing_offset = writing_index * element_size;
            writing_pointer = ring_buffer_raw->bytes + writing_offset;
        }
        else
        {
            // The ring-buffer is full; no place for the
            // producer to write a new element right now.
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
RingBuffer_push_ // For the producer only.
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
            // was already filled out in the ring-buffer by the user
            // using `RingBuffer_writing_pointer`.
        }



        // Using `memory_order_release` to ensure the data written into
        // the ring-buffer is completed before `atomic_writer` gets incremented.

        atomic_fetch_add_explicit
        (
            &ring_buffer_raw->atomic_writer,
            1,
            memory_order_release
        );

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
RingBuffer_reading_pointer_ // For the consumer only.
(
    struct RingBufferRaw* ring_buffer_raw,
    u32                   element_count,
    u32                   element_size
)
{

    void* reading_pointer = nullptr;

    if (ring_buffer_raw)
    {

        // Because this routine is only for the consumer, no synchronization is needed with `atomic_reader`.
        // We perform an acquire of `atomic_writer` to be sure that the producer has finished updating it.

        u16 observed_reader = atomic_load_explicit(&ring_buffer_raw->atomic_reader, memory_order_relaxed);
        u16 observed_writer = atomic_load_explicit(&ring_buffer_raw->atomic_writer, memory_order_acquire);
        u32 amount_in_queue = (u16) { observed_writer - observed_reader };

        if (amount_in_queue)
        {
            u32 reading_index  = observed_reader & (element_count - 1); // @/`Ring-Buffer Power-of-Two`.
            u32 reading_offset = reading_index * element_size;
            reading_pointer = ring_buffer_raw->bytes + reading_offset;
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
RingBuffer_pop_ // For the consumer only.
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
            // The destination of where to write the element to
            // was not provided. This should because either the
            // user doesn't care about the data, or because the
            // user has already used the data in the ring-buffer
            // in-place via `RingBuffer_reading_pointer`.
        }



        // Using `memory_order_release` to ensure all the data above
        // has been read/moved before `atomic_reader` gets incremented.

        atomic_fetch_add_explicit
        (
            &ring_buffer_raw->atomic_reader,
            1,
            memory_order_release
        );

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
RingBuffer_pop_to_latest_ // For the consumer only.
(
    struct RingBufferRaw* ring_buffer_raw,
    u32                   element_count,
    u32                   element_size,
    u8*                   dst
)
{

    // Because this routine is only for the consumer, no synchronization is needed with `atomic_reader`.
    // We perform an acquire of `atomic_writer` to be sure that the producer has finished updating it.

    u16 observed_reader = atomic_load_explicit(&ring_buffer_raw->atomic_reader, memory_order_relaxed);
    u16 observed_writer = atomic_load_explicit(&ring_buffer_raw->atomic_writer, memory_order_acquire);
    u32 amount_in_queue = (u16) { observed_writer - observed_reader };

    if (amount_in_queue)
    {

        if (dst)
        {

            // Instead of reading the next immediate element,
            // we read the latest available element so far,
            // which will be the one that's right behind the writer.

            u32   reading_index   = (observed_writer - 1) & (element_count - 1); // @/`Ring-Buffer Power-of-Two`.
            u32   reading_offset  = reading_index * element_size;
            void* reading_pointer = ring_buffer_raw->bytes + reading_offset;

            memmove(dst, reading_pointer, element_size);

        }
        else
        {
            // The destination of where to write the element to was not provided.
            // This is probably because the user wants to flush old data from the
            // ring-buffer while ensuring there's at least one element left.
        }



        // Using `memory_order_release` to ensure all the data above
        // has been read/moved before `atomic_reader` set to the
        // last available element in the ring-buffer.

        atomic_store_explicit
        (
            &ring_buffer_raw->atomic_reader,
            (u16) { observed_writer - 1 },
            memory_order_release
        );

    }
    else
    {
        // There's no data available right now,
        // which typically means the first element
        // hasn't been pushed into the ring-buffer yet.
    }

    return !!amount_in_queue;

}
