# ESP32-WROOM (DOIT DevKit V1) Pinout

## Pin Assignments

| GPIO | Function   | Connected To                | Direction | Notes                           |
|------|------------|-----------------------------|-----------|---------------------------------|
| 21   | DISP_SDA   | I2C display SDA             | I/O       | Default I2C SDA, internal pullup|
| 22   | DISP_SCL   | I2C display SCL             | Output    | Default I2C SCL, internal pullup|
| 4    | BUTTON     | Red push button             | Input     | Internal pullup, active LOW     |
| 16   | ENC1_A     | Encoder 1 — channel A (CLK) | Input     | Internal pullup, interrupt      |
| 17   | ENC1_B     | Encoder 1 — channel B (DT)  | Input     | Internal pullup, interrupt      |
| 5    | ENC1_SW    | Encoder 1 — push switch     | Input     | Internal pullup, active LOW     |
| 18   | ENC2_A     | Encoder 2 — channel A (CLK) | Input     | Internal pullup, interrupt      |
| 19   | ENC2_B     | Encoder 2 — channel B (DT)  | Input     | Internal pullup, interrupt      |
| 13   | ENC2_SW    | Encoder 2 — push switch     | Input     | Internal pullup, active LOW     |
| 25   | ENC3_A     | Encoder 3 — channel A (CLK) | Input     | Internal pullup, interrupt      |
| 26   | ENC3_B     | Encoder 3 — channel B (DT)  | Input     | Internal pullup, interrupt      |
| 27   | ENC3_SW    | Encoder 3 — push switch     | Input     | Internal pullup, active LOW     |
| 32   | ENC4_A     | Encoder 4 — channel A (CLK) | Input     | Internal pullup, interrupt      |
| 33   | ENC4_B     | Encoder 4 — channel B (DT)  | Input     | Internal pullup, interrupt      |
| 23   | ENC4_SW    | Encoder 4 — push switch     | Input     | Internal pullup, active LOW     |
| 14   | ENC5_A     | Encoder 5 — channel A (CLK) | Input     | Internal pullup, interrupt      |
| 15   | ENC5_B     | Encoder 5 — channel B (DT)  | Input     | Internal pullup, interrupt      |
| 2    | ENC5_SW    | Encoder 5 — push switch     | Input     | Internal pullup, on-board LED   |

## Peripherals

| Component          | Type                     | Interface  | Pins Used       |
|--------------------|--------------------------|------------|-----------------|
| I2C Display        | SSD1306 or similar OLED  | I2C        | 21, 22          |
| Red Button         | Momentary push switch    | GPIO       | 4               |
| Rotary Encoder 1   | EC11 (360° + push)       | GPIO + ISR | 16, 17, 5       |
| Rotary Encoder 2   | EC11 (360° + push)       | GPIO + ISR | 18, 19, 13      |
| Rotary Encoder 3   | EC11 (360° + push)       | GPIO + ISR | 25, 26, 27      |
| Rotary Encoder 4   | EC11 (360° + push)       | GPIO + ISR | 32, 33, 23      |
| Rotary Encoder 5   | EC11 (360° + push)       | GPIO + ISR | 14, 15, 2       |

## Wiring Reference

### I2C Display (4-pin module)

| Display Pin | Connects To    |
|-------------|----------------|
| GND         | ESP32 GND      |
| VCC         | ESP32 3.3V     |
| SCL         | GPIO 22        |
| SDA         | GPIO 21        |

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

| Category         | Count |
|------------------|-------|
| I2C Display      | 2     |
| Red Button       | 1     |
| Encoders (5 × 3) | 15   |
| **Total used**   | **18**|
| Remaining safe   | 0     |
| Input-only spare | 4 (GPIOs 34, 35, 36, 39 — no internal pullup) |

## Notes

- **GPIO 2** (ENC5_SW): doubles as the on-board LED — LED will flicker when the encoder is pressed, which is harmless.
- **GPIO 14**: outputs a brief PWM pulse during boot — no effect on encoder reading since the encoder is passive.
- **GPIO 15**: outputs a brief PWM pulse during boot and controls boot debug logging — same as above, no issue for encoder input.
- All encoder and button pins use `INPUT_PULLUP` so no external pull-up resistors are needed.
- All encoder A/B pins use interrupts on `CHANGE` for responsive rotation detection.
- GND connections from all components can share a common ground rail.
