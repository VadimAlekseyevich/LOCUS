# Internal verification worker — protocol 1

This executable is the first implemented backend process boundary, not a
network server. It links the portable production package and scoring code.
Build on Linux (or Windows) without desktop dependencies:

```sh
cmake -S . -B build-worker -DLOCUS_BUILD_APP=OFF -DLOCUS_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-worker --config Release --target locus_worker locus_worker_fixture
ctest --test-dir build-worker -C Release -R locus_worker_protocol_v1 --output-on-failure
```

## Invocation

```text
locus_worker inspect ARCHIVE EMPTY_STAGE
locus_worker verify STAGE DIFFICULTY_ID GAMEPLAY_SHA256 AUDIO_DURATION_MS RATE_PERCENT MOD_FLAGS MOD_SEED REPLAY
```

Use a process argument array, not a shell command assembled from user
input. The calling service owns a fresh private staging directory and its
cleanup (also after any error). Paths, digest, independently decoded
duration, rate, flags and seed come from trusted publication/attempt
records, never directly from a submitted client's arguments. The stage
must remain immutable during evaluation; no concurrent writers or
untrusted filesystem access. Reopened stages enforce the same total
byte/count budgets as archive extraction.

Each invocation emits one UTF-8 JSON object on stdout with
`protocolVersion: 1` and `ok`. Exit codes: 0 success, 2 rejected job/input,
64 invalid command, 1 internal exception. Treat unexpected exit,
malformed/oversized stdout or timeout as an internal failure. The
server adapter validates the exact typed schema, field bounds, versions
and score/request consistency before emitting validated DTOs. The CLI
alone provides no sandbox;
the private Linux [container adapter](../infra/worker/README.md) enforces
CPU, memory, process, disk and wall-time budgets.
Do not pass worker error text directly to public HTTP clients.

- `inspect`: extracts store-only archive, validates all charts against
  shared timing, returns authoring IDs, titles, asset SHA-256/size and
  each difficulty's effective gameplay digest, physical stars and key
  count. `kind: "packageInspection"`, `mediaValidated: false`. No full
  media decode, duration verification, ownership or publication.
- `verify`: validates expected digest against actual effective chart and
  audio bytes, uses trusted audio duration to prepare the playable chart,
  checks exact replay rules/rate/mods/seed/offset, rejects empty traces,
  unbounded timestamps and untimed Finish events, then rejudges using
  TurnJudge and derives stars, grades, accuracy, max combo, timing
  statistics and experimental PP with versions. `kind: "scoreEvaluation"`,
  `rankApproved: false`. No client summary or PP input.
  Current timestamp envelope is [-rate-1, duration+rate+1] seconds
  (rate is the playback multiplier); order is preserved because native
  timestamps can arrive out of order. This is an initial private-worker
  policy, not a final ranked transport contract.

A mathematically correct replay can be fabricated. This worker does not
prove human input, authenticate users, issue server revision IDs, implement
idempotency, decide rank eligibility or write a database. Media inspection,
session policy and API integration are separate gates.
The initial operator-only Linux container isolation is implemented.
The worker currently revalidates and hashes the whole staged package on
each job. Cache immutable verified revisions only after a measured need
and an explicit trusted-storage boundary.
