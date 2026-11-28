"""Exercise the actual isolated decoder and digest-bound .NET adapter."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import wave
import zlib

dll = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
root.mkdir(parents=True, exist_ok=True)

def call(path, expected=0, digest=None, extra_env=None):
    digest = digest if digest is not None else hashlib.sha256(path.read_bytes()).hexdigest()
    result = subprocess.run(["dotnet", str(dll), "worker", "media", str(path), digest],
                            capture_output=True, text=True, timeout=75,
                            env=dict(os.environ, **(extra_env or {})))
    assert result.returncode == expected, (path.name, result.returncode, result.stderr, result.stdout)
    if expected == 1:
        assert not result.stdout
        return
    data = json.loads(result.stdout)
    assert data["ok"] == (expected == 0)
    if expected == 0:
        assert data["sha256"] == digest and data["bytes"] == path.stat().st_size
    return data

wav = root / "fixture.wav"
with wave.open(str(wav), "wb") as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(48000)
    w.writeframes(bytes(48000 * 3 * 4))
assert call(wav)["audio"] == {"sampleFrames": 144000, "sampleRate": 48000,
                             "channels": 2, "durationMs": 3000}
bad = root / "truncated.wav"
bad.write_bytes(wav.read_bytes()[:-100])
call(bad, 2)
call(wav, 1, "0"*64)
call(wav, 1, extra_env={"LOCUS_WORKER_MODE": "trusted-local"})

def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind+data))
def png(path, width, height, pixels):
    path.write_bytes(b"\x89PNG\r\n\x1a\n" +
        chunk(b"IHDR", struct.pack(">IIBBBBB", width,height,8,2,0,0,0)) +
        chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b""))

picture = root / "fixture.png"
png(picture, 16, 8, (b"\x00" + b"\xff\x00\x00"*16)*8)
assert call(picture)["image"] == {"width": 16, "height": 8}
bad = root / "broken.png"
png(bad, 16, 8, b"\x00")
call(bad, 2)
huge = root / "huge.png"
png(huge, 8192, 8192, b"\x00")
call(huge, 2)
wrong = root / "wrong.wav"
wrong.write_bytes(picture.read_bytes())
call(wrong, 2)

# Encode real MP3 and JPEG fixtures with the host test tool, not the
# production container. MP3 gapless delay/padding is handled by the decoder.
mp3 = root / "fixture.mp3"
jpg = root / "fixture.jpg"
for source, target, args in [(wav, mp3, ["-c:a","libmp3lame"]), (picture, jpg, ["-frames:v","1","-c:v","mjpeg"])]:
    subprocess.run(["ffmpeg", "-nostdin", "-v", "error", "-y", "-i", str(source), *args, str(target)], check=True)
decoded = call(mp3)["audio"]
assert decoded["sampleRate"] == 48000 and decoded["channels"] == 2
assert decoded["durationMs"] == 3000, decoded
assert call(jpg)["image"] == {"width": 16, "height": 8}
print("Isolated media verification passed.")
