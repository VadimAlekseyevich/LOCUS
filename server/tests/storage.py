"""Exercise durable object commit, concurrent dedup, corrupt-object rejection,
and complete package storage using real workers and a bounded stage."""
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys

dll, root = map(lambda s: Path(s).resolve(), sys.argv[1:3])
store = root / "objects"
store.mkdir(mode=0o700)
env = dict(os.environ, LOCUS_OBJECT_ROOT=str(store))

def call(*args, expected=0, custom_env=None, timeout=75):
    proc = subprocess.run(["dotnet", str(dll), *map(str, args)], capture_output=True,
                          text=True, timeout=timeout, env=custom_env or env)
    assert proc.returncode == expected, (args, proc.returncode, proc.stdout, proc.stderr)
    if expected == 1:
        assert not proc.stdout
        return
    return json.loads(proc.stdout)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest(), path.stat().st_size
def object_path(h):
    return store / h[:2] / h

source = root / "raw-blob"
source.write_bytes(os.urandom(1024*1024))
h, size = digest(source)
expected = {"key": "sha256/" + h, "sha256": h, "bytes": size}
# All contenders must succeed and converge to exactly one immutable file.
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
    results = list(pool.map(lambda _: call("object", "put", source, h, size), range(12)))
assert all(r == expected for r in results)
target = object_path(h)
assert target.read_bytes() == source.read_bytes()
assert stat.S_IMODE(target.stat().st_mode) == 0o400
inode = target.stat().st_ino
assert call("object", "verify", h, size) == expected
assert call("object", "put", source, h, size) == expected
assert target.stat().st_ino == inode
call("object", "put", source, "0"*64, size, expected=1)
call("object", "put", source, h, size-1, expected=1)
call("object", "verify", "../x", size, expected=1)
assert not object_path("0"*64).exists()

# An operator-level corruption is detected but not repaired by put.
target.chmod(0o600)
target.write_bytes(b"x"*size)
target.chmod(0o400)
call("object", "verify", h, size, expected=1)
call("object", "put", source, h, size, expected=1)
assert target.stat().st_ino == inode and target.read_bytes() == b"x"*size
# Symlinked root and permissive root fail closed.
alias = root / "object-alias"
alias.symlink_to(store, target_is_directory=True)
call("object", "verify", h, size, expected=1, custom_env=dict(env, LOCUS_OBJECT_ROOT=str(alias)))
loose = root / "loose-objects"
loose.mkdir(mode=0o755)
call("object", "put", source, h, size, expected=1, custom_env=dict(env, LOCUS_OBJECT_ROOT=str(loose)))

stage = root / "stage"
def clear_stage():
    for file in stage.iterdir():
        file.unlink()
clear_stage()
good = root / "prepared.locuspack"
receipt = call("worker", "store", good, stage, timeout=390)
assert receipt["ok"] and receipt["kind"] == "storedPackage"
assert receipt["archive"]["sha256"] == digest(good)[0]
for ref in [receipt["archive"]] + [f["blob"] for f in receipt["files"]]:
    assert call("object", "verify", ref["sha256"], ref["bytes"]) == ref
    assert digest(object_path(ref["sha256"])) == (ref["sha256"], ref["bytes"])
count = len(list(store.glob("*/*")))
clear_stage()
again = call("worker", "store", good, stage, timeout=390)
assert again == receipt and len(list(store.glob("*/*"))) == count
clear_stage()
bad = call("worker", "store", root / "invalid-timing.locuspack", stage, expected=2, timeout=390)
assert not bad["ok"] and len(list(store.glob("*/*"))) == count
assert not list(store.glob("*/.pending-*"))
print("Durable content-addressed storage passed.")
