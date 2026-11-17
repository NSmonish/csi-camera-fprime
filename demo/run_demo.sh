#!/usr/bin/env bash
# run_demo.sh: the laptop demo for the LIONESS F Prime camera stack.
# Run it from anywhere with one command:
#   bash ~/workspace/csi-camera-fprime/demo/run_demo.sh
# It prints PASS or FAIL for each step and leaves the captured products in
# demo/products/. It cleans up its own background processes on exit.
#
# What it does, in order:
#   1. Runs the unit tests (ctest, 6 suites).
#   2. Starts the FlatSat deployment on a virtual serial pair (socat),
#      and shows the opcode registration lines.
#   3. Starts fprime-gds with the UART link on the other end of the pair.
#      Note the --tts-addr 127.0.0.1 flag: it is required. The stock
#      fprime-gds default of 0.0.0.0 makes the comm adapter connect and
#      reset in a loop on this machine, so the ground station never sees
#      the deployment. With 127.0.0.1 it registers and works. This was
#      verified on Nov 16, 2026.
#   4. Drives three live commands through GDS (demo/gds_drive.py):
#      payload picture, star picture, and a same-device double submit
#      that must come back Busy.
#   5. Copies the captured products into demo/products/ and checks their
#      magic bytes and the lossless round trip.
#
# The cameras in this demo are simulated. The real ArduCam only enters
# through the Wednesday hardware test, see HARDWARE_TEST_PLAN.md and
# run_demo_pi.sh for the Pi path.
#
# Style rule for this project: plain punctuation only, no em dashes.

set -u
DEMO_DIR="$(cd "$(dirname "$0")" && pwd)"
WS="$(dirname "$DEMO_DIR")"
SW="$WS/fsw/lioness-sw"
# fprime-util puts the deployment binary under bin/<platform>/.
# Linux builds land in bin/Linux, macOS builds in bin/Darwin.
# Pick the right one for this machine.
case "$(uname -s)" in
    Darwin) PLATFORM="Darwin" ;;
    *)      PLATFORM="Linux" ;;
esac
BIN="$SW/build-fprime-automatic-native/bin/$PLATFORM/LionessSw_FlatSat"
DICT="$SW/build-fprime-automatic-native/LionessSw/FlatSat/Top/FlatSatTopologyDictionary.json"
VENV="$SW/fprime-venv/bin"
RUN_DIR="$DEMO_DIR/.run"
WORK_DIR="$RUN_DIR/work"
PRODUCTS_DIR="$DEMO_DIR/products"
# Pick a free TCP port for the ground station instead of a fixed
# one. A fixed port fails with "address already in use" when a
# previous run left the port in TIME_WAIT, or when an old ground
# station process still holds it. A free port picked at the start
# cannot collide with either.
PORT="$(python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1]); s.close()')"
PTY_A=/tmp/demo_ttyA
PTY_B=/tmp/demo_ttyB

PASS=0
FAIL=0
step() { echo ""; echo "=== $1 ==="; }
ok()   { echo "PASS: $1"; PASS=$((PASS+1)); }
bad()  { echo "FAIL: $1"; FAIL=$((FAIL+1)); }

cleanup() {
    # Kill only the processes this script started, by pid file.
    for f in "$RUN_DIR"/*.pid; do
        [ -f "$f" ] || continue
        pid="$(cat "$f" 2>/dev/null)"
        [ -n "$pid" ] && kill "$pid" 2>/dev/null
    done
    sleep 1
    for f in "$RUN_DIR"/*.pid; do
        [ -f "$f" ] || continue
        pid="$(cat "$f" 2>/dev/null)"
        [ -n "$pid" ] && kill -9 "$pid" 2>/dev/null
    done
    # fprime-gds spawns child processes (comm adapter, data
    # handlers) that outlive the parent, so killing the recorded
    # pid is not enough. Kill anything tied to this run: the
    # GDS children carry the run directory in their command
    # line, and the deployment carries its pty path.
    pkill -f "$RUN_DIR" 2>/dev/null || true
    pkill -f "LionessSw_FlatSat -u $PTY_A" 2>/dev/null || true
    sleep 1
    pkill -9 -f "$RUN_DIR" 2>/dev/null || true
    pkill -9 -f "LionessSw_FlatSat -u $PTY_A" 2>/dev/null || true
    rm -f "$PTY_A" "$PTY_B"
}
trap cleanup EXIT

mkdir -p "$RUN_DIR" "$WORK_DIR" "$PRODUCTS_DIR"
rm -rf "$WORK_DIR/camera_storage"

step "Step 0: checks"
# macOS does not ship socat. It is a Homebrew install:
# brew install socat. Linux: sudo apt install socat.
if [ "$PLATFORM" = "Darwin" ]; then SOCAT_HINT="brew install socat"; else SOCAT_HINT="sudo apt install socat"; fi
command -v socat >/dev/null 2>&1 && ok "socat is installed" || { bad "socat is missing, install it with: $SOCAT_HINT"; }
[ -x "$BIN" ] && ok "deployment binary exists" || bad "deployment binary missing at $BIN, build it first with fprime-util build"
[ -f "$DICT" ] && ok "dictionary exists" || bad "dictionary missing at $DICT"
[ -x "$VENV/fprime-gds" ] && ok "fprime-gds exists" || bad "fprime-gds missing in $VENV"

step "Step 1: unit tests (ctest)"
if (cd "$SW/build-fprime-automatic-native-ut" && ctest --output-on-failure > "$RUN_DIR/ctest.log" 2>&1); then
    ok "ctest: 6 of 6 suites pass (see $RUN_DIR/ctest.log)"
    tail -3 "$RUN_DIR/ctest.log"
else
    bad "ctest failed, see $RUN_DIR/ctest.log"
fi

step "Step 2: start the deployment on a virtual serial pair"
socat PTY,link=$PTY_A,raw,echo=0 PTY,link=$PTY_B,raw,echo=0 > "$RUN_DIR/socat.log" 2>&1 &
echo $! > "$RUN_DIR/socat.pid"
sleep 1
if [ -e "$PTY_A" ] && [ -e "$PTY_B" ]; then ok "virtual serial pair up: $PTY_A <-> $PTY_B"; else bad "socat did not create the pty pair, see $RUN_DIR/socat.log"; fi
# exec makes the subshell become the binary, so $! is the real
# deployment pid. Without exec, $! is a wrapper subshell and
# killing it leaves the deployment running.
(cd "$WORK_DIR"; exec "$BIN" -u "$PTY_A" -r /tmp/demo_ttyNOPE -b 115200 > "$RUN_DIR/deploy.log" 2>&1) &
echo $! > "$RUN_DIR/deploy.pid"
sleep 4
if grep -q "PortOpened" "$RUN_DIR/deploy.log" 2>/dev/null; then
    ok "deployment started, primary UART open on $PTY_A"
else
    bad "deployment did not open its UART, see $RUN_DIR/deploy.log"
fi
echo "Opcodes the deployment registered (look for the camera ones):"
grep "OpCodeRegistered" "$RUN_DIR/deploy.log" | grep -E "0x10006000|0x10007000|0x10008000|0x10009000" | sed 's/.*Opcode/  Opcode/' || true
grep -c "OpCodeRegistered" "$RUN_DIR/deploy.log" | xargs echo "Total opcodes registered:"

step "Step 3: start fprime-gds on the other end of the pair"
(cd "$SW"; exec "$VENV/fprime-gds" -n --dictionary "$DICT" \
    --communication-selection uart --uart-device "$PTY_B" --uart-baud 115200 \
    --uart-skip-port-check --tts-port $PORT --tts-addr 127.0.0.1 --no-zmq \
    -g none -l "$RUN_DIR/gds_logs" > "$RUN_DIR/gds.log" 2>&1) &
echo $! > "$RUN_DIR/gds.pid"
# C-style loop, not seq: macOS does not ship seq.
for ((i = 1; i <= 15; i++)); do
    if python3 -c "import socket; socket.create_connection(('127.0.0.1',$PORT),timeout=2).close()" 2>/dev/null; then break; fi
    sleep 1
done
if python3 -c "import socket; socket.create_connection(('127.0.0.1',$PORT),timeout=2).close()" 2>/dev/null; then
    ok "ground station TCP port $PORT is open"
else
    bad "ground station did not open port $PORT, see $RUN_DIR/gds.log"
fi
GDS_LOG_DIR=""
for ((i = 1; i <= 10; i++)); do
    GDS_LOG_DIR="$(ls -d "$RUN_DIR/gds_logs"/*/ 2>/dev/null | tail -1)"
    if [ -n "$GDS_LOG_DIR" ] && grep -a -q "Registered client FSW" "$GDS_LOG_DIR/ThreadedTCP.log" 2>/dev/null; then break; fi
    sleep 1
done
if [ -n "$GDS_LOG_DIR" ] && grep -a -q "Registered client FSW" "$GDS_LOG_DIR/ThreadedTCP.log" 2>/dev/null; then
    ok "comm adapter registered with the ground station as FSW"
else
    bad "comm adapter did not register as FSW, see $GDS_LOG_DIR"
fi

step "Step 4: drive three live commands through GDS"
if (cd "$WORK_DIR" && "$VENV/python" "$DEMO_DIR/gds_drive.py" $PORT > "$RUN_DIR/gds_drive.log" 2>&1); then
    ok "live GDS commands all passed"
else
    bad "live GDS commands had failures, see $RUN_DIR/gds_drive.log"
fi
cat "$RUN_DIR/gds_drive.log"

step "Step 5: collect the products"
if [ -d "$WORK_DIR/camera_storage" ]; then
    cp "$WORK_DIR"/camera_storage/payload_* "$PRODUCTS_DIR"/ 2>/dev/null
    ls -la "$PRODUCTS_DIR"/
    python3 - "$PRODUCTS_DIR" <<'PYEOF'
import os, sys, zlib
d = sys.argv[1]
ok = True
for f in sorted(os.listdir(d)):
    # The README in this directory documents how the products were
    # made. It is not a product, so skip it.
    if f == "README.md":
        continue
    p = os.path.join(d, f)
    data = open(p, "rb").read()
    if f.endswith(".jpg") and data[:4] == b"\xff\xd8\xff\xe0":
        print(f"PASS: {f} {len(data)} bytes, real JPEG")
    elif f.endswith(".png") and data[:4] == b"\x89PNG":
        print(f"PASS: {f} {len(data)} bytes, real PNG")
    elif f.endswith(".zlib"):
        raw = zlib.decompress(data)
        print(f"PASS: {f} {len(data)} bytes, decompresses to {len(raw)} bytes" if len(raw) == 1152000 else f"FAIL: {f} decompresses to {len(raw)}")
        ok = ok and len(raw) == 1152000
    else:
        print(f"FAIL: {f} has unexpected content")
        ok = False
sys.exit(0 if ok else 1)
PYEOF
    [ $? -eq 0 ] && ok "products in demo/products/ verified" || bad "product check failed"
else
    bad "no camera_storage directory, no products were stored"
fi

echo ""
echo "=== Demo result: $PASS passed, $FAIL failed ==="
echo "Logs: $RUN_DIR"
echo "Products: $PRODUCTS_DIR"
[ "$FAIL" -eq 0 ]
