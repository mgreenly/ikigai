FROM archlinux:latest

# Enable debug repositories for glibc debug symbols
RUN echo -e "\n[core-debug]\nInclude = /etc/pacman.d/mirrorlist\n\n[extra-debug]\nInclude = /etc/pacman.d/mirrorlist" >> /etc/pacman.conf

# Update system and install base development tools
# Refresh package database first
RUN pacman -Sy --noconfirm

# Install packages (without full system upgrade to avoid conflicts)
RUN pacman -S --noconfirm --needed \
    base-devel \
    git \
    sudo \
    uncrustify \
    check \
    talloc \
    curl \
    util-linux \
    libb64 \
    libutf8proc \
    lcov \
    valgrind \
    wget \
    tar \
    xz \
    cmake \
    pkgconf \
    libmicrohttpd \
    gnutls \
    zlib \
    autogen \
    autoconf \
    automake \
    libtool \
    perl \
    perl-capture-tiny \
    perl-datetime \
    python \
    cppunit

# Install glibc debug symbols from debug repository
RUN pacman -S --noconfirm glibc-debug

# Create mk/arch.mk with Arch-specific package list
RUN mkdir -p /workspace/mk && cat > /workspace/mk/arch.mk <<'EOF'
# Arch package manager overrides
PKG_UPDATE = pacman -Sy
PKG_INSTALL = pacman -S --noconfirm
PACKAGES = \
	base-devel \
	uncrustify \
	check \
	talloc \
	curl \
	util-linux \
	libb64 \
	libutf8proc \
	lcov \
	valgrind \
	git \
	sudo \
	wget \
	tar \
	xz \
	cmake \
	pkgconf \
	libmicrohttpd \
	gnutls \
	zlib \
	autogen \
	autoconf \
	automake \
	libtool \
	perl \
	perl-capture-tiny \
	perl-datetime \
	python \
	cppunit
EOF

# Set working directory
WORKDIR /workspace

# Build libsubunit from source (required by Check framework tests)
RUN cd /tmp && \
    git clone --depth 1 --branch 1.4.4 https://github.com/testing-cabal/subunit.git && \
    cd subunit && \
    autoreconf -i && \
    ./configure --prefix=/usr && \
    make && \
    make install && \
    cd /tmp && rm -rf subunit

# Build complexity from source (not available in Arch repos)
RUN cd /tmp && \
    wget https://ftp.gnu.org/gnu/complexity/complexity-1.13.tar.xz && \
    tar xf complexity-1.13.tar.xz && \
    cd complexity-1.13 && \
    cd src && autogen opts.def && cd .. && \
    ./configure && \
    make V=1 && \
    make install && \
    cd /tmp && rm -rf complexity-*

# Build ulfius dependencies: orcania (static library)
RUN cd /tmp && \
    git clone --depth 1 --branch v2.3.3 https://github.com/babelouest/orcania.git && \
    cd orcania && \
    mkdir build && cd build && \
    cmake .. -DBUILD_STATIC=ON -DBUILD_SHARED=OFF && \
    make && \
    make install && \
    cd /tmp && rm -rf orcania

# Build ulfius dependencies: yder (static library)
RUN cd /tmp && \
    git clone --depth 1 --branch v1.4.20 https://github.com/babelouest/yder.git && \
    cd yder && \
    mkdir build && cd build && \
    cmake .. -DBUILD_STATIC=ON -DBUILD_SHARED=OFF -DWITH_JOURNALD=off && \
    make && \
    make install && \
    cd /tmp && rm -rf yder

# Build ulfius (static library)
RUN cd /tmp && \
    git clone --depth 1 --branch v2.7.15 https://github.com/babelouest/ulfius.git && \
    cd ulfius && \
    mkdir build && cd build && \
    cmake .. -DBUILD_STATIC=ON -DBUILD_SHARED=OFF -DBUILD_UWSC=OFF && \
    make && \
    make install && \
    cd /tmp && rm -rf ulfius

# Update linker cache
RUN ldconfig

# Set library linking preferences - static link libraries built from source
ENV SERVER_LIBS="-lcurl -ltalloc -luuid -lb64 -lpthread"
ENV SERVER_STATIC_LIBS="-lulfius -lyder -lorcania"

# Default command
CMD ["/bin/bash"]
