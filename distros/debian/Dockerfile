FROM debian:13.2@sha256:c71b05eac0b20adb4cdcc9f7b052227efd7da381ad10bb92f972e8eae7c6cdc9

# Install build dependencies
RUN apt-get update && apt-get upgrade -y && apt-get install -y \
    build-essential \
    uncrustify \
    complexity \
    check \
    libtalloc-dev \
    libulfius-dev \
    libcurl4-gnutls-dev \
    uuid-dev \
    libb64-dev \
    libutf8proc-dev \
    libpq-dev \
    libxkbcommon-dev \
    postgresql-client \
    lcov \
    valgrind \
    git \
    sudo \
    libxml2-dev \
    bc \
    debhelper \
    dpkg-dev \
    fakeroot \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /workspace

# Default command
CMD ["/bin/bash"]
