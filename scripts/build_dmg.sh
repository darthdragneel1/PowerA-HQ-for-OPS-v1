#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="1.4.2"
APP_NAME="PowerA PC HQ"
APP_BUNDLE="$DIR/build/$APP_NAME.app"
DIST_DIR="$DIR/dist"
STAGE_DIR="$DIR/build/dmg_stage"
DMG_NAME="PowerA_PC_HQ_v${VERSION}_macOS.dmg"
DMG_OUTPUT="$DIST_DIR/$DMG_NAME"

echo "=========================================================="
echo "  Building $APP_NAME v$VERSION macOS DMG Installer"
echo "=========================================================="

# 1. Ensure the app bundle is up to date and standalone
echo "[1/6] Building and verifying application bundle..."
"$DIR/scripts/build_bundle.sh"

if [ ! -d "$APP_BUNDLE" ]; then
    echo "ERROR: App bundle not found at $APP_BUNDLE"
    exit 1
fi

# 2. Prepare staging directory
echo "[2/6] Preparing DMG staging environment..."
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR" "$DIST_DIR"

# Copy App bundle
echo "[3/6] Copying '$APP_NAME.app' into staging directory..."
cp -R -p "$APP_BUNDLE" "$STAGE_DIR/"

# Create standard Applications folder symlink
echo "[4/6] Creating /Applications drag-and-drop symlink..."
ln -s /Applications "$STAGE_DIR/Applications"

# Set custom volume icon if available
if [ -f "$DIR/assets/AppIcon.icns" ]; then
    cp "$DIR/assets/AppIcon.icns" "$STAGE_DIR/.VolumeIcon.icns"
    if command -v SetFile &>/dev/null; then
        SetFile -c icnC "$STAGE_DIR/.VolumeIcon.icns" 2>/dev/null || true
        SetFile -a C "$STAGE_DIR" 2>/dev/null || true
    fi
fi

# Add quick start instructions for end users
cat << 'EOF' > "$STAGE_DIR/QUICK_START.txt"
==========================================================
  PowerA PC HQ for macOS - Installation & Quick Start
==========================================================

1. INSTALLATION:
   - Drag "PowerA PC HQ" into the "Applications" folder.
   - Launch "PowerA PC HQ" from Launchpad or /Applications.

2. CONTROLLER SETUP:
   - Connect your PowerA OPS v1 Controller via:
     * Wired USB-C Cable (Recommended for lowest latency)
     * 2.4 GHz USB Wireless Dongle
   - The app will detect the controller immediately and wake
     up the thumbsticks and buttons.

3. FEATURES:
   - Real-time Stick, Trigger, and Button input visualizer
   - RGB Lighting customization (Static, Breathing, Rainbow Cycle)
   - Profile management (T1 Default, T2 Speed, T3 Hair Trigger)
   - Stick Deadzone & Sensitivity curve tuning
   - Dual motor rumble haptic test

Enjoy gaming on macOS!
==========================================================
EOF

# 3. Create compressed DMG image using hdiutil
echo "[5/6] Generating compressed disk image (UDZO)..."
rm -f "$DMG_OUTPUT" "$DIR/build/temp.dmg"

hdiutil create \
  -volname "$APP_NAME" \
  -srcfolder "$STAGE_DIR" \
  -ov \
  -format UDZO \
  -imagekey zlib-level=9 \
  "$DMG_OUTPUT"

# 4. Codesign DMG and generate SHA256 checksum
echo "[6/6] Applying ad-hoc signature and calculating checksum..."
codesign --force --sign - "$DMG_OUTPUT" 2>/dev/null || true

cd "$DIST_DIR"
shasum -a 256 "$DMG_NAME" > "$DMG_NAME.sha256"

# Clean up staging directory
rm -rf "$STAGE_DIR"

echo "=========================================================="
echo "  BUILD SUCCESSFUL!"
echo "  Installer: $DMG_OUTPUT"
echo "  Size:      $(du -h "$DMG_OUTPUT" | cut -f1)"
echo "  SHA256:    $(cat "$DMG_NAME.sha256" | cut -d ' ' -f1)"
echo "=========================================================="
