#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "buffer_queue.h"
#include "dma_reader.h"
#include "i2s.h"

int main()
{
    const uint32_t sys_freq = 192000000UL;
    set_sys_clock_hz(sys_freq, true);

    stdio_init_all();

#ifndef NDEBUG
    printf("clk_sys = %u Hz\n", clock_get_hz(clk_sys));
#endif

    const PIO pio = pio0;

    const uint8_t SCK_PIN = 16;
    const uint8_t BCK_PIN = 17;
    const uint8_t LRCK_PIN = 18;
    const uint8_t DIN_PIN = 19;

    const uint32_t sample_freq = 48000;

    I2SMasterClock clock(pio, pio_claim_unused_sm(pio, true), SCK_PIN, sample_freq, 256);
    I2SMasterInput input(pio, pio_claim_unused_sm(pio, true), BCK_PIN, DIN_PIN, sample_freq, 32);

    BufferQueue buffer_queue(4, 2048);
    DMAReader dma_reader(pio, input.sm, buffer_queue);

    clock.enable();
    input.enable();

    const uint8_t LED_PIN = 25;

    uint64_t previous_time = to_ms_since_boot(get_absolute_time());

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    while (true)
    {
        const volatile uint32_t *const buffer = buffer_queue.getReadBuffer();
        if (buffer != nullptr)
        {
            for (int i = 0; i < 8; i++)
            {
                uint32_t sample = (buffer[i]) << 1;
                int32_t signed_sample = ((int32_t)sample) >> 8;
                printf("%08X ", signed_sample);
            }

            printf("\n");
        }

        uint64_t current_time = to_ms_since_boot(get_absolute_time());

        if (current_time - previous_time > 500)
        {
            gpio_xor_mask(1u << LED_PIN);
            previous_time = current_time;
        }
    }
}
