"""Exercise actual HTTP routes and the production C++ process adapter."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import urllib.error
import urllib.request

dll = Path(sys.argv[1]).resolve()
fixture = Path(sys.argv[2]).resolve()
root = Path(sys.argv[3]).resolve()
root.mkdir(parents=True)
stage = root / "stage"
stage.mkdir()
subprocess.run([str(fixture), str(root)], check=True)

def worker(*args, expected=0):
    proc = subprocess.run(["dotnet", str(dll), "worker", *map(str, args)],
                          capture_output=True, text=True, timeout=70)
    assert proc.returncode == expected, (proc.returncode, proc.stdout, proc.stderr)
    value = json.loads(proc.stdout)
    assert value["protocolVersion"] == 1 and value["ok"] == (expected == 0), value
    return value

inspection = worker("inspect", root / "fixture.locuspack", stage)
chart = inspection["difficulties"][0]
assert inspection["mediaValidated"] is False
job = ("verify", stage, chart["difficultyId"], chart["gameplaySha256"], 3000, 100, 0, 0)
score = worker(*job, root / "valid.replay")
assert score["grades"]["perfect"] == 3 and score["rankApproved"] is False
worker(*job, root / "offset.replay", expected=2)

base = "http://127.0.0.1:5080"
def request(path, status=200, method="GET"):
    req = urllib.request.Request(base + path, method=method)
    try:
        with urllib.request.urlopen(req, timeout=5) as r:
            assert r.status == status, (path, r.status)
            return json.load(r)
    except urllib.error.HTTPError as e:
        assert e.code == status, (path, e.code, e.read())
        return None

with (root / "server.log").open("w+") as log:
    server = subprocess.Popen(["dotnet", str(dll), "--urls", base],
                              stdout=log, stderr=subprocess.STDOUT)
    try:
        for _ in range(60):
            try:
                request("/health/live")
                break
            except (OSError, urllib.error.URLError):
                if server.poll() is not None:
                    raise RuntimeError("Server exited")
                time.sleep(0.5)
        else:
            raise RuntimeError("Server did not start")
        assert request("/health/ready")["schemaVersion"] == 2
        first = request("/api/v1/maps?limit=1")
        assert len(first["items"]) == 1 and first["next"]
        second = request("/api/v1/maps?limit=1&after=" + first["next"])
        assert len(second["items"]) == 1 and second["next"] is None
        assert first["items"][0]["revisionId"] != second["items"][0]["revisionId"]
        request("/api/v1/maps?limit=101", 400)
        assert request("/api/v1/players/00000000-0000-0000-0000-000000000001")["displayName"] == "Alpha"
        request("/api/v1/players/00000000-0000-0000-0000-000000000003", 404)
        board = request("/api/v1/revisions/30000000-0000-0000-0000-000000000001/leaderboard")
        assert [x["pp"] for x in board["items"]] == [20, 15], board
        request("/api/v1/revisions/30000000-0000-0000-0000-000000000002/leaderboard", 404)
        request("/api/v1/revisions/30000000-0000-0000-0000-000000000003/leaderboard", 404)
        request("/api/v1/me", 401)
        # No write route: framework may challenge via the fallback policy.
        try:
            urllib.request.urlopen(urllib.request.Request(base + "/api/v1/maps", method="POST"), timeout=5)
            raise AssertionError("Unexpected write route")
        except urllib.error.HTTPError as e:
            assert e.code in (401, 405), e.code
    finally:
        server.terminate()
        try:
            server.wait(timeout=10)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()
        log.seek(0)
        print(log.read())
print("Backend smoke passed.")
