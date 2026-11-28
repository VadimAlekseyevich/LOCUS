"""Real structural + media + timing preparation in isolated containers."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import wave
import zipfile

dll, root, media = map(lambda s: Path(s).resolve(), sys.argv[1:4])
stage = root / "stage"
# Reuse a real exported fixture, replacing only the opaque audio test asset.
with zipfile.ZipFile(root / "fixture.locuspack") as archive:
    contents = {name: archive.read(name) for name in archive.namelist()}
audio_name = next(name for name in contents if name.endswith(".wav"))
assert audio_name == "music.wav"
class LocusZipInfo(zipfile.ZipInfo):
    # LOCUS ZIP requires the UTF-8 flag for every entry, including ASCII.
    def _encodeFilenameFlags(self):
        return self.filename.encode("utf-8"), self.flag_bits | 0x800

def pack(name, audio, art=None):
    data = dict(contents)
    data[audio_name] = audio.read_bytes()
    if art:
        manifest = json.loads(data["set.locus.json"])
        manifest["backgroundFile"] = "background.png"
        data["set.locus.json"] = json.dumps(manifest).encode()
        data["background.png"] = art.read_bytes()
    path = root / name
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_STORED) as archive:
        for key, value in data.items():
            archive.writestr(LocusZipInfo(key), value)
    return path

def run(path, expected=0, clean=True, env=None):
    if clean:
        for f in stage.iterdir():
            f.unlink()
    result = subprocess.run(["dotnet", str(dll), "worker", "prepare", str(path), str(stage)],
                            capture_output=True, text=True, timeout=330,
                            env=dict(os.environ, **(env or {})))
    assert result.returncode == expected, (path, result.returncode, result.stderr, result.stdout)
    if expected == 1:
        assert not result.stdout
        return
    value = json.loads(result.stdout)
    assert value["ok"] == (expected == 0)
    return value

good = pack("prepared.locuspack", media / "fixture.wav", media / "fixture.png")
value = run(good)
assert value["kind"] == "preparedPackage"
assert value["archive"]["sha256"] == hashlib.sha256(good.read_bytes()).hexdigest()
assert value["audio"]["audio"]["durationMs"] == 3000
assert value["audio"]["sha256"] == value["inspection"]["audio"]["sha256"]
assert value["background"]["image"] == {"width": 16, "height": 8}
assert len(value["files"]) == 4
for f in value["files"]:
    assert f["content"]["sha256"] == hashlib.sha256((stage / f["name"]).read_bytes()).hexdigest()
run(good, 1, clean=False)  # no reuse of nonempty stage
run(good, 1, env={"LOCUS_WORKER_MODE": "trusted-local"})
run(pack("invalid-media.locuspack", media / "truncated.wav"), 2)

short = root / "short.wav"
with wave.open(str(short), "wb") as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(48000)
    w.writeframes(bytes(48000 * 2))
run(pack("invalid-timing.locuspack", short), 2)
print("Complete package preparation passed.")
