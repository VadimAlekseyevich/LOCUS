# Private content-addressed object storage

The initial single-host backend implements IBlobStore on a dedicated,
durable local Linux filesystem. S3-compatible storage remains the intended
multi-host backend behind the same content-identity contract; no S3 service
is required to exercise the first private publication transaction.

Logical key: sha256/<64 lowercase hex>. Physical layout: root/HH/HASH,
where HH is the first two hex digits. Object references carry key, exact
byte length and SHA-256. Filenames, owner and original extension are
metadata, not part of object identity. No user-controlled relative path
enters the store. Maximum blob size is 320 MiB.

The operator provisions a private mode-0700 root owned by the non-root
service UID and trusted ancestors. Never mount it into any worker, serve
it as a static web root, or allow concurrent untrusted writers. Shards
are mode 0700; committed objects are mode 0400. These are application
immutability guarantees, not protection against root or a compromised
storage owner. The directory tree must not be mutable by other users.

A put streams and hashes the source, enforces exact size, flushes data
and mode to disk, uses renameat2(RENAME_NOREPLACE) in the same directory,
and fsyncs directories. Concurrent identical puts converge to one file;
a corrupt existing object fails closed, never gets replaced. Reads verify
length and digest on the opened stream before exposing it. No silent
fallback to weaker rename semantics. Use a filesystem with verified
durability guarantees; provision root durably before use.

The original uncompressed .locuspack and individual assets are both
stored: exact archive download and cross-revision asset deduplication
have different roles. This costs extra storage in the private slice.
A storedPackage receipt is not a publication. PostgreSQL references must
be committed only after all required object puts return successfully.
An interrupted job can leave unreferenced complete objects or pending
files, but must not expose a partial object under a content key.
No automatic GC yet; reconcile only with a future reference-aware,
race-safe lifecycle protocol. For now clean pending names only when all
writers are stopped. Production requires capacity/admission quotas,
backups, integrity scrubbing, independent read/write credentials,
durable staging, object lifecycle and S3 conditional-create semantics.
