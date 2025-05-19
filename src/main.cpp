#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "i2s.h"

static const uint16_t rx_buffer_size = 8192;

alignas(4) static uint32_t rx_buffer_0[rx_buffer_size];
alignas(4) static uint32_t rx_buffer_1[rx_buffer_size];

static volatile bool rx_buffer_0_ready = false;
static volatile bool rx_buffer_1_ready = false;

static uint8_t dma_channel = 0;

static void __isr dma_handler()
{
    static uint8_t current_buffer = 0;

    if (dma_channel_get_irq0_status(dma_channel))
    {
        dma_channel_acknowledge_irq0(dma_channel);

        if (current_buffer == 0)
        {
            current_buffer = 1;
            rx_buffer_0_ready = true;
            dma_channel_set_write_addr(dma_channel, rx_buffer_1, false);
        }
        else
        {
            current_buffer = 0;
            rx_buffer_1_ready = true;
            dma_channel_set_write_addr(dma_channel, rx_buffer_0, false);
        }

        dma_channel_set_trans_count(dma_channel, rx_buffer_size, true);
    }
}

static void setup_rx_dma(PIO pio, uint8_t sm)
{
    dma_channel = dma_claim_unused_channel(true);

    dma_channel_config config = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_32);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, pio_get_dreq(pio, sm, false));
    dma_channel_configure(dma_channel, &config, rx_buffer_0, &pio->rxf[sm], rx_buffer_size, false);

    dma_channel_set_irq0_enabled(dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    dma_channel_start(dma_channel);
}

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

    const uint32_t sample_freq = 8000;

    I2SMasterClock clock(pio, pio_claim_unused_sm(pio, true), SCK_PIN, sample_freq, 256);
    I2SMasterInput input(pio, pio_claim_unused_sm(pio, true), BCK_PIN, DIN_PIN, sample_freq, 32);

    setup_rx_dma(pio, input.sm);

    clock.enable();
    input.enable();

    while (true)
    {
        if (rx_buffer_0_ready)
        {
            rx_buffer_0_ready = false;

            for (int i = 0; i < 8; i++)
            {
                uint32_t sample = (rx_buffer_0[i]) << 1;
                int32_t signed_sample = ((int32_t)sample) >> 8;
                printf("%08X ", signed_sample);
            }

            printf("\n");
        }

        if (rx_buffer_1_ready)
        {
            rx_buffer_1_ready = false;

            for (int i = 0; i < 8; i++)
            {
                uint32_t sample = (rx_buffer_1[i]) << 1;
                int32_t signed_sample = ((int32_t)sample) >> 8;
                printf("%08X ", signed_sample);
            }

            printf("\n");
        }
    }
}
