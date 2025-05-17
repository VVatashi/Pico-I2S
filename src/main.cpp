#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "i2s.pio.h"

gpio_function_t get_gpio_function(PIO pio)
{
    if (pio == pio0)
    {
        return GPIO_FUNC_PIO0;
    }

    if (pio == pio1)
    {
        return GPIO_FUNC_PIO1;
    }

    // RP2350 has 3 PIO blocks
#ifdef GPIO_FUNC_PIO2
    if (pio == pio2)
    {
        return GPIO_FUNC_PIO2;
    }
#endif

    return GPIO_FUNC_NULL;
}

uint init_i2s_master_clock(PIO pio, uint sm, uint offset, uint out_pin, float freq_hz)
{
    pio_sm_config config = i2s_master_clock_program_get_default_config(offset);

    sm_config_set_sideset_pins(&config, out_pin);
    sm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (2 * freq_hz));

    pio_gpio_init(pio, out_pin);

    pio_sm_set_consecutive_pindirs(pio, sm, out_pin, 1, true);

    pio_sm_init(pio, sm, offset, &config);

    return sm;
}

uint init_i2s_master_input(PIO pio, uint sm, uint offset, uint out_pin_base, uint in_pin, float freq_hz)
{
    pio_sm_config config = i2s_master_input_program_get_default_config(offset);

    sm_config_set_sideset_pins(&config, out_pin_base);
    sm_config_set_in_pins(&config, in_pin);

    sm_config_set_in_shift(&config, false, true, 32);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (2 * freq_hz));

    pio_gpio_init(pio, out_pin_base);
    pio_gpio_init(pio, out_pin_base + 1);
    pio_gpio_init(pio, in_pin);

    pio_sm_set_consecutive_pindirs(pio, sm, out_pin_base, 2, true);
    pio_sm_set_consecutive_pindirs(pio, sm, in_pin, 1, false);
    gpio_set_function(in_pin, get_gpio_function(pio));

    pio_sm_init(pio, sm, offset, &config);
    pio_sm_exec(pio, sm, pio_encode_jmp(offset + i2s_master_input_offset_entry_point));

    return sm;
}

const uint16_t rx_buffer_size = 8000;

uint32_t rx_buffer_0[rx_buffer_size];
uint32_t rx_buffer_1[rx_buffer_size];

uint8_t current_buffer = 0;

uint8_t dma_channel = 0;

void __isr dma_handler()
{
    dma_hw->ints0 = 1u << dma_channel;

    if (current_buffer == 0)
    {
        dma_channel_set_write_addr(0, rx_buffer_1, false);
        current_buffer = 1;
    }
    else
    {
        dma_channel_set_write_addr(0, rx_buffer_0, false);
        current_buffer = 0;
    }

    dma_channel_set_trans_count(0, rx_buffer_size, true);

    for (int i = 0; i < 8; i++)
    {
        uint32_t sample = (current_buffer == 0 ? rx_buffer_1[i] : rx_buffer_0[i]) << 1;
        int32_t signed_sample = ((int32_t)sample) >> 8;
        printf("%08X ", signed_sample);
    }

    printf("\n");
}

void setup_rx_dma(PIO pio, uint sm)
{
    dma_channel = dma_claim_unused_channel(true);

    dma_channel_config config = dma_channel_get_default_config(dma_channel);
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

    const uint SCK_PIN = 16;
    const uint BCK_PIN = 17;
    const uint LRCK_PIN = 18;
    const uint DIN_PIN = 19;

    const uint32_t sample_freq = 96000;

    uint i2s_clock_offset = pio_add_program(pio, &i2s_master_clock_program);
    uint i2s_clock_sm = init_i2s_master_clock(pio, pio_claim_unused_sm(pio, true), i2s_clock_offset, SCK_PIN, 256 * sample_freq);

    uint i2s_input_offset = pio_add_program(pio, &i2s_master_input_program);
    uint i2s_input_sm = init_i2s_master_input(pio, pio_claim_unused_sm(pio, true), i2s_input_offset, BCK_PIN, DIN_PIN, 64 * sample_freq);
    setup_rx_dma(pio, i2s_input_sm);

    pio_sm_set_enabled(pio, i2s_clock_sm, true);
    pio_sm_set_enabled(pio, i2s_input_sm, true);

    while (true)
    {
        tight_loop_contents();
    }
}
