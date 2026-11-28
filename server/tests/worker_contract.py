"""Adversarial protocol checks against the actual .NET process boundary.
The fake executable runs only in explicitly trusted-local test mode."""
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

dll = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
inspection = json.loads((root / "inspection.json").read_text())
score = json.loads((root / "score.json").read_text())
chart = inspection["difficulties"][0]
verify = ["verify", str(root / "stage"), chart["difficultyId"], chart["gameplaySha256"],
          "3000", "100", "0", "0", str(root / "valid.replay")]
inspect = ["inspect", str(root / "fixture.locuspack"), str(root / "stage")]

with tempfile.TemporaryDirectory() as tmp:
    script = Path(tmp) / "fixture"
    env = dict(os.environ, LOCUS_WORKER_MODE="trusted-local", LOCUS_WORKER_PATH=str(script))
    def check(body, args, expected=1, code=0):
        if not isinstance(body, str):
            body = json.dumps(body, ensure_ascii=False)
        # stdout literal is embedded as a Python repr, not shell-expanded.
        script.write_text("#!/usr/bin/python3\nimport sys, os\n"
                          "assert 'ConnectionStrings__Locus' not in os.environ\n"
                          f"sys.stdout.write({body!r})\nsys.exit({code})\n", encoding="utf-8")
        script.chmod(0o700)
        p = subprocess.run(["dotnet", str(dll), "worker", *args], env=env,
                           text=True, capture_output=True, timeout=10)
        assert p.returncode == expected, (p.returncode, p.stderr)
        if expected == 1:
            assert not p.stdout, "invalid worker output escaped validation"
        return p.stdout

    assert json.loads(check(inspection, inspect, 0)) == inspection
    assert json.loads(check(score, verify, 0)) == score
    rejected = json.loads(check({"protocolVersion": 1, "ok": False,
                                 "error": "/private/secret"}, inspect, 2, 2))
    assert rejected["error"] == "Worker rejected input."
    check(inspection, verify)
    check(score, inspect)
    check(score, verify, code=2)
    check('{"ok":true,' + json.dumps(score)[1:], verify)
    check(json.dumps(score).replace('"pp":', '"\\u0070p":0,"pp":', 1), verify)
    check(json.dumps(score) + "{}", verify)
    for key, value in [
        ("rankApproved", True), ("gameplaySha256", "a"*64), ("difficultyId", "b"*32),
        ("ratePercent", 101), ("modFlags", 1), ("modSeed", 1),
        ("judgementVersion", 2), ("difficultyVersion", 5), ("performanceVersion", 2),
        ("pp", -1), ("pp", "1"), ("stars", 100001), ("keyPresses", 2),
        ("accuracyPercent", 99), ("maxCombo", 4), ("timedHits", 2),
        ("meanErrorMs", 181), ("unstableRate", 1801), ("histogram", [0]*36),
        ("histogram", None), ("grades", {"perfect": 2, "good": 0, "bad": 0, "miss": 0}),
        ("unknown", 1),
    ]:
        bad = copy.deepcopy(score); bad[key] = value
        check(bad, verify)
    bad = copy.deepcopy(score); del bad["pp"]; check(bad, verify)
    for key, value in [("mediaValidated", True), ("setId", ""), ("title", " "),
                       ("difficultyVersion", 3), ("background", {"bytes": 0, "sha256": "a"*64}),
                       ("difficulties", []), ("audio", {"bytes": 1, "sha256": "G"*64})]:
        bad = copy.deepcopy(inspection); bad[key] = value; check(bad, inspect)
    bad = copy.deepcopy(inspection); bad["difficulties"].append(copy.deepcopy(chart)); check(bad, inspect)
    bad = copy.deepcopy(inspection); bad["audio"]["bytes"] = 256*1024*1024
    bad["background"] = {"bytes": 100*1024*1024, "sha256": "a"*64}; check(bad, inspect)
    bad = copy.deepcopy(inspection); bad["difficulties"][0]["keyPresses"] = 0; check(bad, inspect)
print("Typed worker contract adversarial checks passed.")
