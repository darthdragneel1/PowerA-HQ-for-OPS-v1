#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$DIR/bin/powera-driver"

echo "=== Restarting PowerA macOS Driver Daemon ==="

# 1. Terminate existing daemon instances
echo "Stopping any existing powera-driver instances..."
killall powera-driver 2>/dev/null || true
pkill -f "bin/powera-driver" 2>/dev/null || true
sleep 0.6

# 2. Check executable
if [ ! -x "$BIN" ]; then
    echo "Driver binary not found at $BIN, running build script first..."
    "$DIR/scripts/build_bundle.sh"
fi

# 3. Start daemon in background
echo "Starting PowerA Driver Subsystem in background mode..."
nohup "$BIN" --daemon > "$DIR/driver/powera_driver.log" 2>&1 &
DRIVER_PID=$!

sleep 0.8

# 4. Verify IPC endpoint with retry loop
SUCCESS=0
for i in {1..10}; do
    if curl -s http://127.0.0.1:8765/api/status > /dev/null 2>&1; then
        echo "✓ PowerA Driver daemon running successfully (PID: $DRIVER_PID, Port: 8765)."
        curl -s http://127.0.0.1:8765/api/status
        echo ""
        SUCCESS=1
        break
    fi
    sleep 0.3
done

if [ $SUCCESS -eq 0 ]; then
    echo "Warning: Driver daemon did not respond on http://127.0.0.1:8765. Checking log:"
    cat "$DIR/driver/powera_driver.log" 2>/dev/null || true
fi
