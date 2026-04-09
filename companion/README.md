# Companion Server

WebSocket server that captures MacBook mic audio, streams it to Deepgram for real-time speech-to-text, and relays transcriptions to the ESP32 todo device.

## Setup

```bash
cd companion
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

Copy your Deepgram API key into `.env`:

```
DEEPGRAM_API_KEY=your-actual-key-here
```

You can get a free key at https://console.deepgram.com — the free tier includes 200 minutes.

### macOS mic permissions

The first time you run the server, macOS will prompt for microphone access. Grant it for your terminal app (Terminal.app, iTerm2, etc.).

## Usage

### Test mode (validate mic + Deepgram)

Run this first to confirm your API key and mic are working — no ESP32 needed:

```bash
source venv/bin/activate
python server.py --test
```

Speak into your mic. Transcriptions print to the terminal in real time. Ctrl-C to stop.

### Server mode (for ESP32)

```bash
source venv/bin/activate
python server.py
```

The server listens on `ws://0.0.0.0:8765`. You can change the port with `--port`.

### Testing with websocat

You can simulate the ESP32 with [websocat](https://github.com/vi/websocat):

```bash
websocat ws://localhost:8765
```

Then type these JSON messages:

```json
{"action": "start"}
```

Speak into the mic — you'll see partial transcription messages:

```json
{"type": "partial", "text": "buy milk and eggs"}
```

Stop recording:

```json
{"action": "stop"}
```

You'll get the final transcript:

```json
{"type": "final", "text": "Buy milk and eggs from the store."}
```

## Protocol

### ESP32 → Server

| Message | Effect |
|---------|--------|
| `{"action": "start"}` | Begin recording and transcribing |
| `{"action": "stop"}` | Stop recording, send final transcript |

### Server → ESP32

| Message | Meaning |
|---------|---------|
| `{"type": "partial", "text": "..."}` | Live interim transcript (overwrite previous) |
| `{"type": "final", "text": "..."}` | Recording complete, use this as the task title |
| `{"type": "error", "message": "..."}` | Something went wrong |
