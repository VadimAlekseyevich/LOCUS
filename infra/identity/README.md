# Private identity provider

Keycloak 26.8.2 is the selected **development** OIDC provider. It is not
part of the game executable or the API process. The realm has no committed
users, credentials, signing keys or administrative access for the API.
The development image tag and registry digest are pinned for reproducibility;
review security releases and verify image provenance before deploying. The production provider may be
changed without changing the desktop protocol or scoring engine.

## Local development

Install Docker Compose and obtain a local TLS certificate trusted by your
OS/browser with IP SAN 127.0.0.1. Put the certificate and private key in
a private directory outside the repository as tls.crt and tls.key. Give
the container's non-root user read access to the files (prefer restricted
group permissions); never commit or publicly distribute private keys.
Do not disable TLS certificate checks.

Set LOCUS_IDENTITY_TLS_DIR to the absolute directory, and
LOCUS_IDENTITY_ADMIN / LOCUS_IDENTITY_PASSWORD to unique local bootstrap
credentials via your shell or local secret manager. From this directory:

```sh
docker compose up -d
```

The admin console is https://127.0.0.1:5444/admin/. The issuer is
https://127.0.0.1:5444/realms/locus. Create an invited test user in the
locus realm and set a password via Keycloak. Copy the user's immutable
ID (subject), not their username or email. For an existing local dev
PostgreSQL database, apply migrations then provision a new server player:

```sh
export Auth__Issuer=https://127.0.0.1:5444/realms/locus
export Auth__Audience=https://api.locus.test
dotnet run --project ../../server -- provision-player SUBJECT TestPlayer
dotnet run --project ../../server
```

The provision command creates a new player and identity in a single
transaction. Duplicate names or subjects fail without modifying existing
players. The operator must verify the subject in the trusted realm
before provisioning. Do not use the player's local desktop ID.

Native client contract: public client locus-desktop, authorization code
with mandatory S256 PKCE, loopback IP redirect with ephemeral port,
scope openid locus.player. The callback must include the exact issuer (RFC
9207). Native C++ PKCE/state/nonce generation and callback parsing now exist;
the listener, HTTPS exchange, ID-token checks and credential storage remain
pending. A native client must validate state and nonce,
use the system browser, bind the listener to loopback only, and securely
store refresh credentials. No password grant, client secret, implicit
grant, service-account token or token exchange. Access tokens use RS256,
at+jwt, separate API audience and 120-second lifetime; refresh rotation
allows zero reuse. ID tokens are not API credentials. The dedicated player
scope explicitly maps the immutable Keycloak subject into the access token;
replacing default client scopes otherwise removes the built-in subject mapper.
For an existing realm, add the Subject (sub) protocol mapper to the
locus.player client scope (access token and introspection enabled). This
preserves existing user IDs; do not reset the database merely to update a mapper.

Registration is closed and no public account creation is implemented.
The local provider uses Keycloak dev mode and a disposable embedded
database; the named volume retains realm state between restarts. Import
does not overwrite an existing realm; inspect configuration after any
upgrade. For a disposable reset, docker compose down -v deletes all
identity data and invalidates existing subject mappings. Do not use this
command on a real account database.

## Production gates

Do not deploy this compose file publicly. Production needs optimized
Keycloak start mode, dedicated PostgreSQL database/role, real TLS and
hostname, restricted admin ingress, secret rotation, backups, MFA policy,
verified email and recovery, registration abuse protection, monitoring
and a tested update procedure. Bootstrap credentials must be removed
after setup. Revocation of refresh credentials ends future issuance;
already issued JWTs remain valid until expiry plus clock skew unless
LOCUS disables/unlinks the player. A session-specific immediate
revocation design is still pending. No public release is authorized by
this development setup.

References: [Keycloak server guide](https://www.keycloak.org/server/),
[OIDC native redirect](https://www.keycloak.org/securing-apps/oidc-layers),
[RFC 8252](https://www.rfc-editor.org/rfc/rfc8252),
[OAuth security BCP](https://www.rfc-editor.org/rfc/rfc9700).
