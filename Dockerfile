# =============================================================================
# Stage 1: Base builder image with all dependencies
# =============================================================================
FROM ubuntu:24.04 AS builder-base

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -qq && apt-get install -y \
    autoconf \
    automake \
    build-essential \
    cmake \
    git-core \
    libtool \
    meson \
    ninja-build \
    pkg-config \
    texinfo \
    wget \
    yasm \
    nasm \
    zlib1g-dev \
    libbz2-dev \
    && rm -rf /var/lib/apt/lists/*

# =============================================================================
# Stage 2: Build libaom
# =============================================================================
FROM builder-base AS libaom-builder

WORKDIR /build/libaom

# Copy only libaom source
COPY external/libaom /build/libaom

# Build libaom as static library (decoder only)
RUN mkdir -p aom_build && cd aom_build && \
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DENABLE_DOCS=OFF \
        -DENABLE_EXAMPLES=OFF \
        -DENABLE_TESTS=OFF \
        -DENABLE_TOOLS=OFF \
        -DCONFIG_AV1_ENCODER=0 \
        -DCONFIG_MULTITHREAD=1 \
        -DCONFIG_PIC=1 && \
    cmake --build . --parallel $(nproc)

# =============================================================================
# Stage 3: Build ffmpeg with vendored libaom
# =============================================================================
FROM builder-base AS ffmpeg-builder

WORKDIR /build

# Copy libaom build artifacts from previous stage
COPY --from=libaom-builder /build/libaom /build/libaom

# Copy ffmpeg source
COPY external/ffmpeg /build/ffmpeg

# Copy VideoParser headers (required by our ffmpeg fork)
# ffmpeg includes "../../../VideoParser/include/shared.h" from libavutil/frame.h
# From /build/ffmpeg/libavutil/frame.h, that resolves to /VideoParser/include/shared.h
COPY VideoParser /VideoParser

WORKDIR /build/ffmpeg

# Configure and build ffmpeg with vendored libaom
ENV PKG_CONFIG_PATH=/build/libaom/aom_build
ENV SRC_PATH=/build/ffmpeg

RUN ./configure \
    --disable-programs \
    --disable-doc \
    --disable-stripping \
    --enable-static \
    --enable-pthreads \
    --enable-debug=2 \
    --disable-avfilter \
    --disable-swscale \
    --disable-swresample \
    --disable-audiotoolbox \
    --disable-videotoolbox \
    --disable-vaapi \
    --disable-vdpau \
    --disable-vulkan \
    --disable-securetransport \
    --disable-iconv \
    --disable-libdrm \
    --disable-encoders \
    --disable-muxers \
    --disable-outdevs \
    --disable-bsfs \
    --disable-indevs \
    --disable-protocols \
    --enable-protocol=file \
    --disable-parsers \
    --enable-parser=h264 \
    --enable-parser=hevc \
    --enable-parser=vp9 \
    --enable-parser=av1 \
    --enable-parser=vorbis \
    --disable-decoder=tiff \
    --disable-demuxers \
    --enable-demuxer=h264 \
    --enable-demuxer=hevc \
    --enable-demuxer=avi \
    --enable-demuxer=matroska \
    --enable-demuxer=mov \
    --enable-demuxer=mpegvideo \
    --enable-demuxer=mpegts \
    --enable-libaom \
    '--extra-cflags=-I/build/libaom -I/build/libaom/aom_build' \
    '--extra-ldflags=-L/build/libaom/aom_build' \
    --disable-inline-asm \
    && make -j$(nproc)

# =============================================================================
# Stage 4: Build videoparser
# =============================================================================
FROM builder-base AS videoparser-builder

WORKDIR /app

# Copy libaom from builder
COPY --from=libaom-builder /build/libaom /app/external/libaom

# Copy ffmpeg from builder
COPY --from=ffmpeg-builder /build/ffmpeg /app/external/ffmpeg

# Copy videoparser source (excluding external which we already have)
COPY CMakeLists.txt /app/
COPY VideoParser /app/VideoParser
COPY VideoParserCli /app/VideoParserCli
COPY util /app/util

# Build videoparser (skip ffmpeg build since it's already done in previous stage)
RUN mkdir -p build && cd build && \
    cmake -DSKIP_FFMPEG_BUILD=ON .. && \
    cmake --build .

# =============================================================================
# Stage 5: Final minimal runtime image
# =============================================================================
FROM ubuntu:24.04 AS runtime

# Install only runtime dependencies
RUN apt-get update -qq && apt-get install -y --no-install-recommends \
    libbz2-1.0 \
    zlib1g \
    && rm -rf /var/lib/apt/lists/*

# Copy the built binary
COPY --from=videoparser-builder /app/build/VideoParserCli/video-parser /usr/local/bin/video-parser

ENTRYPOINT ["video-parser"]
CMD ["--help"]
