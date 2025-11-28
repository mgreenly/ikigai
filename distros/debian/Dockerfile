FROM debian:trixie

# Install build dependencies
RUN apt-get update && apt-get install -y \
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
    lcov \
    valgrind \
    git \
    sudo \
    debhelper \
    dpkg-dev \
    fakeroot \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /workspace

# Default command
CMD ["/bin/bash"]
