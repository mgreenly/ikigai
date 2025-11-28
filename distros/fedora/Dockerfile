FROM fedora:latest

# Create mk/fedora.mk with Fedora-specific package list and package manager
RUN mkdir -p /workspace/mk && cat > /workspace/mk/fedora.mk <<'EOF'
# Fedora package manager overrides
PKG_UPDATE = true
PKG_INSTALL = dnf install -y
PACKAGES = \
	gcc \
	gcc-c++ \
	uncrustify \
	check-devel \
	libtalloc-devel \
	libcurl-devel \
	libuuid-devel \
	libb64-devel \
	utf8proc-devel \
	valgrind \
	git \
	sudo \
	wget \
	tar \
	xz \
	cmake \
	pkgconfig \
	libmicrohttpd-devel \
	gnutls-devel \
	zlib-devel \
	autogen \
	autogen-libopts-devel \
	autoconf \
	automake \
	libtool \
	perl \
	perl-Capture-Tiny \
	perl-DateTime \
	libasan \
	libubsan \
	libtsan \
	rpm-build \
	rpmdevtools
EOF

# Set working directory
WORKDIR /workspace

# Install make first so we can use the Makefile
RUN dnf install -y make

# Copy Makefile to use install-deps target
COPY Makefile /workspace/

# Install base dependencies using Makefile
RUN make DISTRO=fedora install-deps

# Build missing packages from source (complexity, lcov, ulfius)
# Install lcov 2.3.2 for GCC 15 compatibility
RUN cd /tmp && \
    git clone --depth 1 --branch v2.3.2 https://github.com/linux-test-project/lcov.git && \
    cd lcov && \
    make install && \
    cd /tmp && rm -rf lcov

# Complexity requires regenerating autogen files on newer systems
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
