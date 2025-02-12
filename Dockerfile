FROM ubuntu:24.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install required packages
RUN apt-get update -qq && apt-get install -y \
    autoconf \
    automake \
    build-essential \
    cmake \
    git-core \
    libsdl2-dev \
    libtool \
    libunistring-dev \
    meson \
    ninja-build \
    pkg-config \
    texinfo \
    wget \
    yasm \
    nasm \
    zlib1g-dev \
    libbz2-dev \
    libaom-dev \
    # Clean up apt cache to reduce image size
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy the source code
COPY . .

# Build ffmpeg first, but clean it before!
RUN ./util/build-ffmpeg.sh --clean

# Build the project
RUN ./util/build-cmake.sh

# Set the entrypoint to the video-parser CLI
ENTRYPOINT ["/app/build/VideoParserCli/video-parser"]

# Default command (can be overridden)
CMD ["--help"]
