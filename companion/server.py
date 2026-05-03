from __future__ import annotations

import argparse
import asyncio
import json
import logging
import os
import signal
import sys
import time

import sounddevice as sd
import websockets
from aiohttp import web
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
UI_PORT = 8090

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)
log = logging.getLogger("companion")


# ---------------------------------------------------------------------------
# UI Broadcaster — pushes live events to all connected dashboard clients
# ---------------------------------------------------------------------------

class UIBroadcaster:
    def __init__(self):
        self._clients: set[web.WebSocketResponse] = set()
        self._event_log: list[dict] = []
        self.state = {
            "esp_connected": False,
            "esp_addr": None,
            "recording": False,
            "transcript": "",
            "last_final": "",
        }

    async def add(self, ws: web.WebSocketResponse):
        self._clients.add(ws)
        await ws.send_json({
            "type": "init",
            "state": self.state,
            "log": self._event_log[-50:],
        })

    def remove(self, ws: web.WebSocketResponse):
        self._clients.discard(ws)

    async def emit(self, event_type: str, data: dict | None = None):
        entry = {
            "type": event_type,
            "ts": time.time(),
            **(data or {}),
        }
        self._event_log.append(entry)
        if len(self._event_log) > 200:
            self._event_log = self._event_log[-100:]

        dead = []
        for ws in self._clients:
            try:
                await ws.send_json(entry)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self._clients.discard(ws)


ui = UIBroadcaster()


# ---------------------------------------------------------------------------
# Transcription session
# ---------------------------------------------------------------------------

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
        log.debug("Deepgram URL: %s", url)
        log.debug("Auth header: Token %s...%s", DEEPGRAM_API_KEY[:4], DEEPGRAM_API_KEY[-4:])
        try:
            self.dg_ws = await websockets.connect(url, additional_headers=headers)
        except Exception as e:
            log.error("Deepgram connection failed: %s", e)
            raise
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

        ui.state["recording"] = True
        ui.state["transcript"] = ""
        await ui.emit("recording_start")

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
        chunks_sent = 0
        try:
            while self.running:
                try:
                    data = await asyncio.wait_for(self.audio_queue.get(), timeout=0.5)
                    await self.dg_ws.send(data)
                    chunks_sent += 1
                    if chunks_sent % 50 == 0:
                        log.debug("Audio chunks sent: %d (%d bytes each, queue: %d)",
                                  chunks_sent, len(data), self.audio_queue.qsize())
                except asyncio.TimeoutError:
                    continue
        except websockets.exceptions.ConnectionClosed as e:
            log.warning("Deepgram WS closed while sending audio: %s", e)
        except Exception:
            log.exception("Error in audio send loop")
        log.debug("Audio send loop exited after %d chunks", chunks_sent)

    async def _receive_transcripts_loop(self):
        """Read transcript messages from Deepgram and forward to the ESP32."""
        msg_count = 0
        try:
            async for raw_msg in self.dg_ws:
                msg = json.loads(raw_msg)
                msg_type = msg.get("type", "unknown")
                msg_count += 1
                log.debug("Deepgram msg #%d [%s]: %s", msg_count, msg_type,
                          json.dumps(msg)[:200])

                if msg_type != "Results":
                    continue

                transcript = (
                    msg.get("channel", {})
                    .get("alternatives", [{}])[0]
                    .get("transcript", "")
                )
                is_final = msg.get("is_final", False)

                if not transcript:
                    log.debug("Empty transcript (is_final=%s), skipping", is_final)
                    continue

                if is_final:
                    sep = " " if self.confirmed_text else ""
                    self.confirmed_text += sep + transcript
                    display = self.confirmed_text
                    log.debug("is_final transcript: %r -> confirmed: %r", transcript, self.confirmed_text)
                else:
                    sep = " " if self.confirmed_text else ""
                    display = self.confirmed_text + sep + transcript

                text = display.strip()
                await self._relay({"type": "partial", "text": text})
                ui.state["transcript"] = text
                await ui.emit("transcript", {"text": text, "is_final": is_final})

        except websockets.exceptions.ConnectionClosed as e:
            log.info("Deepgram connection closed: %s", e)
        except Exception:
            log.exception("Error in transcript receive loop")
        log.debug("Receive loop exited after %d messages", msg_count)

    async def stop(self):
        self.running = False

        if self.mic_stream:
            self.mic_stream.stop()
            self.mic_stream.close()
            self.mic_stream = None
            log.info("Mic stopped")

        if self.dg_ws:
            try:
                log.debug("Sending Finalize to Deepgram")
                await self.dg_ws.send(json.dumps({"type": "Finalize"}))
                await asyncio.sleep(0.8)
                log.debug("Finalize sent, waited 0.8s")
            except Exception as e:
                log.debug("Finalize send failed: %s", e)
            try:
                log.debug("Sending CloseStream to Deepgram")
                await self.dg_ws.send(json.dumps({"type": "CloseStream"}))
                await asyncio.sleep(0.3)
                log.debug("CloseStream sent, waited 0.3s")
            except Exception as e:
                log.debug("CloseStream send failed: %s", e)

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

        ui.state["recording"] = False
        ui.state["last_final"] = final
        ui.state["transcript"] = ""
        await ui.emit("recording_stop", {"text": final})

    async def _relay(self, payload: dict):
        if self.esp_ws is None:
            tag = "FINAL" if payload.get("type") == "final" else "     "
            print(f"  [{tag}] {payload.get('text', '')}", flush=True)
            return
        try:
            await self.esp_ws.send(json.dumps(payload))
        except websockets.exceptions.ConnectionClosed:
            log.warning("ESP32 disconnected, cannot relay message")


# ---------------------------------------------------------------------------
# ESP32 WebSocket handler
# ---------------------------------------------------------------------------

async def handle_client(websocket):
    addr = websocket.remote_address
    log.info("Client connected: %s", addr)
    session = None

    ui.state["esp_connected"] = True
    ui.state["esp_addr"] = f"{addr[0]}:{addr[1]}"
    await ui.emit("esp_connect", {"addr": ui.state["esp_addr"]})

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
        ui.state["esp_connected"] = False
        ui.state["esp_addr"] = None
        await ui.emit("esp_disconnect")


# ---------------------------------------------------------------------------
# Dashboard HTTP + WebSocket (aiohttp)
# ---------------------------------------------------------------------------

_DASHBOARD_DIR = os.path.dirname(os.path.abspath(__file__))


async def dashboard_handler(request):
    path = os.path.join(_DASHBOARD_DIR, "dashboard.html")
    return web.FileResponse(path)


async def dashboard_ws_handler(request):
    ws = web.WebSocketResponse()
    await ws.prepare(request)
    await ui.add(ws)
    try:
        async for _msg in ws:
            pass
    finally:
        ui.remove(ws)
    return ws


# ---------------------------------------------------------------------------
# Entrypoints
# ---------------------------------------------------------------------------

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
    """Run the WebSocket server for ESP32 clients + the dashboard UI."""
    _check_api_key()

    log.info("Starting companion server on ws://%s:%d", WS_HOST, WS_PORT)
    log.info("Dashboard UI at http://%s:%d", WS_HOST, UI_PORT)

    stop_event = asyncio.get_event_loop().create_future()

    loop = asyncio.get_event_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, stop_event.set_result, None)

    app = web.Application()
    app.router.add_get("/", dashboard_handler)
    app.router.add_get("/ws", dashboard_ws_handler)
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, WS_HOST, UI_PORT)
    await site.start()

    async with websockets.serve(handle_client, WS_HOST, WS_PORT):
        log.info("Server ready — waiting for connections...")
        await stop_event

    await runner.cleanup()
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
    parser.add_argument(
        "--ui-port",
        type=int,
        default=UI_PORT,
        help=f"Dashboard UI port (default: {UI_PORT})",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable debug logging",
    )
    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    WS_PORT = args.port
    UI_PORT = args.ui_port

    if args.test:
        asyncio.run(run_test())
    else:
        asyncio.run(run_server())
