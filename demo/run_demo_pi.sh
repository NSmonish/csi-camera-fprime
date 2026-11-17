#!/usr/bin/env bash
# run_demo_pi.sh: the Pi demo for the LIONESS F Prime camera stack.
# Run this ON the Raspberry Pi, from the copied workspace:
#   bash ~/csi-camera-fprime/demo/run_demo_pi.sh
#
# This script was written on a laptop and has NOT been run on a Pi yet.
# Every step prints PASS, FAIL or SKIP, so a failure on the Pi is visible
# instead of silent. Do not present a SKIP as a pass.
#
# What it does, in order:
#   Part A, the same F Prime demo as the laptop, but on the Pi:
#     1. Checks the Pi has what the demo needs (socat, python3, g++,
#        the F Prime venv, the deployment binary or the sources to
#        build it).
#     2. Builds the deployment on the Pi if the binary is missing
#        (native build, slow, about 10 minutes), or uses the binary
#        you copied over. Set DEMO_BIN to point at a cross-compiled
#        binary from the Mac instead.
#     3. Runs the same virtual serial pair plus fprime-gds plus live
#        commands as the laptop demo, and leaves the products in
#        demo/products_pi/.
#   Part B, the real camera, only if a camera is attached:
#     4. Checks the ArduCam is detected (v4l2-ctl, libcamera).
#     5. Captures real sample frames with rpicam-still.
#     6. Runs tools/verify_real_frame on each frame and reports the
#        stride and format the real driver actually uses.
#
# Copy to the Pi beforehand (from the laptop):
#   rsync -av ~/workspace/csi-camera-fprime/ pi@<pi-host>:~/csi-camera-fprime/
# That copies the sources, the demo scripts, the frame checker and the
# hardware test plan. It does NOT copy the laptop build directories in a
# usable form: the laptop binaries are x86_64 and will not run on the
# Pi, so the Pi builds its own. The fprime-venv is also laptop-only;
# on the Pi, create it with the F Prime bootstrap (see Part A step 1).
#
# Style rule for this project: plain punctuation only, no em dashes.

set -u
DEMO_DIR="$(cd "$(dirname "$0")" && pwd)"
WS="$(dirname "$DEMO_DIR")"
SW="$WS/fsw/lioness-sw"
DICT="$SW/build-fprime-automatic-native/LionessSw/FlatSat/Top/FlatSatTopologyDictionary.json"
VENV="$SW/fprime-venv/bin"
BIN="${DEMO_BIN:-$SW/build-fprime-automatic-native/bin/Linux/LionessSw_FlatSat}"
RUN_DIR="$DEMO_DIR/.run_pi"
WORK_DIR="$RUN_DIR/work"
PRODUCTS_DIR="$DEMO_DIR/products_pi"
FRAMES_DIR="$RUN_DIR/frames"
PORT=50050
PTY_A=/tmp/demo_ttyA
PTY_B=/tmp/demo_ttyB

PASS=0
FAIL=0
SKIP=0
step() { echo ""; echo "=== $1 ==="; }
ok()   { echo "PASS: $1"; PASS=$((PASS+1)); }
bad()  { echo "FAIL: $1"; FAIL=$((FAIL+1)); }
skip() { echo "SKIP: $1"; SKIP=$((SKIP+1)); }

cleanup() {
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
    rm -f "$PTY_A" "$PTY_B"
}
trap cleanup EXIT

mkdir -p "$RUN_DIR" "$WORK_DIR" "$PRODUCTS_DIR" "$FRAMES_DIR"
rm -rf "$WORK_DIR/camera_storage"

step "Part A step 1: checks on the Pi"
ARCH="$(uname -m)"
[ "$ARCH" = "aarch64" ] && ok "running on aarch64 ($ARCH)" || bad "unexpected architecture $ARCH, this script expects a Raspberry Pi"
command -v socat >/dev/null 2>&1 && ok "socat is installed" || { bad "socat is missing, install it with: sudo apt install socat"; }
command -v python3 >/dev/null 2>&1 && ok "python3 is present" || bad "python3 is missing"
command -v g++ >/dev/null 2>&1 && ok "g++ is present" || bad "g++ is missing, install it with: sudo apt install g++"
if [ -x "$VENV/fprime-gds" ]; then
    ok "fprime-gds exists on the Pi"
else
    bad "fprime-gds is missing on the Pi. The laptop venv does not work here. On the Pi, create it with: python3 -m venv fprime-venv && fprime-venv/bin/pip install fprime-tools (run from $SW)"
fi
if [ -x "$BIN" ]; then
    ok "deployment binary exists: $BIN"
    file "$BIN" | grep -q "ARM aarch64" && ok "binary is aarch64" || bad "binary is not aarch64, it will not run on the Pi"
fi

step "Part A step 2: build the deployment on the Pi if needed"
if [ ! -x "$BIN" ]; then
    if [ -x "$VENV/fprime-util" ]; then
        echo "No binary found. Building natively on the Pi, this takes about 10 minutes."
        if (cd "$SW" && . fprime-venv/bin/activate && fprime-util build > "$RUN_DIR/build.log" 2>&1); then
            ok "native build finished, see $RUN_DIR/build.log"
        else
            bad "native build failed, see $RUN_DIR/build.log"
        fi
    else
        bad "no binary and no fprime-util on the Pi. Copy the workspace and build, or cross-compile on the Mac and set DEMO_BIN to the built binary."
    fi
fi

# Only run the live demo if the binary and fprime-gds are both present.
if [ -x "$BIN" ] && [ -x "$VENV/fprime-gds" ]; then
    step "Part A step 3: start the deployment on a virtual serial pair"
    socat PTY,link=$PTY_A,raw,echo=0 PTY,link=$PTY_B,raw,echo=0 > "$RUN_DIR/socat.log" 2>&1 &
    echo $! > "$RUN_DIR/socat.pid"
    sleep 1
    if [ -e "$PTY_A" ] && [ -e "$PTY_B" ]; then ok "virtual serial pair up: $PTY_A <-> $PTY_B"; else bad "socat did not create the pty pair, see $RUN_DIR/socat.log"; fi
    (cd "$WORK_DIR" && "$BIN" -u "$PTY_A" -r /tmp/demo_ttyNOPE -b 115200 > "$RUN_DIR/deploy.log" 2>&1 & echo $! > "$RUN_DIR/deploy.pid")
    sleep 4
    grep -q "PortOpened" "$RUN_DIR/deploy.log" 2>/dev/null && ok "deployment started, primary UART open on $PTY_A" || bad "deployment did not open its UART, see $RUN_DIR/deploy.log"
    grep "OpCodeRegistered" "$RUN_DIR/deploy.log" | grep -E "0x10006000|0x10007000|0x10008000|0x10009000" | sed 's/.*Opcode/  Opcode/' || true

    step "Part A step 4: start fprime-gds on the other end of the pair"
    (cd "$SW" && "$VENV/fprime-gds" -n --dictionary "$DICT" \
        --communication-selection uart --uart-device "$PTY_B" --uart-baud 115200 \
        --uart-skip-port-check --tts-port $PORT --tts-addr 127.0.0.1 --no-zmq \
        -g none -l "$RUN_DIR/gds_logs" > "$RUN_DIR/gds.log" 2>&1 & echo $! > "$RUN_DIR/gds.pid")
    for i in $(seq 1 15); do
        if python3 -c "import socket; socket.create_connection(('127.0.0.1',$PORT),timeout=2).close()" 2>/dev/null; then break; fi
        sleep 1
    done
    if python3 -c "import socket; socket.create_connection(('127.0.0.1',$PORT),timeout=2).close()" 2>/dev/null; then
        ok "ground station TCP port $PORT is open"
    else
        bad "ground station did not open port $PORT, see $RUN_DIR/gds.log"
    fi
    GDS_LOG_DIR=""
    for i in $(seq 1 10); do
        GDS_LOG_DIR="$(ls -d "$RUN_DIR/gds_logs"/*/ 2>/dev/null | tail -1)"
        if [ -n "$GDS_LOG_DIR" ] && grep -a -q "Registered client FSW" "$GDS_LOG_DIR/ThreadedTCP.log" 2>/dev/null; then break; fi
        sleep 1
    done
    if [ -n "$GDS_LOG_DIR" ] && grep -a -q "Registered client FSW" "$GDS_LOG_DIR/ThreadedTCP.log" 2>/dev/null; then
        ok "comm adapter registered with the ground station as FSW"
    else
        bad "comm adapter did not register as FSW, see $GDS_LOG_DIR"
    fi

    step "Part A step 5: drive three live commands through GDS"
    if (cd "$WORK_DIR" && "$VENV/python" "$DEMO_DIR/gds_drive.py" $PORT > "$RUN_DIR/gds_drive.log" 2>&1); then
        ok "live GDS commands all passed"
    else
        bad "live GDS commands had failures, see $RUN_DIR/gds_drive.log"
    fi
    cat "$RUN_DIR/gds_drive.log"

    step "Part A step 6: collect the products"
    if [ -d "$WORK_DIR/camera_storage" ]; then
        cp "$WORK_DIR"/camera_storage/payload_* "$PRODUCTS_DIR"/ 2>/dev/null
        ls -la "$PRODUCTS_DIR"/
        ok "products copied to demo/products_pi/"
    else
        bad "no camera_storage directory, no products were stored"
    fi
else
    skip "Part A live demo skipped: binary or fprime-gds missing on the Pi"
fi

step "Part B step 1: is the real camera detected"
if [ -e /dev/video0 ]; then
    ok "/dev/video0 exists"
    v4l2-ctl --list-devices 2>/dev/null | head -5 || bad "v4l2-ctl failed, install it with: sudo apt install v4l-utils"
    libcamera-hello --list-cameras 2>/dev/null | head -5 || echo "libcamera-hello not available or no cameras listed"
else
    skip "no /dev/video0, no camera attached. Part B needs the ArduCam connected to the Pi."
fi

if [ -e /dev/video0 ]; then
    step "Part B step 2: capture real sample frames"
    if command -v rpicam-still >/dev/null 2>&1; then
        (cd "$FRAMES_DIR" && rpicam-still --width 4608 --height 2592 --raw -o frame_4608.jpg -t 2000 > "$RUN_DIR/rpicam.log" 2>&1) && ok "4608x2592 frame captured" || bad "rpicam-still failed, see $RUN_DIR/rpicam.log"
        (cd "$FRAMES_DIR" && rpicam-still --width 1280 --height 720 --raw -o frame_720.jpg -t 2000 >> "$RUN_DIR/rpicam.log" 2>&1) && ok "1280x720 frame captured" || bad "1280x720 capture failed, see $RUN_DIR/rpicam.log"
        ls -la "$FRAMES_DIR"/
    else
        skip "rpicam-still is not installed, install it with: sudo apt install rpicam-apps"
    fi

    step "Part B step 3: check the real frames against the stack assumptions"
    CHECKER="$WS/tools/verify_real_frame"
    if [ ! -x "$CHECKER" ]; then
        g++ -std=c++14 -O2 -Wall -Wextra -I"$SW/LionessSw/Components/ImageProcessing" "$WS/tools/verify_real_frame.cpp" -o "$CHECKER" > "$RUN_DIR/checker_build.log" 2>&1 && ok "frame checker built on the Pi" || bad "frame checker did not build, see $RUN_DIR/checker_build.log"
    fi
    if [ -x "$CHECKER" ]; then
        # rpicam-still --raw writes a DNG. The checker takes a plain raw
        # file, so extract the Bayer bytes from the DNG first if needed.
        # If no plain .raw file is present, this step reports SKIP with the
        # exact extraction step to do, instead of failing silently.
        for spec in "frame_4608 4608 2592" "frame_720 1280 720"; do
            set -- $spec
            if [ -f "$FRAMES_DIR/$1.raw" ]; then
                "$CHECKER" "$FRAMES_DIR/$1.raw" "$2" "$3" "$FRAMES_DIR/$1" > "$RUN_DIR/check_$1.log" 2>&1 && ok "$1.raw passes the frame checks" || bad "$1.raw fails the frame checks, see $RUN_DIR/check_$1.log"
            else
                skip "$1.raw not present. rpicam-still wrote a DNG, extract the raw Bayer bytes from $FRAMES_DIR/$1.dng first, then run: $CHECKER $FRAMES_DIR/$1.raw $2 $3 $FRAMES_DIR/$1"
            fi
        done
    fi
fi

echo ""
echo "=== Pi demo result: $PASS passed, $FAIL failed, $SKIP skipped ==="
echo "Logs: $RUN_DIR"
echo "Products: $PRODUCTS_DIR"
[ "$FAIL" -eq 0 ]
