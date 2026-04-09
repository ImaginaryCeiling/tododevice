from __future__ import annotations

import argparse
import asyncio
import json
import logging
import os
import signal
import sys

import sounddevice as sd
import websockets
from dotenv import load_dotenv

load_dotenv()

DEEPGRAM_API_KEY = os.getenv("DEEPGRAM_API_KEY")
DEEPGRAM_WS_URL = "wss://api.deepgram.com/v1/listen"

SAMPLE_RATE = 16000
CHANNELS = 1
DTYPE = "int16"
CHUNK_SAMPLES = int(SAMPLE_RATE * 0.1)  # 100ms chunks

WS_HOST = "0.0.0.0"
WS_PORT = 8765

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)
log = logging.getLogger("companion")


class TranscriptionSession:
    """Manages mic capture → Deepgram streaming → transcript relay for one recording."""

    def __init__(self, esp_ws=None):
        self.esp_ws = esp_ws
        self.audio_queue: asyncio.Queue[bytes] = asyncio.Queue()
        self.loop = asyncio.get_event_loop()
        self.running = False
        self.mic_stream = None
        self.dg_ws = None
        self._tasks: list[asyncio.Task] = []
        self.confirmed_text = ""

    async def start(self):
        self.running = True
        self.confirmed_text = ""

        params = "&".join([
            "model=nova-2",
            "language=en",
            "smart_format=true",
            "interim_results=true",
            "punctuate=true",
            f"sample_rate={SAMPLE_RATE}",
            "encoding=linear16",
            f"channels={CHANNELS}",
        ])
        url = f"{DEEPGRAM_WS_URL}?{params}"
        headers = {"Authorization": f"Token {DEEPGRAM_API_KEY}"}

        log.info("Connecting to Deepgram...")
        self.dg_ws = await websockets.connect(url, additional_headers=headers)
        log.info("Deepgram connected")

        self.mic_stream = sd.InputStream(
            samplerate=SAMPLE_RATE,
            channels=CHANNELS,
            dtype=DTYPE,
            blocksize=CHUNK_SAMPLES,
            callback=self._audio_callback,
        )
        self.mic_stream.start()
        log.info("Mic capture started")

        self._tasks = [
            asyncio.create_task(self._send_audio_loop()),
            asyncio.create_task(self._receive_transcripts_loop()),
        ]

    def _audio_callback(self, indata, frames, time_info, status):
        """Called from the PortAudio thread — schedule into asyncio safely."""
        if status:
            log.warning("PortAudio status: %s", status)
        if self.running:
            self.loop.call_soon_threadsafe(
                self.audio_queue.put_nowait, bytes(indata)
            )

    async def _send_audio_loop(self):
        """Drain the audio queue and forward chunks to Deepgram."""
        try:
            while self.running:
                try:
                    data = await asyncio.wait_for(self.audio_queue.get(), timeout=0.5)
                    await self.dg_ws.send(data)
                except asyncio.TimeoutError:
                    continue
        except websockets.exceptions.ConnectionClosed:
            log.warning("Deepgram WS closed while sending audio")
        except Exception:
            log.exception("Error in audio send loop")

    async def _receive_transcripts_loop(self):
        """Read transcript messages from Deepgram and forward to the ESP32."""
        try:
            async for raw_msg in self.dg_ws:
                msg = json.loads(raw_msg)
                if msg.get("type") != "Results":
                    continue

                transcript = (
                    msg.get("channel", {})
                    .get("alternatives", [{}])[0]
                    .get("transcript", "")
                )
                if not transcript:
                    continue

                is_final = msg.get("is_final", False)

                if is_final:
                    sep = " " if self.confirmed_text else ""
                    self.confirmed_text += sep + transcript
                    display = self.confirmed_text
                else:
                    sep = " " if self.confirmed_text else ""
                    display = self.confirmed_text + sep + transcript

                await self._relay({"type": "partial", "text": display.strip()})

        except websockets.exceptions.ConnectionClosed:
            log.info("Deepgram connection closed")
        except Exception:
            log.exception("Error in transcript receive loop")

    async def stop(self):
        self.running = False

        if self.mic_stream:
            self.mic_stream.stop()
            self.mic_stream.close()
            self.mic_stream = None
            log.info("Mic stopped")

        if self.dg_ws:
            try:
                await self.dg_ws.send(json.dumps({"type": "Finalize"}))
                await asyncio.sleep(0.8)
            except Exception:
                pass
            try:
                await self.dg_ws.send(json.dumps({"type": "CloseStream"}))
                await asyncio.sleep(0.3)
            except Exception:
                pass

        for t in self._tasks:
            t.cancel()
        await asyncio.gather(*self._tasks, return_exceptions=True)
        self._tasks.clear()

        if self.dg_ws:
            try:
                await self.dg_ws.close()
            except Exception:
                pass
            self.dg_ws = None

        final = self.confirmed_text.strip()
        await self._relay({"type": "final", "text": final})
        log.info("Final transcript: %s", final if final else "(empty)")

    async def _relay(self, payload: dict):
        if self.esp_ws is None:
            tag = "FINAL" if payload.get("type") == "final" else "     "
            print(f"  [{tag}] {payload.get('text', '')}", flush=True)
            return
        try:
            await self.esp_ws.send(json.dumps(payload))
        except websockets.exceptions.ConnectionClosed:
            log.warning("ESP32 disconnected, cannot relay message")


async def handle_client(websocket):
    addr = websocket.remote_address
    log.info("Client connected: %s", addr)
    session = None

    try:
        async for raw in websocket:
            try:
                data = json.loads(raw)
            except json.JSONDecodeError:
                log.warning("Invalid JSON from client: %s", raw)
                continue

            action = data.get("action")

            if action == "start":
                if session:
                    log.info("Tearing down previous session before starting new one")
                    await session.stop()
                session = TranscriptionSession(websocket)
                try:
                    await session.start()
                    log.info("Recording session started")
                except Exception:
                    log.exception("Failed to start recording session")
                    await websocket.send(json.dumps({
                        "type": "error",
                        "message": "Failed to start transcription session",
                    }))
                    session = None

            elif action == "stop":
                if session:
                    await session.stop()
                    session = None
                    log.info("Recording session stopped")
                else:
                    log.warning("Stop received but no active session")

            else:
                log.warning("Unknown action: %s", action)

    except websockets.exceptions.ConnectionClosed:
        log.info("Client disconnected: %s", addr)
    finally:
        if session:
            await session.stop()


def _check_api_key():
    if not DEEPGRAM_API_KEY or DEEPGRAM_API_KEY == "your-deepgram-api-key-here":
        log.error("Set DEEPGRAM_API_KEY in companion/.env before running")
        sys.exit(1)


async def run_test():
    """Standalone mic → Deepgram → terminal test (no WebSocket server)."""
    _check_api_key()

    print("\n  Speak into the mic — transcriptions will appear below.")
    print("  Press Ctrl-C to stop.\n")

    session = TranscriptionSession(esp_ws=None)
    await session.start()

    stop = asyncio.get_event_loop().create_future()
    loop = asyncio.get_event_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, stop.set_result, None)

    await stop
    print()
    await session.stop()


async def run_server():
    """Run the WebSocket server for ESP32 clients."""
    _check_api_key()

    log.info("Starting companion server on ws://%s:%d", WS_HOST, WS_PORT)

    stop_event = asyncio.get_event_loop().create_future()

    loop = asyncio.get_event_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, stop_event.set_result, None)

    async with websockets.serve(handle_client, WS_HOST, WS_PORT):
        log.info("Server ready — waiting for connections...")
        await stop_event

    log.info("Server shut down")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Todo Device companion server")
    parser.add_argument(
        "--test",
        action="store_true",
        help="Test mode: capture mic audio, transcribe via Deepgram, print to terminal (no WebSocket server)",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=WS_PORT,
        help=f"WebSocket server port (default: {WS_PORT})",
    )
    args = parser.parse_args()

    WS_PORT = args.port

    if args.test:
        asyncio.run(run_test())
    else:
        asyncio.run(run_server())
