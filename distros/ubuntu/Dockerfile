FROM ubuntu:24.04

# Install build dependencies (matching GitHub CI)
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
    libpq-dev \
    libxkbcommon-dev \
    postgresql-client \
    lcov \
    valgrind \
    git \
    sudo \
    libxml2-dev \
    bc \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /workspace

# Default command
CMD ["/bin/bash"]
