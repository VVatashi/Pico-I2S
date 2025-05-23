#pragma once

class BufferQueue
{
private:
    volatile uint32_t *buffer;

    volatile uint8_t current_write_buffer = 0;
    volatile uint8_t current_read_buffer = 0;

    volatile uint32_t ready_buffers_mask = 0;

    BufferQueue(const BufferQueue &) = delete;

    inline bool isBufferReady(uint8_t buffer_index) const
    {
        return ready_buffers_mask & (1UL << buffer_index);
    }

    inline void setBufferReady(uint8_t buffer_index)
    {
        ready_buffers_mask |= (1UL << buffer_index);
    }

    inline void clearBufferReady(uint8_t buffer_index)
    {
        ready_buffers_mask &= ~(1UL << buffer_index);
    }

public:
    const uint8_t buffer_count;
    const size_t buffer_size;

    BufferQueue(uint8_t buffer_count, size_t buffer_size);

    volatile uint32_t *getWriteBuffer();

    const volatile uint32_t *const getReadBuffer();

    ~BufferQueue();
};
