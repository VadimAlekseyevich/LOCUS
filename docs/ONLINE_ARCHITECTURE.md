# LOCUS — Online architecture and preparation

**Status: proposed plan, not an implemented service.** The desktop game remains
local-first. There is no account server, public upload endpoint, online
leaderboard, global PP, web frontend, or deployed infrastructure. This
document defines preparation and acceptance gates; implementation details
remain subject to design review. It is not an API specification.

## 1. Current reusable foundation and gaps

| Existing contract | Reuse | Gap before online use |
| --- | --- | --- |
| `locus_core` / `LOCUS_BUILD_APP=OFF` | Portable chart parser, exact timestamps, judge, mods, replay codec, Difficulty V4 | Portable score verification entry point implemented; authoritative package/session input validation still needed |
| Song Set ID + difficulty ID | Stable local identity through rename/copy/import | Server-owned published IDs, ownership and revision mapping |
| `ChartGameplayRevision` | Detect changed gameplay within local history | Local fingerprint is unchanged; SHA-256 publication content descriptor binds effective chart and exact audio bytes, not ownership |
| Results v3 + LOCREP04 | Immutable summaries and separate replay data | Portable local result model, persistent local player ID and random 128-bit attempt ID implemented; authenticated account mapping and submission status pending |
| `.locuspack` | Portable package with shared audio, metadata, optional background and charts | Portable structural validation shared with desktop; quotas, full media inspection and immutable publication pending |
| Async catalog/history | Cached, cancellable off-frame data loading | Network I/O, paging, cancellation, session ownership and durable submission retries |
| Local file transactions | Protect local edits and recoverable import | Not a server persistence layer; Windows-only chart/byte I/O and local paths must not leak into API |

Read [architecture](ARCHITECTURE.md), [chart format](CHART_FORMAT.md),
[results](RESULTS.md) and [song sets](SONG_SETS.md) for current contracts.
The Linux core CI does not build the desktop storage targets. The portable
`EvaluateCompletedReplay` entry point in `core/score_verification.h` derives
counts and statistics from a completed trace and requires the caller's
trusted chart offset. Local replay validation uses this same entry point.
It does not authenticate a submission or decide ranking eligibility.
The portable `locus_package` target builds the same ZIP reader/writer, manifest codec and staged package validator as the desktop import path. It validates bounded JSON and archive structure, shared audio references, unique chart IDs, effective timing and image signatures. It does not decode audio or fully decode images, and never publishes to a server.

## 2. Proposed deployment and ownership

Start with a modular monolith. Game client and website share one versioned
HTTPS API; the service owns authentication, accounts, publication, score
submission, rankings, moderation and job scheduling. PostgreSQL is the
proposed source of truth for relational records. S3-compatible object
storage holds immutable packs, audio, artwork and replay blobs. Background
workers validate submissions and calculate derived scores via the exact
same pinned C++ core used by the desktop client. Do not reimplement the
judge or difficulty formula in JavaScript or C#.

Proposed stack (not selected dependencies): ASP.NET Core API, PostgreSQL,
TypeScript web UI (e.g. Next.js), C++ verifier executable/worker, libcurl for
native HTTPS, Docker for local development. Choose a simple authenticated
process boundary for the C++ worker first; benchmark before adopting FFI,
a new service or another language. No Redis, Kubernetes, microservices or
persistent socket connection are necessary for the first vertical slice.

Suggested repository boundaries when needed:

- `src/core/`: portable deterministic rules and validation, no HTTP, OS
  UI, account or storage dependencies.
- `src/app/`: local desktop state, transport, storage, network adapter.
- `server/`: API and persistent server-domain model.
- `web/`: browser UI consuming the API, not a second scoring implementation.
- `protocol/`: versioned OpenAPI contracts and generated/validated DTOs.
- `infra/`: reproducible local and deployed configuration; no secrets.
- `workers/`: isolated validation/calculation entry points.

Do not create empty directories or abstract repositories for hypothetical
consumers. Extract actual boundaries as the first vertical slice requires.

## 3. Identity, revisions and immutable data

Keep local authoring IDs separate from server-issued identities. A locally
imported setId is a merge key, never proof of online ownership. A local
difficulty ID survives rename; a copied difficulty has a new identity.
Server ownership comes from the authenticated account and permissions, not
an uploaded manifest's mapper string.

Proposed relational model:

| Entity | Important fields / constraints |
| --- | --- |
| User | Server user ID, unique normalized username, account state |
| SongSet | Server ID, owner, publication state, metadata |
| Difficulty | Server ID, parent set, stable association with authoring ID |
| PublishedRevision | Immutable chart/timing/media references, cryptographic digests, format/version, publication time |
| Attempt | Server score ID, user ID, revision ID, exact rate/mod settings/seed, rule version, server receipt time, replay reference, verification status |
| DerivedPerformance | Attempt ID + performance formula version, difficulty version, derived PP |
| BestScore | Unique player/revision/leaderboard category, reference to verified attempt |
| PlayerStats | Rebuildable projections (PP, play count, accuracy, rank) |
| ModerationEvent | Subject, decision, actor, timestamp, reason and audit trail |

The portable `DescribePackageRevision` computes SHA-256 for exact manifest,
audio, optional artwork and chart bytes, plus per-difficulty gameplay content
hashes from the effective shared timing, canonical version-1 little-endian word
stream and exact audio SHA-256. Metadata-only chart renames change the raw
file hash, not the gameplay hash. The caller owns immutable private staging;
this descriptor is not a server-issued revision ID. The implementation uses
pinned MIT PicoSHA2 and streams files in bounded blocks.
Use these digests for content addressing, never authentication. Bind effective chart, authoritative
shared tempo map and audio content to a published revision. Do not confuse
the existing local gameplay fingerprint with a security hash. Metadata
changes need not invalidate comparable gameplay; define that policy
explicitly. Keep old revisions accessible for historical replay and
recalculation. No file paths or mutable array indexes in server IDs.

## 4. Scores, verification and PP

A completed scored run is saved locally independently of network state. The portable
`BuildCompletedRun` constructs an eligible local attempt snapshot; `RunResultsStore`
assigns local persistence identity and time. `locus_result_model` has no filesystem
dependency. Its local IDs and historical fields are not the future API schema.
A future authenticated submission contains an idempotency key, server
attempt/session identifier, published revision, exact rate and flags,
seed where applicable, and bounded replay bytes. The local summary is
a claim, not authority. The server obtains the chart and offset from its
immutable revision, validates supported format/rules/mods, reconstructs
grades and statistics with the portable judge, and computes performance
with server-selected versions. Client-provided stars, accuracy, PP,
username, timestamps and eligibility are not trusted.

Important gaps in the current scoring trace:
- It records judge operations in dispatch order with calibrated song
  seconds, not raw device input, key releases, monotonic wall-clock
  timeline or original pause/seek history.
- Replay offsets, timestamps and operation ordering are untrusted. Bind
  offset to canonical timing, bound timestamps and event counts, reject
  impossible state transitions under a defined submission policy. A
  Finish operation must not substitute for verification of a completed
  attempt. Do not sort existing events to hide invalid order.
- A fabricated perfect input trace can still be mathematically valid.
  Replay verification is not anti-cheat. Server-issued attempts and
  replay anomaly analysis mitigate some abuse but cannot prove human
  input on an untrusted client.
- Rate/mod eligibility is separate from local score eligibility.
  Define a ranking policy for HT/DT, EZ/HR, HD/FL, MR/RD, SD/PF,
  calibration and allowable presentation settings before opening
  competitive leaderboards. Autoplay, TEST, spectator and failed
  attempts never become ranked submissions.

Keep all verified attempts and original replay blobs independent of derived
PP. Difficulty V4 measures physical execution; READ stays diagnostic and
has zero ranking weight. [Performance V1](PERFORMANCE.md) is now a provisional pure calculation from
validated difficulty, actual grades and explicit mods. Local Results shows
PP and an owner-scoped best-score projection exist. The desktop scans committed summaries in a background worker and displays a compact Home profile; a dedicated, paginated local profile displays the same best-score projection. Calibrate the constants before competition. Store calculator versions and support
deterministic recalculation without overwriting source attempts.

Use a durable outbox for eligible submissions with bounded retries and
idempotency scoped to the authenticated user. Never transfer a previous
local user's history to a newly signed-in account automatically. Offline
runs remain local unless an explicitly defined verification/eligibility
policy accepts them; reconnecting must not silently grant ranked status.
Verification and leaderboard/PP projections must be transactional or
recoverable and rebuildable from verified attempts.

## 5. Accounts and native network integration

Desktop remains playable while signed out or the service is unavailable.
A local profile identity and authenticated server user ID are distinct. The desktop currently loads or creates one default local player in `player.json` and scopes result storage/history/replay access to that owner for the entire process. Result v3 records carry `playerId` and a random 128-bit attempt ID. Old unowned results remain on disk without migration. A future switcher must freeze the owner at launch, cancel old history requests and recreate the owner-scoped adapters; there is no switching UI or sign-in yet.
Scope history, cached account data and queued submissions to the correct
owner. Account switching cannot change the owner of an in-flight attempt.

Use browser-based native OAuth/OIDC authorization with PKCE and a
supported redirect mechanism; do not collect account passwords inside
the game. Use short-lived access credentials, OS credential storage for
refresh credentials, TLS, explicit sign-out/revocation and server-side
authorization on each private operation. Browser sessions use secure
cookies and CSRF protection as applicable. Never log credentials or
include them in diagnostic ZIPs.

One network owner handles timeouts, cancellation, backoff, request
correlation and connection state. No blocking network call in the
render, input, audio or judgement loop. Results and catalog presenters
consume immutable snapshots; a stale response cannot overwrite a newer
selection or account session. Keep remote catalog entries distinct from
installed charts; downloads become local installs only after validation.

## 6. Map publication and delivery

Upload flow: authenticated request -> private staging -> bounded validation
-> worker calculation -> immutable revision -> moderation/publication.
Packages are untrusted input. Validate archive structure/paths, counts,
compressed/expanded byte budgets, exact reads, duplicate names/IDs,
manifest, chart JSON, effective shared timing, audio duration and media
decoding. Restrict worker privileges, CPU, memory, runtime and temporary
disk; isolate media decoding. Fail atomically without publishing partial
revisions. Verify content digests before installing downloads; rate-limit
uploads/downloads and enforce per-account quotas. Do not reuse the local
merge behavior to overwrite a published immutable revision.

Suggested publication model: draft/private, pending review, unranked,
ranked/loved (policy to be defined), archived/disabled. Publication and
ranking eligibility are separate decisions. Only authorized owners or
moderators may update status. Public downloads should use controlled
object URLs/CDN delivery with revocation support. Implement uploader
permissions, reports, takedowns, appeals and repeat-infringer handling
before accepting public uploads; see the [hosting policy](../README.md#community-chart-packs-with-music--future-hosting-policy).
No legal workflow is currently operational.

## 7. Implementation gates (ordered)

| Gate | Deliverable | Acceptance |
| --- | --- | --- |
| W0 | Documentation and portable rules boundary | Core-only CI; no future capability described as implemented |
| W1 | Portable verifier + package validation boundary (verifier, structural package validation and SHA-256 content descriptor implemented; media/session policy pending) | Linux tests against exact chart/replay fixtures; reject mismatched offset, malformed input and unsupported rules; no desktop dependencies |
| W2 | Local run/profile ownership and PP contract (portable attempt model, local identity and owner-scoped storage implemented; experimental PP calculator and pure profile projection implemented; owner-scoped full history scan and compact Home summary implemented; local profile and paginated best scores implemented; account API and competitive policy pending) | Stable local attempt identity, explicit owner and immutable run snapshot; versioned calculator; offline regression tests |
| W3 | API/data contract and private backend | OpenAPI, migrations, auth, isolated dev environment, secrets and authorization tests; no public uploads |
| W4 | Private vertical slice | Publish one authorized test map, play, submit replay, server rejudge, show leaderboard and profile through the same API; retry idempotency |
| W5 | Catalog/download/upload workflow | Immutable revision mapping, paging, cancellation, safe install, quotas, rollback and fault recovery |
| W6 | Competitive and public readiness | Ranking/mod policies, anomaly review, moderation tools, legal/privacy workflows, backups, monitoring, load/abuse tests, real Windows acceptance |

W0 is preparatory, not a release gate that claims later features exist.
Track implementation status in PRs/issues and [roadmap](ROADMAP.md);
update contracts as code lands. Before public release also finish the
desktop [engineering acceptance](REFACTORING_PLAN.md).

## 8. Decisions still open

- Authoritative ranking metric, PP aggregation and recalculation policy.
- Exact online eligibility for rates, transforms, calibration and visual mods.
- Whether offline runs can ever be submitted for global ranking.
- How publication updates affect ranked status and existing best scores.
- Identity provider, hosting region/operator, budget, retention, privacy
  and copyright/reporting procedures.
- Web framework/API version details and deployment layout after a small
  working prototype.

This plan concerns a website and online backend for the existing desktop
game, not a browser port of the gameplay engine. Browser gameplay,
multiplayer, friends, chat and live spectator are separate future projects.
