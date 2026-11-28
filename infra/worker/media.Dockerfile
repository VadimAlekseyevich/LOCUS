# Separate decoder image; only the media worker receives codec libraries.
FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends g++ pkg-config libavformat-dev libavcodec-dev libavutil-dev \
    && rm -rf /var/lib/apt/lists/*
COPY workers/media/main.cpp /src/main.cpp
RUN g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror /src/main.cpp \
    $(pkg-config --cflags --libs libavformat libavcodec libavutil) -o /locus_media

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends libavformat60 libavcodec60 libavutil58 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build --chmod=0555 /locus_media /usr/local/bin/locus_media
USER 65532:65532
ENTRYPOINT ["/usr/local/bin/locus_media"]
