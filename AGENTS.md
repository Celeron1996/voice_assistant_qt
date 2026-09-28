# AGENTS.md

Qt Widgets UI for the board voice assistant (100ask i.MX6ULL, Buildroot Qt 5.12.8, linuxfb).
Sources at the repo root; `README.md` has the full guide. Depends on the `voice_assistant` project
deployed at `/root/voice_assistant/` (Python backend owns all cloud protocol + audio).

## Build

```sh
mkdir -p ../build-voice_assistant_qt-100ask_imx6ull-Debug
cd ../build-voice_assistant_qt-100ask_imx6ull-Debug
/home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/bin/qmake ../voice_assistant_qt/voice_assistant_qt.pro CONFIG+=debug
make -j4        # re-run qmake after adding/removing files
```

## Deploy & run

```sh
scp VoiceAssistantQt root@192.168.1.14:/root/          # kill the running app first ("Text file busy")
ssh root@192.168.1.14 '/etc/init.d/S99voiceassistant stop'   # avoid mic conflict with the headless service
ssh root@192.168.1.14 'cd /root && nohup env QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0 \
    QT_QPA_FONTDIR=/usr/lib/fonts ./VoiceAssistantQt >/tmp/vaqt.log 2>&1 &'
```

- `VOICE_ASSISTANT_SCRIPT` / `VOICE_ASSISTANT_DIR` override the Python backend path (default `/root/voice_assistant`).
- `VOICE_QT_AUTOSTART=1` starts the conversation immediately (used for headless verification).
- `docs/spectrum_feed.py` is a sine-sweep test source: point `VOICE_ASSISTANT_SCRIPT` at it to exercise the
  spectrum without the mic/cloud.

## Verify visually

```sh
ssh root@192.168.1.14 'dd if=/dev/fb0 bs=4096 count=600' > fb.raw
convert -size 1024x600 -depth 8 bgra:fb.raw fb.png
```

## Gotchas (all handled in code — don't regress)

- **Touch**: copied from photo_album — disable libinput (`QT_QPA_FB_NO_LIBINPUT`), find the goodix device by
  `ABS_MT_POSITION_X/Y`, fix the abs range via `EVIOCSABS` (udev mislabels it; DTS says 800x480 but the driver emits 1024x600).
- **Volume**: the board's `pactl` is old — no `get-sink-volume`; query by parsing `pactl list sinks`.
  `set-sink-volume` / `set-sink-mute` work.
- **Spectrum data source**: Python prints `@AUDIO M/T <base64 PCM>`; its streaming text may glue onto the same
  line, so the parser searches for the token anywhere in the line (not only at the start). Keep it that way.
- **FFT belongs in C++**: pure-Python 256-pt FFT costs ~39 ms on this board (would eat the core); the widget
  runs 256-pt FFT + Hann window at 30 fps instead.
- App exit/pause: `QProcess::terminate()` → Python's SIGTERM handler closes the session/arecord/keepalive cleanly.
