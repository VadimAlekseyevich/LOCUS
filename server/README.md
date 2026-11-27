# LOCUS backend — private development slice

.NET 10 (ASP.NET Core Minimal API), PostgreSQL 17, Npgsql 10.0.3.
The C++ worker remains the sole implementation of gameplay calculations.
This is a runnable private development service, **not a public deployment**.
There are no account creation, login, map upload or score submission endpoints.

## Local start

Install .NET 10 SDK, Docker and CMake (for the worker). Start a disposable
local PostgreSQL instance, replacing the sample password:

```sh
docker run --rm --name locus-dev-db -p 127.0.0.1:5432:5432 \
  -e POSTGRES_DB=locus -e POSTGRES_USER=locus \
  -e POSTGRES_PASSWORD=dev-only-change-me postgres:17
```

In another shell:

```sh
export ConnectionStrings__Locus='Host=127.0.0.1;Port=5432;Database=locus;Username=locus;Password=dev-only-change-me'
dotnet run --project server -- migrate
dotnet run --project server
```

Default URL: http://127.0.0.1:5080. Do not expose externally. The migration
is explicit, transactionally applied under an advisory lock, and records
a checksum. Re-running it is safe; an incompatible migration history
fails. The schema is now version 2. Numbered embedded migrations are append-only;
a valid older prefix is upgraded atomically. Gaps, unknown versions and
checksum mismatches are rejected. Readiness checks the complete history,
not just its largest version. Production will require distinct migration
and application database roles, TLS, backup and secret management.

```sh
curl http://127.0.0.1:5080/health/ready
curl 'http://127.0.0.1:5080/api/v1/maps?limit=20'
```

An empty catalog is expected until trusted publication is implemented.
See [HTTP contract](../protocol/openapi.yaml). Only current published
revisions appear in the catalog; only active users have visible profiles.
Leaderboards are exact-revision, restricted to explicitly ranked published
revisions and rank-approved Performance V1 scores, with one best score per
active player. The provisional ordering is PP, then accuracy, then attempt
ID. The response is a bounded top-N, not a global player ranking.

## Operator worker commands

Build the worker as described in [workers](../workers/README.md). Set
`LOCUS_WORKER_PATH` to its absolute executable path. Then run:

```sh
dotnet run --project server -- worker inspect /private/input.locuspack /private/empty-stage
dotnet run --project server -- worker verify /private/stage DIFFICULTY_ID GAMEPLAY_SHA256 AUDIO_DURATION_MS RATE_PERCENT MOD_FLAGS MOD_SEED /private/input.replay
```

The adapter uses an argument array (no shell), bounded stdout/stderr,
60-second wall timeout and process-tree termination. It checks the worker
response envelope and kind; it does not accept HTTP-controlled paths or
persist the response in PostgreSQL. It is not a resource sandbox.
The operator is responsible for trusted private files, independently
decoded duration and cleanup. No ranking approval is inferred from a
successful process. Worker output is for internal use only.

## Current boundaries

SQL defines server IDs, ownership, published revision references, attempt
idempotency and verified-score records. Schema v2 adds operator-only
external identity bindings: exact case-sensitive (issuer, subject) maps
to a server player, with at most one binding per issuer per player. No
identity is linked by display name, email or a desktop ID. This is storage,
not authentication; no identity provider has been selected. The HTTP API is read-only; the
CI fixture inserts synthetic records through SQL. No real publication or
submission transaction, account authentication, media decoding, score
approval workflow, object storage, global PP or frontend yet.
Next: authentication and a private authenticated publication/submission
vertical slice with validated worker DTOs and durable object references.

CI builds the actual C++ executable and .NET service, exercises concurrent
fresh migrations, a v1-to-v2 upgrade, invalid history rejection and identity
constraints on PostgreSQL, and checks the operator process bridge and HTTP
visibility/ranking/pagination. The local game has no network dependency.
