#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
DIST_DIR="$SCRIPT_DIR/../dist"
PACKAGE="ikigai"
VERSION=$(grep '#define IK_VERSION "' "$PROJECT_DIR/src/version.h" | cut -d'"' -f2)

echo "=== Fedora Package Build ==="
echo "Project: $PROJECT_DIR"
echo "Build dir: $BUILD_DIR"
echo "Dist dir: $DIST_DIR"
echo ""

# Step 1: Clean and create build directory
echo "Preparing build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Step 2: Setup rpmbuild directory structure
echo "Setting up rpmbuild directory structure..."
mkdir -p "$BUILD_DIR/rpmbuild"/{SOURCES,SPECS,BUILD,RPMS,SRPMS}

# Step 3: Copy tarball and spec file
TARBALL="$DIST_DIR/${PACKAGE}-${VERSION}.tar.gz"
if [ ! -f "$TARBALL" ]; then
    echo "Error: Tarball not found: $TARBALL"
    echo "Run 'make dist' first to create the source tarball"
    exit 1
fi

echo "Copying source tarball..."
cp "$TARBALL" "$BUILD_DIR/rpmbuild/SOURCES/"

echo "Copying and updating spec file with version $VERSION..."
sed "s/__VERSION__/$VERSION/g" "$SCRIPT_DIR/packaging/ikigai.spec" > "$BUILD_DIR/rpmbuild/SPECS/ikigai.spec"

# Step 4: Build the RPM package
echo "Building RPM package..."
cd "$BUILD_DIR"
rpmbuild --define "_topdir $BUILD_DIR/rpmbuild" \
         -bb "$BUILD_DIR/rpmbuild/SPECS/ikigai.spec"

# Step 5: Copy results to dist
echo ""
echo "Copying packages to dist/..."
mkdir -p "$DIST_DIR"
cp "$BUILD_DIR"/rpmbuild/RPMS/*/*.rpm "$DIST_DIR/" 2>/dev/null || true

echo ""
echo "=== Build Complete ==="
ls -lh "$DIST_DIR"/*.rpm
echo ""
echo "Package contents left in: $BUILD_DIR"
