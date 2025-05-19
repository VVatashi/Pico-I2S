#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "i2s.pio.h"
#include "i2s.h"

static inline float get_clkdiv(uint32_t freq_hz)
{
    return clock_get_hz(clk_sys) / (float)(freq_hz * 2);
}

static gpio_function_t get_gpio_function(PIO pio)
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

// I2SMasterClock implementation

I2SMasterClock::I2SMasterClock(PIO pio, uint8_t sm, uint8_t out_pin, uint32_t freq_hz, uint16_t multiplier)
    : pio(pio), sm(sm), offset(pio_add_program(pio, &i2s_master_clock_program)), freq_hz(freq_hz), multiplier(multiplier)
{
    pio_sm_config config = i2s_master_clock_program_get_default_config(offset);
    sm_config_set_sideset_pins(&config, out_pin);

    float divider = get_clkdiv(freq_hz * multiplier);
    sm_config_set_clkdiv(&config, divider);

#ifndef NDEBUG
    printf("sck frequency = %u Hz, divider = %f\n", freq_hz * multiplier, divider);
#endif

    pio_gpio_init(pio, out_pin);
    pio_sm_set_consecutive_pindirs(pio, sm, out_pin, 1, true);
    pio_sm_init(pio, sm, offset, &config);
}

void I2SMasterClock::enable()
{
    pio_sm_set_enabled(pio, sm, true);
}

void I2SMasterClock::disable()
{
    pio_sm_set_enabled(pio, sm, false);
}

void I2SMasterClock::setFrequency(uint32_t freq_hz)
{
    this->freq_hz = freq_hz;

    float divider = get_clkdiv(freq_hz * multiplier);
    pio_sm_set_clkdiv(pio, sm, divider);

#ifndef NDEBUG
    printf("sck frequency = %u Hz, divider = %f\n", freq_hz * multiplier, divider);
#endif
}

uint32_t I2SMasterClock::getFrequency() const
{
    return this->freq_hz;
}

I2SMasterClock::~I2SMasterClock()
{
    disable();
    pio_remove_program(pio, &i2s_master_clock_program, offset);
    pio_sm_unclaim(pio, sm);
}

// I2SMasterInput implementation

I2SMasterInput::I2SMasterInput(PIO pio, uint8_t sm, uint8_t out_pin_base, uint8_t in_pin, uint32_t freq_hz, uint8_t frame_size)
    : pio(pio), sm(sm), offset(pio_add_program(pio, &i2s_master_input_program)), freq_hz(freq_hz), frame_size(frame_size)
{
    pio_sm_config config = i2s_master_input_program_get_default_config(offset);
    sm_config_set_sideset_pins(&config, out_pin_base);
    sm_config_set_in_pins(&config, in_pin);
    sm_config_set_in_shift(&config, false, true, 32);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);

    float divider = get_clkdiv(freq_hz * frame_size * 2);
    sm_config_set_clkdiv(&config, divider);

#ifndef NDEBUG
    printf("bck frequency = %u Hz, divider = %f\n", freq_hz * frame_size * 2, divider);
#endif

    pio_gpio_init(pio, out_pin_base);
    pio_gpio_init(pio, out_pin_base + 1);
    pio_gpio_init(pio, in_pin);

    pio_sm_set_consecutive_pindirs(pio, sm, out_pin_base, 2, true);
    pio_sm_set_consecutive_pindirs(pio, sm, in_pin, 1, false);

    gpio_set_function(in_pin, get_gpio_function(pio));

    pio_sm_init(pio, sm, offset, &config);
    pio_sm_exec(pio, sm, pio_encode_jmp(offset + i2s_master_input_offset_entry_point));
}

void I2SMasterInput::enable()
{
    pio_sm_set_enabled(pio, sm, true);
}

void I2SMasterInput::disable()
{
    pio_sm_set_enabled(pio, sm, false);
}

void I2SMasterInput::setFrequency(uint32_t freq_hz)
{
    this->freq_hz = freq_hz;

    float divider = get_clkdiv(freq_hz * frame_size * 2);
    pio_sm_set_clkdiv(pio, sm, divider);

#ifndef NDEBUG
    printf("bck frequency = %u Hz, divider = %f\n", freq_hz * frame_size * 2, divider);
#endif
}

uint32_t I2SMasterInput::getFrequency() const
{
    return this->freq_hz;
}

I2SMasterInput::~I2SMasterInput()
{
    disable();
    pio_remove_program(pio, &i2s_master_input_program, offset);
    pio_sm_unclaim(pio, sm);
}
