# ESP32-WROOM (DOIT DevKit V1) Pinout

## Pin Assignments

| GPIO | Function   | Connected To                | Direction | Notes                           |
|------|------------|-----------------------------|-----------|---------------------------------|
| 21   | TFT_MOSI   | SPI display data (DIN)      | Output    | Software SPI                    |
| 22   | TFT_SCLK   | SPI display clock (CLK)     | Output    | Software SPI                    |
| 5    | TFT_CS     | SPI display chip select     | Output    | Active LOW                      |
| 2    | TFT_DC     | SPI display data/command    | Output    | Safe at boot (floats LOW)       |
| 13   | TFT_RST    | SPI display reset           | Output    | Active LOW                      |
||
| 4    | BUTTON     | Red push button             | Input     | Internal pullup, active LOW     |
|
| 16   | ENC1_A     | Encoder 1 — channel A (CLK) | Input     | Internal pullup, interrupt      |
| 17   | ENC1_B     | Encoder 1 — channel B (DT)  | Input     | Internal pullup, interrupt      |
| 35   | ENC1_SW    | Encoder 1 — push switch     | Input     | Input-only, external 10K pullup |
|
| 18   | ENC2_A     | Encoder 2 — channel A (CLK) | Input     | Internal pullup, interrupt      |
| 19   | ENC2_B     | Encoder 2 — channel B (DT)  | Input     | Internal pullup, interrupt      |
| 36   | ENC2_SW    | Encoder 2 — push switch     | Input     | Input-only, external 10K pullup |
|
| 25   | ENC3_A     | Encoder 3 — channel A (CLK) | Input     | Internal pullup, interrupt      |
| 26   | ENC3_B     | Encoder 3 — channel B (DT)  | Input     | Internal pullup, interrupt      |
| 27   | ENC3_SW    | Encoder 3 — push switch     | Input     | Internal pullup, active LOW     |
|
| 32   | ENC4_A     | Encoder 4 — channel A (CLK) | Input     | Internal pullup, interrupt      |
| 33   | ENC4_B     | Encoder 4 — channel B (DT)  | Input     | Internal pullup, interrupt      |
| 23   | ENC4_SW    | Encoder 4 — push switch     | Input     | Internal pullup, active LOW     |
|
| 14   | ENC5_A     | Encoder 5 — channel A (CLK) | Input     | Internal pullup, interrupt      |
| 15   | ENC5_B     | Encoder 5 — channel B (DT)  | Input     | Internal pullup, interrupt      |
| 34   | ENC5_SW    | Encoder 5 — push switch     | Input     | Input-only, external 10K pullup |

## Peripherals

| Component          | Type                        | Interface | Pins Used       |
|--------------------|-----------------------------|-----------|-----------------|
| SPI Display        | Waveshare 2" ST7789 IPS LCD | SPI       | 21, 22, 5, 2, 13 |
| Red Button         | Momentary push switch       | GPIO      | 4               |
| Rotary Encoder 1   | EC11 (360° + push)          | GPIO + ISR | 16, 17, 35     |
| Rotary Encoder 2   | EC11 (360° + push)          | GPIO + ISR | 18, 19, 36     |
| Rotary Encoder 3   | EC11 (360° + push)          | GPIO + ISR | 25, 26, 27     |
| Rotary Encoder 4   | EC11 (360° + push)          | GPIO + ISR | 32, 33, 23     |
| Rotary Encoder 5   | EC11 (360° + push)          | GPIO + ISR | 14, 15, 34     |

## Wiring Reference

### SPI Display (Waveshare 2" LCD, 8-pin connector)

| Display Pin | Connects To    |
|-------------|----------------|
| VCC         | ESP32 3.3V     |
| GND         | ESP32 GND      |
| DIN         | GPIO 21        |
| CLK         | GPIO 22        |
| CS          | GPIO 5         |
| DC          | GPIO 2         |
| RST         | GPIO 13        |
| BL          | ESP32 3.3V     |

### Red Button (4-pin, two pins per side)

Each side pair is internally shorted. Wire one pin from each side:

| Button Pin  | Connects To       |
|-------------|--------------------|
| Side A      | GPIO 4             |
| Side B      | ESP32 GND          |

### Rotary Encoders (EC11-style, 5 pins: 3 + 2)

3-pin side (rotation):

| Encoder Pin | Connects To          |
|-------------|----------------------|
| A (CLK)     | GPIO (see table)     |
| GND         | ESP32 GND            |
| B (DT)      | GPIO (see table)     |

2-pin side (push button):

| Encoder Pin | Connects To          |
|-------------|----------------------|
| SW          | GPIO (see table)     |
| GND         | ESP32 GND            |

## Pin Budget

| Category           | Count |
|--------------------|-------|
| SPI Display        | 5     |
| Red Button         | 1     |
| Encoders (5 × 3)  | 15    |
| **Total used**     | **21**|
| Remaining spare    | 0 safe GPIOs |
| Input-only spare   | 1 (GPIO 39 — no internal pullup) |

## Notes

- **GPIOs 34, 35, 36** (ENC5_SW, ENC1_SW, ENC2_SW): input-only pins with no internal pullup — each requires an external 10K resistor to 3.3V.
- **GPIO 2** (TFT_DC): has weak internal pulldown, floats LOW at boot — safe for display DC line.
- **GPIO 14**: outputs a brief PWM pulse during boot — no effect on encoder reading.
- **GPIO 15**: outputs a brief PWM pulse during boot and controls boot debug logging — no issue for encoder input.
- **BL (backlight)**: tied directly to 3.3V for always-on. Can be moved to a GPIO for brightness control if a pin becomes available.
- All encoder A/B pins use interrupts on `CHANGE` for responsive rotation detection.
- GND connections from all components can share a common ground rail.
