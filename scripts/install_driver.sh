#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
echo "=== Installing PowerA OPS v1 macOS Driver & App ==="

# 1. Build latest application bundle and driver binary
echo "[1/4] Building latest application bundle and driver binaries..."
"$DIR/scripts/build_bundle.sh"

# 2. Install GameController DB mapping
echo "[2/4] Installing universal macOS GameController DB..."
mkdir -p "$HOME/.config"
if [ ! -f "$HOME/.config/gamecontrollerdb.txt" ]; then
    touch "$HOME/.config/gamecontrollerdb.txt"
fi

cat << 'EOF' >> "$HOME/.config/gamecontrollerdb.txt"

# PowerA OPS v1 Controller - macOS Driver Auto-Config
03000000d62000002540000000010000,PowerA OPS v1 Wired,platform:Mac OS X,a:b0,b:b1,x:b2,y:b3,back:b4,start:b6,guide:b5,leftshoulder:b9,rightshoulder:b10,leftstick:b7,rightstick:b8,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,paddle1:b11,paddle2:b12,paddle3:b13,paddle4:b14,
03000000d62000002740000000010000,PowerA OPS v1 2.4G Wireless Adapter,platform:Mac OS X,a:b0,b:b1,x:b2,y:b3,back:b4,start:b6,guide:b5,leftshoulder:b9,rightshoulder:b10,leftstick:b7,rightstick:b8,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,paddle1:b11,paddle2:b12,paddle3:b13,paddle4:b14,
03000000d62000000c40000000010000,PowerA OPS v1 2.4G Dongle,platform:Mac OS X,a:b0,b:b1,x:b2,y:b3,back:b4,start:b6,guide:b5,leftshoulder:b9,rightshoulder:b10,leftstick:b7,rightstick:b8,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,paddle1:b11,paddle2:b12,paddle3:b13,paddle4:b14,
05000000d62000002540000000010000,PowerA OPS v1 Bluetooth,platform:Mac OS X,a:b0,b:b1,x:b2,y:b3,back:b4,start:b6,guide:b5,leftshoulder:b9,rightshoulder:b10,leftstick:b7,rightstick:b8,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,paddle1:b11,paddle2:b12,paddle3:b13,paddle4:b14,
05000000d62000002740000000010000,PowerA OPS v1 Bluetooth,platform:Mac OS X,a:b0,b:b1,x:b2,y:b3,back:b4,start:b6,guide:b5,leftshoulder:b9,rightshoulder:b10,leftstick:b7,rightstick:b8,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,paddle1:b11,paddle2:b12,paddle3:b13,paddle4:b14,
EOF

# 3. Copy application to ~/Applications
echo "[3/4] Syncing 'PowerA PC HQ.app' to $HOME/Applications..."
mkdir -p "$HOME/Applications"
rm -rf "$HOME/Applications/PowerA PC HQ.app"
cp -R "$DIR/build/PowerA PC HQ.app" "$HOME/Applications/PowerA PC HQ.app"

# 4. Configure launchd agent for automatic background startup
echo "[4/4] Registering launchd background driver service..."
mkdir -p "$HOME/Library/LaunchAgents"

DRIVER_BIN_PATH="$HOME/Applications/PowerA PC HQ.app/Contents/MacOS/powera-driver"
if [ ! -x "$DRIVER_BIN_PATH" ]; then
    DRIVER_BIN_PATH="$DIR/bin/powera-driver"
fi

cat << EOF > "$HOME/Library/LaunchAgents/com.powera.driver.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.powera.driver</string>
    <key>ProgramArguments</key>
    <array>
        <string>${DRIVER_BIN_PATH}</string>
        <string>--daemon</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>StandardOutPath</key>
    <string>/tmp/powera_driver.log</string>
    <key>StandardErrorPath</key>
    <string>/tmp/powera_driver.err</string>
</dict>
</plist>
EOF

launchctl unload "$HOME/Library/LaunchAgents/com.powera.driver.plist" 2>/dev/null || true
launchctl load "$HOME/Library/LaunchAgents/com.powera.driver.plist" 2>/dev/null || true

echo "=========================================================="
echo " PowerA PC HQ and Driver successfully installed & updated!"
echo " App location: $HOME/Applications/PowerA PC HQ.app"
echo "=========================================================="
