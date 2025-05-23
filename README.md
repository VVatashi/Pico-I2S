# Pico I2S

A simple implementation of the I²S (Inter-IC Sound) protocol for the Raspberry Pi Pico (RP2040).

# Hardware

## ADC

### PCM1808

Tested with the following configurations:

| RP2040 Frequency | Sample Rate | SCK multiplier | I2S Frame Size | Notes                                  |
| ---------------- | ----------- | -------------- | -------------- | -------------------------------------- |
| 192 MHz          | 8 kHz       | 256            | 32             | ✅                                     |
| 192 MHz          | 44.1 kHz    | 256            | 32             | ❌ Unable to set correct clock divider |
| 192 MHz          | 48 kHz      | 256            | 32             | ✅                                     |
| 192 MHz          | 96 kHz      | 256            | 32             | ✅                                     |
