#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_BUNDLE="$DIR/build/PowerA PC HQ.app"
CONTENTS="$APP_BUNDLE/Contents"
MACOS="$CONTENTS/MacOS"
RESOURCES="$CONTENTS/Resources"

echo "=== Building PowerA PC HQ for macOS Bundle ==="
mkdir -p "$MACOS" "$RESOURCES"

# Detect Homebrew prefix (Apple Silicon: /opt/homebrew, Intel: /usr/local)
if [ -d "/opt/homebrew" ]; then
    BREW_PREFIX="/opt/homebrew"
elif [ -d "/usr/local" ]; then
    BREW_PREFIX="/usr/local"
else
    BREW_PREFIX="$(brew --prefix 2>/dev/null || echo "/opt/homebrew")"
fi

# 1. Compile driver daemon
echo "[1/4] Compiling native driver daemon..."
clang -O3 -Wall \
  -I"$DIR/driver" \
  -I"$BREW_PREFIX/include" \
  -I"$BREW_PREFIX/include/hidapi" \
  -L"$BREW_PREFIX/lib" \
  -lhidapi \
  -framework IOKit -framework CoreFoundation \
  "$DIR/driver/powera_protocol.c" \
  "$DIR/driver/powera_driver.c" \
  -o "$MACOS/powera-driver"

# Also copy to bin/ for standalone CLI use
mkdir -p "$DIR/bin"
cp "$MACOS/powera-driver" "$DIR/bin/powera-driver"

# 2. Compile Karabiner bridge tool
echo "[2/5] Compiling Karabiner-Elements bridge tool..."
clang++ -std=c++17 -O3 -Wall \
  -I"$BREW_PREFIX/include" \
  -I"$BREW_PREFIX/include/hidapi" \
  -L"$BREW_PREFIX/lib" \
  -lhidapi \
  -framework ApplicationServices -framework IOKit -framework CoreFoundation \
  "$DIR/tools/powera_karabiner_bridge.cpp" \
  -o "$MACOS/powera-karabiner-bridge"

cp "$MACOS/powera-karabiner-bridge" "$DIR/bin/powera-karabiner-bridge"

# 3. Compile Cocoa GUI App
echo "[3/6] Compiling native Cocoa application..."
clang -O3 -Wall \
  -framework Cocoa -framework WebKit \
  "$DIR/src/main.m" \
  -o "$MACOS/PowerA_PC_HQ"

cp "$MACOS/PowerA_PC_HQ" "$DIR/bin/PowerA_PC_HQ"

# 4. Bundle dynamic libraries into Contents/Frameworks for 100% standalone portability
echo "[4/6] Bundling dynamic libraries (hidapi) for zero-dependency execution..."
FRAMEWORKS="$CONTENTS/Frameworks"
mkdir -p "$FRAMEWORKS"
if [ -f "$BREW_PREFIX/opt/hidapi/lib/libhidapi.0.dylib" ]; then
    cp -L "$BREW_PREFIX/opt/hidapi/lib/libhidapi.0.dylib" "$FRAMEWORKS/libhidapi.0.dylib"
elif [ -f "$BREW_PREFIX/lib/libhidapi.0.dylib" ]; then
    cp -L "$BREW_PREFIX/lib/libhidapi.0.dylib" "$FRAMEWORKS/libhidapi.0.dylib"
elif [ -f "/opt/homebrew/lib/libhidapi.0.dylib" ]; then
    cp -L "/opt/homebrew/lib/libhidapi.0.dylib" "$FRAMEWORKS/libhidapi.0.dylib"
elif [ -f "/usr/local/lib/libhidapi.0.dylib" ]; then
    cp -L "/usr/local/lib/libhidapi.0.dylib" "$FRAMEWORKS/libhidapi.0.dylib"
fi

if [ -f "$FRAMEWORKS/libhidapi.0.dylib" ]; then
    chmod 755 "$FRAMEWORKS/libhidapi.0.dylib"
    install_name_tool -id "@rpath/libhidapi.0.dylib" "$FRAMEWORKS/libhidapi.0.dylib"
    
    install_name_tool -change "$BREW_PREFIX/opt/hidapi/lib/libhidapi.0.dylib" "@rpath/libhidapi.0.dylib" "$MACOS/powera-driver" 2>/dev/null || true
    install_name_tool -change "$BREW_PREFIX/lib/libhidapi.0.dylib" "@rpath/libhidapi.0.dylib" "$MACOS/powera-driver" 2>/dev/null || true
    install_name_tool -change "/opt/homebrew/opt/hidapi/lib/libhidapi.0.dylib" "@rpath/libhidapi.0.dylib" "$MACOS/powera-driver" 2>/dev/null || true
    install_name_tool -change "/opt/homebrew/lib/libhidapi.0.dylib" "@rpath/libhidapi.0.dylib" "$MACOS/powera-driver" 2>/dev/null || true
    install_name_tool -change "/usr/local/opt/hidapi/lib/libhidapi.0.dylib" "@rpath/libhidapi.0.dylib" "$MACOS/powera-driver" 2>/dev/null || true
    install_name_tool -change "/usr/local/lib/libhidapi.0.dylib" "@rpath/libhidapi.0.dylib" "$MACOS/powera-driver" 2>/dev/null || true
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$MACOS/powera-driver" 2>/dev/null || true
    
    install_name_tool -change "$BREW_PREFIX/opt/hidapi/lib/libhidapi.0.dylib" "@rpath/libhidapi.0.dylib" "$MACOS/powera-karabiner-bridge" 2>/dev/null || true
    install_name_tool -change "$BREW_PREFIX/lib/libhidapi.0.dylib" "@rpath/libhidapi.0.dylib" "$MACOS/powera-karabiner-bridge" 2>/dev/null || true
    install_name_tool -change "/opt/homebrew/opt/hidapi/lib/libhidapi.0.dylib" "@rpath/libhidapi.0.dylib" "$MACOS/powera-karabiner-bridge" 2>/dev/null || true
    install_name_tool -change "/opt/homebrew/lib/libhidapi.0.dylib" "@rpath/libhidapi.0.dylib" "$MACOS/powera-karabiner-bridge" 2>/dev/null || true
    install_name_tool -change "/usr/local/opt/hidapi/lib/libhidapi.0.dylib" "@rpath/libhidapi.0.dylib" "$MACOS/powera-karabiner-bridge" 2>/dev/null || true
    install_name_tool -change "/usr/local/lib/libhidapi.0.dylib" "@rpath/libhidapi.0.dylib" "$MACOS/powera-karabiner-bridge" 2>/dev/null || true
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$MACOS/powera-karabiner-bridge" 2>/dev/null || true
fi

# 4. Copy UI and assets into Resources
echo "[5/6] Packaging web UI, authentic assets, and profiles..."
mkdir -p "$RESOURCES/ui" "$RESOURCES/assets" "$RESOURCES/profiles"
cp -r "$DIR/src/ui/"* "$RESOURCES/ui/"
cp -r "$DIR/assets/"* "$RESOURCES/assets/"
cp -r "$DIR/profiles/"* "$RESOURCES/profiles/"
if [ -f "$DIR/assets/AppIcon.icns" ]; then
    cp "$DIR/assets/AppIcon.icns" "$RESOURCES/AppIcon.icns"
fi

# 4. Generate Info.plist
echo "[4/4] Generating Info.plist..."
cat << 'EOF' > "$CONTENTS/Info.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>PowerA_PC_HQ</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon</string>
    <key>CFBundleIdentifier</key>
    <string>com.powera.pchq</string>
    <key>CFBundleName</key>
    <string>PowerA PC HQ</string>
    <key>CFBundleDisplayName</key>
    <string>PowerA PC HQ</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.4.2</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSSupportsAutomaticGraphicsSwitching</key>
    <true/>
</dict>
</plist>
EOF

# 6. Ad-hoc codesign the entire bundle
echo "[6/6] Applying ad-hoc codesignature to bundle..."
codesign --force --deep --sign - "$APP_BUNDLE"

echo "=== Successfully built: '$APP_BUNDLE' ==="

# 5. Sync to ~/Applications if directory exists
USER_APPS="$HOME/Applications"
if [ -d "$USER_APPS" ]; then
    echo "[5/5] Deploying to $USER_APPS/PowerA PC HQ.app..."
    rm -rf "$USER_APPS/PowerA PC HQ.app"
    cp -R "$APP_BUNDLE" "$USER_APPS/PowerA PC HQ.app"
    echo "=== Deployed cleanly to $USER_APPS/PowerA PC HQ.app ==="
fi
