# Linear Ticket Device — Product Requirements

A physical device for creating Linear issues by voice. Hold a button, speak, dial in metadata with five knobs, press again to submit.

## User Flow

1. **Record** — Hold the red button and speak into the microphone (ticket title/description)
2. **Transcribe** — On release, audio is sent to a local Whisper server on the Mac Mini; transcribed text appears on the OLED display
3. **Configure** — Turn the five rotary encoders to set issue metadata:
   - Encoder 1: Status (e.g. Backlog, Todo, In Progress)
   - Encoder 2: Priority (Urgent, High, Medium, Low, None)
   - Encoder 3: Assignee
   - Encoder 4: Project
   - Encoder 5: Label
4. **Submit** — Press the red button again to create the issue via Linear's GraphQL API
5. **Confirm** — Display shows success/failure, then resets to idle

## Hardware

| Component           | Part                        | Interface | Pins          |
|---------------------|-----------------------------|-----------|---------------|
| Microcontroller     | ESP32-WROOM (DOIT DevKit V1)| —         | —             |
| Display             | SSD1306 OLED (128x64, I2C)  | I2C       | 21 (SDA), 22 (SCL) |
| Red Button          | Momentary push switch        | GPIO      | 4             |
| Encoder 1 (Status)  | EC11 rotary encoder          | GPIO+ISR  | 16, 17, 5    |
| Encoder 2 (Priority)| EC11 rotary encoder          | GPIO+ISR  | 18, 19, 13   |
| Encoder 3 (Assignee)| EC11 rotary encoder          | GPIO+ISR  | 25, 26, 27   |
| Encoder 4 (Project) | EC11 rotary encoder          | GPIO+ISR  | 32, 33, 23   |
| Encoder 5 (Label)   | EC11 rotary encoder          | GPIO+ISR  | 14, 15, 2    |
| Microphone          | MAX4466 or MAX9814 analog    | ADC       | 34            |

Total GPIO usage: 19 of 22 usable pins.

## Software Architecture

### ESP32 Firmware (PlatformIO + Arduino)

```
src/
├── main.cpp              — setup/loop, state machine, WiFi init
├── encoders.cpp/.h       — interrupt-driven encoder reading for all 5 knobs
├── display.cpp/.h        — OLED UI rendering (idle, recording, config, submitting, result)
├── audio.cpp/.h          — I2S ADC recording, buffering, WAV encoding
├── network.cpp/.h        — WiFi connection, HTTP client for Whisper + Linear API
├── linear.cpp/.h         — Linear GraphQL queries (fetch options, create issue)
└── config.h              — pin definitions, WiFi credentials, API keys, server URLs
```

### Mac Mini Server

A lightweight HTTP server wrapping whisper.cpp (or faster-whisper).

- `POST /transcribe` — accepts WAV audio, returns `{ "text": "..." }`
- Runs Whisper `base` or `small` model for sub-second latency on Apple Silicon

### Linear API Integration

- **At boot:** fetch available workflow states, priorities, team members, projects, and labels via Linear's GraphQL API. Cache in memory so encoders have options to cycle through.
- **On submit:** `issueMutation` with title (from transcription), status, priority, assignee, project, and label IDs selected via encoders.
- **Auth:** Personal API key stored in `config.h` (or fetched from SPIFFS).

## State Machine

```
IDLE → (button held) → RECORDING → (button released) → TRANSCRIBING
  → (text received) → CONFIGURING → (button pressed) → SUBMITTING
  → (API response) → RESULT → (timeout 3s) → IDLE
```

| State         | Display Shows                          | Inputs Active       |
|---------------|----------------------------------------|---------------------|
| IDLE          | "Ready" / last-submitted issue title   | Red button          |
| RECORDING     | "Recording..." + elapsed time          | Red button (release) |
| TRANSCRIBING  | "Transcribing..."                      | None                |
| CONFIGURING   | Transcribed text + 5 field selections  | Encoders + button   |
| SUBMITTING    | "Creating issue..."                    | None                |
| RESULT        | "Created: [title]" or error message    | Auto-timeout        |

## Implementation Plan

### Phase 1: Core Input/Output (no network)

Get the physical interface working end-to-end without any network calls.

- [ ] **1.1** Set up I2C OLED display (SSD1306) with basic text rendering
- [ ] **1.2** Read all 5 rotary encoders with interrupt-driven decoding
- [ ] **1.3** Read the red button with debouncing
- [ ] **1.4** Build the state machine (IDLE → RECORDING → CONFIGURING → SUBMITTING → RESULT) with placeholder data instead of real network calls
- [ ] **1.5** Render the configuration UI on the OLED — show all 5 fields updating in real time as encoders turn, using hardcoded option lists

### Phase 2: Network + Linear API

Connect to WiFi and talk to Linear.

- [ ] **2.1** WiFi connection with auto-reconnect
- [ ] **2.2** Fetch workflow states, priorities, members, projects, labels from Linear's GraphQL API at boot
- [ ] **2.3** Replace hardcoded encoder options with live data from Linear
- [ ] **2.4** Submit issues to Linear via GraphQL mutation
- [ ] **2.5** Display success/error feedback from the API response

### Phase 3: Voice Input

Add the microphone and speech-to-text pipeline.

- [ ] **3.1** Set up whisper.cpp HTTP server on the Mac Mini
- [ ] **3.2** Wire MAX4466/MAX9814 mic to GPIO 34
- [ ] **3.3** Record audio via I2S ADC mode into a DMA buffer on button hold
- [ ] **3.4** Encode buffer as WAV and POST to the Whisper server on button release
- [ ] **3.5** Use transcribed text as the issue title/description

### Phase 4: Polish

- [ ] **4.1** Encoder push-button shortcuts (e.g. push encoder to reset that field to default)
- [ ] **4.2** Persist last-used settings to SPIFFS so defaults survive reboot
- [ ] **4.3** Visual feedback: recording animation, submit confirmation animation
- [ ] **4.4** Error handling: WiFi dropout, API failures, mic silence detection
- [ ] **4.5** Power management / sleep mode when idle

## Dependencies

| Dependency       | Purpose                    |
|------------------|----------------------------|
| Adafruit SSD1306 | I2C OLED display driver    |
| Adafruit GFX     | Graphics primitives        |
| ArduinoJson      | JSON parsing for API calls |
| WiFi (built-in)  | Network connectivity       |
| HTTPClient       | HTTP requests              |

## Decisions

- Transcribed text will be editable before submission (mechanism TBD — likely scroll through words/characters with an encoder)
- Encoder push-buttons reserved for future use
- Display scrolling/paging to be solved as needed once the UI is built
- Multi-team/workspace support deferred
