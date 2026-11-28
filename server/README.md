# LOCUS backend — private development slice

.NET 10 (ASP.NET Core Minimal API), PostgreSQL 17, Npgsql 10.0.3.
The C++ worker remains the sole implementation of gameplay calculations.
This is a runnable private development service, **not a public deployment**.
There are no account creation, token issuance, map upload or score submission endpoints.
The optional JWT bearer resource-server boundary authenticates already provisioned players.

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

## Private player authentication

Without both settings below, no authenticated endpoint is usable. If only
one setting is supplied or the issuer is not HTTPS, startup fails.
For the selected local Keycloak realm see [identity setup](../infra/identity/README.md).
Alternatively configure a trusted OIDC issuer which issues delegated user access tokens:

```sh
export Auth__Issuer='https://identity.example'
export Auth__Audience='https://api.example'
```

These are placeholders, not a deployed identity provider. The API uses
Microsoft JWT Bearer, HTTPS discovery/JWKS, exact issuer and audience,
RS256 signatures, access-token type at+jwt or application/at+jwt,
expiration and 30-second clock skew. It requires the exact scope
`locus.player` and a case-sensitive issuer/subject mapping to an active
server player. The issuer must grant this scope only to delegated human
user sessions, not client-credentials grants, and issue short-lived access
tokens. Do not reuse a client ID as the API audience. No ID tokens,
local IDs, emails, client-supplied player IDs or roles authorize requests.
No bearer token is persisted. Unmapped/disabled players are denied on
each private request; unlinking or disabling takes effect on the next
authorization decision. Private routes require this policy by default;
only explicit public reads and health routes are anonymous.

`GET /api/v1/me` returns the authenticated server player's ID, display name
and creation time. Invalid/missing tokens receive 401, valid tokens lacking
the required player authorization receive 403. Responses are not cacheable.
The service still binds to loopback by default; require HTTPS at ingress
before any remote access. The private Keycloak realm supports authorization code with mandatory S256
PKCE and rotating refresh credentials. The operator-only provision-player
command creates a new server player and identity atomically; delegated or
impersonated tokens with an act claim are denied. The desktop has optional native browser sign-in and session restoration;
production deployment and immediate session revocation remain pending. CI uses a
disposable HTTPS discovery issuer and test signing keys, never a production
validation bypass.

## Operator worker commands

Build the portable worker as described in [workers](../workers/README.md).
Linux container mode is the default and requires an explicitly selected
local image ID, a non-root operator with access to the trusted local Docker
daemon, and a dedicated bounded staging mount. See [worker isolation](../infra/worker/README.md)
for the exact setup. The HTTP API must not have daemon access.

```sh
dotnet run --project server -- worker inspect /private/input.locuspack /private/stage
dotnet run --project server -- worker verify /private/stage DIFFICULTY_ID GAMEPLAY_SHA256 AUDIO_DURATION_MS RATE_PERCENT MOD_FLAGS MOD_SEED /private/input.replay
```

The adapter uses an argument array, clears inherited environment, limits
stdout/stderr by bytes, enforces a 60-second job budget and removes the
named isolated container even on cancellation. It checks the worker response
envelope and kind. Typed response validation and persistence remain pending.
The operator owns trusted private paths, independently decoded duration,
immutable stage contents and cleanup. No ranking approval is inferred.

Explicit `LOCUS_WORKER_MODE=trusted-local` and absolute `LOCUS_WORKER_PATH`
enable direct execution for trusted development inputs only. This mode
is not a sandbox and must never receive uploaded files.

## Current boundaries

SQL defines server IDs, ownership, published revision references, attempt
idempotency and verified-score records. Schema v2 adds operator-only
external identity bindings: exact case-sensitive (issuer, subject) maps
to a server player, with at most one binding per issuer per player. No
identity is linked by display name, email or a desktop ID. The API can resolve validated bearer tokens to these bindings; no production
identity deployment exists; Keycloak is selected for private development. The HTTP API is read-only; the CI fixture
inserts synthetic records through SQL. No real publication or
submission transaction, media decoding, score
approval workflow, object storage, global PP or frontend yet.
Next: typed worker DTO validation, media isolation, and a private
authenticated publication/submission vertical slice with
validated worker DTOs and durable object references.

CI builds the actual C++ executable and .NET service, exercises concurrent
fresh migrations, a v1-to-v2 upgrade, invalid history rejection and identity
constraints on PostgreSQL, and checks the operator process bridge and HTTP
visibility/ranking/pagination, real TLS discovery, invalid JWT rejection and
active-player authorization. CI also exercises the actual imported Keycloak
realm, browser-form authorization code with PKCE, provisioning rollback,
ID-token rejection and refresh rotation. The local game has no network dependency.
