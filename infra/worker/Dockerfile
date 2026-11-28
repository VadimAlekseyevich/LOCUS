# Private development image. Build the portable worker on Ubuntu 24.04
# first; only the executable and runtime libraries enter this image.
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends libstdc++6 \
    && rm -rf /var/lib/apt/lists/*
COPY --chmod=0555 locus_worker /usr/local/bin/locus_worker
USER 65532:65532
ENTRYPOINT ["/usr/local/bin/locus_worker"]
