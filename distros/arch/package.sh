#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
DIST_DIR="$SCRIPT_DIR/../dist"
PACKAGE="ikigai"
VERSION=$(grep '#define IK_VERSION "' "$PROJECT_DIR/src/version.h" | cut -d'"' -f2)

echo "=== Arch Package Build ==="
echo "Project: $PROJECT_DIR"
echo "Build dir: $BUILD_DIR"
echo "Dist dir: $DIST_DIR"
echo ""

# Step 1: Clean and create build directory
echo "Preparing build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Step 2: Copy PKGBUILD and tarball
TARBALL="$DIST_DIR/${PACKAGE}-${VERSION}.tar.gz"
if [ ! -f "$TARBALL" ]; then
    echo "Error: Tarball not found: $TARBALL"
    echo "Run 'make dist' first to create the source tarball"
    exit 1
fi

echo "Copying and updating PKGBUILD with version $VERSION..."
sed "s/__VERSION__/$VERSION/g" "$SCRIPT_DIR/packaging/PKGBUILD" > "$BUILD_DIR/PKGBUILD"

echo "Copying tarball..."
cp "$TARBALL" "$BUILD_DIR/"

# Step 3: Build the package with makepkg
echo "Building Arch package..."
cd "$BUILD_DIR"

# makepkg needs to run as non-root, but we're likely running as root in Docker
# Create a build user if we're root
if [ "$(id -u)" -eq 0 ]; then
    echo "Running as root, creating build user..."
    useradd -m -G wheel builder || true
    echo "builder ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/builder
    chown -R builder:builder "$BUILD_DIR"
    su - builder -c "cd '$BUILD_DIR' && makepkg --nodeps --noconfirm"
else
    makepkg --nodeps --noconfirm
fi

# Step 4: Copy results to dist
echo ""
echo "Copying packages to dist/..."
mkdir -p "$DIST_DIR"
cp "$BUILD_DIR"/*.pkg.tar.zst "$DIST_DIR/" 2>/dev/null || true

echo ""
echo "=== Build Complete ==="
ls -lh "$DIST_DIR"/*.pkg.tar.zst
echo ""
echo "Package contents left in: $BUILD_DIR"
