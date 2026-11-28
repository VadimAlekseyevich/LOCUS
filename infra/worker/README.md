# Private Linux worker isolation

The worker command defaults to isolated Linux container execution. It
requires a trusted local rootful Docker daemon with support for
bind-recursive=disabled, cgroups resource limits and the default seccomp
and host security profiles. The operator is non-root but Docker access is
root-equivalent: do not give the HTTP API this permission, and do not
mount a daemon socket in the worker. Production scheduling/identity is
a separate gate.

Build on Ubuntu 24.04, matching the runtime image:

```sh
cmake -S . -B build-worker -DLOCUS_BUILD_APP=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-worker --target locus_worker
mkdir -p build-worker/image
cp build-worker/locus_worker build-worker/image/
docker build -t locus-worker-dev -f infra/worker/Dockerfile build-worker/image
export LOCUS_WORKER_IMAGE="$(docker image inspect --format '{{.Id}}' locus-worker-dev)"
```

The adapter accepts an immutable local image ID only; it never pulls
at job execution time. The private development Dockerfile uses an Ubuntu
release tag and distro packages, not yet a reproducible production image.
Pin verified base/package digests, scan/sign the image and review its
runtime security profile before deployment.

For each inspection allocate a fresh private, dedicated tmpfs mount:

```sh
stage="$(mktemp -d)"
sudo mount -t tmpfs -o size=384m,nr_inodes=1024,nosuid,nodev,noexec,mode=0700,uid="$(id -u)",gid="$(id -g)" tmpfs "$stage"
# input is an operator-owned regular file outside stage
dotnet run --project server -- worker inspect /absolute/private/input.locuspack "$stage"
# Verify using the same stage, or a trusted immutable revision.
# After use, including failure:
sudo umount "$stage"
rmdir "$stage"
```

The operator must retain exclusive control of the source paths and stage,
without concurrent writers. The adapter rejects symbolic path components,
nested stage mounts, unbounded or nonempty inspection stages, and input
files inside the stage. It mounts only the input file (read-only) and
stage (writable for inspect, read-only for verify), not the repository,
credentials, database, object storage or host working directory. Mount
validation assumes a local daemon sharing the operator's mount namespace.

Each invocation creates a fresh container with one CPU quota, 768 MiB
RAM including swap limit, 32 PIDs, 64 open files, 256 MiB max file size,
no core dumps, no network, no IPC sharing, all capabilities dropped,
no-new-privileges, non-root operator UID/GID, read-only root and no
persistent container logging. Inspection stage is capped at 384 MiB and
1024 inodes (part of memory pressure); tune budgets against dense charts.
The 60-second process budget includes container startup. Cleanup has a
separate 10-second budget. A daemon failure can leave an orphan; the
operator must reconcile containers labeled org.locus.worker=1 after
recovery. Do not run a broad reaper while live jobs are active.

This is defense in depth, not a proof against kernel/container escapes.
Aggregate admission/concurrency quotas, dedicated worker host, hardened
production runtime, media decoder isolation, durable staging and strict
typed output validation are still required. Explicit trusted-local mode
is for private, already trusted inputs only; never use it for uploads.
