#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
DIST_DIR="$SCRIPT_DIR/../dist"
PACKAGE="ikigai"
VERSION=$(grep '#define IK_VERSION "' "$PROJECT_DIR/src/version.h" | cut -d'"' -f2)

echo "=== Debian Package Build ==="
echo "Project: $PROJECT_DIR"
echo "Build dir: $BUILD_DIR"
echo "Dist dir: $DIST_DIR"
echo ""

# Step 1: Clean and create build directory
echo "Preparing build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Step 2: Extract tarball
TARBALL="$DIST_DIR/${PACKAGE}-${VERSION}.tar.gz"
if [ ! -f "$TARBALL" ]; then
    echo "Error: Tarball not found: $TARBALL"
    echo "Run 'make dist' first to create the source tarball"
    exit 1
fi

echo "Extracting $TARBALL..."
tar -xzf "$TARBALL" -C "$BUILD_DIR"

# Step 3: Copy debian directory into extracted source and update version
echo "Copying debian packaging files..."
cp -r "$SCRIPT_DIR/packaging" "$BUILD_DIR/${PACKAGE}-${VERSION}/debian"

echo "Updating version to $VERSION in debian files..."
sed -i "s/__VERSION__/$VERSION/g" "$BUILD_DIR/${PACKAGE}-${VERSION}/debian/changelog"

# Step 4: Build the package
echo "Building Debian package..."
cd "$BUILD_DIR/${PACKAGE}-${VERSION}"
dpkg-buildpackage -us -uc -b

# Step 5: Copy results to dist
echo ""
echo "Copying packages to dist/..."
mkdir -p "$DIST_DIR"
cp "$BUILD_DIR"/*.deb "$DIST_DIR/" 2>/dev/null || true

echo ""
echo "=== Build Complete ==="
ls -lh "$DIST_DIR"/*.deb
echo ""
echo "Package contents left in: $BUILD_DIR"
