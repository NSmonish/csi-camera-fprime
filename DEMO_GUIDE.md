# LIONESS camera demo guide: Wednesday Nov 18, 2026

This guide teaches you the demo in plain terms first, then gives you
the exact run-through for two machines: your laptop, and the Raspberry
Pi. The laptop demo is fully verified. The Pi demo script is written
and its laptop parts were smoke tested, but it has not been run on a
Pi yet, so the guide says so where it matters.

## What each piece is

Think of the demo as a phone call between ground and the satellite.
There are four actors, and the picture travels through all of them.

- **PayloadManager.** The manager for the payload camera. You tell it
  "take a payload picture" and it asks the Camera for a frame, waits
  for the processed products, and stores them. It never blocks while
  waiting. It takes the command, returns at once, and the products
  arrive later.
- **StarTrackerManager.** The manager for the star tracker camera. You
  tell it "take a star picture" and it asks the Camera for a raw frame
  and hands that frame to StarTracker. It stores no preview and no
  lossless product, because the star tracker needs the raw frame.
- **Camera.** The shared camera component. It owns one job slot and one
  worker thread per camera, payload and star, so both cameras can
  capture at the same time. A second request for the same camera while
  it is busy is refused at once with Busy, and nothing is stored for
  it. The Camera talks to the cameras through the V4L2 layer. On the
  laptop demo the cameras are simulated devices that model the
  ArduCam IMX708. On the Pi the same component will talk to the real
  V4L2 devices.
- **ImageProcessing.** The component that turns a raw frame into
  products. A JPEG preview (320x240, quality 30, made with the stb
  image library), a PNG preview (same 320x240 frame), and a lossless
  product (the whole raw frame compressed with zlib, byte for byte
  recoverable). The PNG is a viewable preview, not the lossless
  product. The zlib file is the true lossless archive.

## How a picture flows

Payload picture, start to finish:

1. You send TakePayloadPicture through the ground station. The
   command travels over the UART link as a CCSDS packet, into the
   command dispatcher, to PayloadManager.
2. PayloadManager submits the request to the Camera through a sync
   port that returns at once. The command is accepted and the
   manager waits.
3. The Camera worker captures a frame from the payload device
   through V4L2, then hands the raw frame to ImageProcessing.
4. ImageProcessing makes the JPEG preview, the PNG preview and the
   zlib lossless product, and returns the buffers to the Camera.
5. The Camera reports completion to PayloadManager. PayloadManager
   stores the three products in `camera_storage/` and reports the
   PictureStored event with the byte counts.

Star picture, start to finish:

1. You send TakeStarPicture. It travels the same way, to
   StarTrackerManager.
2. StarTrackerManager submits to the Camera for the star device.
3. The Camera captures a raw frame from the star device.
4. The raw frame goes to StarTracker, which records it, and the
   buffer is returned to the Camera. No preview and no lossless
   product are made for a star frame.

The two managers can fire at the same time because each camera has
its own job slot and worker. That is the fix from Nov 16. Before it,
one shared busy flag meant the second manager never reached V4L2.

## The commands and what to expect

All commands are 1280x720 in the demo. The opcodes are:

| Command | Opcode | Arguments |
|---|---|---|
| Camera TakePicture | 0x10006000 | cameraId, width, height, operation, timeoutMs |
| ImageProcessing SetPreviewQuality | 0x10007000 | quality |
| PayloadManager TakePayloadPicture | 0x10008000 | width, height, operation |
| StarTrackerManager TakeStarPicture | 0x10009000 | width, height |

The operation argument for a payload picture is BOTH, which means
preview plus lossless. The demo uses BOTH for every payload picture.

A fresh deployment numbers its requests from 1. The first payload
picture is request 1 and stores `payload_1_preview.jpg`,
`payload_1_preview.png` and `payload_1_lossless.zlib`. The next
requests count up from there. If you restart the deployment, the
numbers start at 1 again. If you do not restart it, they keep
counting, so check the PictureStored event for the request number
instead of assuming it is 1.

Expected products for a 1280x720 BOTH capture, measured Nov 16,
2026:

- `payload_N_preview.jpg`, about 6,800 bytes, starts with bytes
  FF D8 FF E0. The exact size changes a little from frame to frame
  because the simulated frame content changes.
- `payload_N_preview.png`, about 1,660 bytes, starts with bytes
  89 50 4E 47, decodes to 320x240, 8 bit grayscale.
- `payload_N_lossless.zlib`, 15,677 bytes, decompresses to exactly
  1,152,000 bytes, which is 1280 times 720 times 10 over 8, byte for
  byte equal to the captured frame.

## The laptop demo, 5 minutes

One command runs the whole thing:

```sh
bash ~/workspace/csi-camera-fprime/demo/run_demo.sh
```

It prints PASS or FAIL for each step and takes about 30 seconds.
What it does:

1. Runs the unit tests. You should see 6 of 6 suites pass.
2. Starts the FlatSat deployment on a virtual serial pair and
   prints the opcode registration lines. Point at the four camera
   opcodes: 0x10006000, 0x10007000, 0x10008000, 0x10009000, out of
   52 total.
3. Starts fprime-gds with the UART link on the other end of the
   pair. It waits for the comm adapter to register as FSW.
4. Sends three live commands through GDS: a payload picture, a
   star picture, and two payload pictures back to back. The second
   of the pair comes back Busy and stores nothing.
5. Copies the products into `demo/products/` and checks their
   magic bytes and the lossless round trip.

What to say at each step:

- Step 1: "First, the unit tests. Six suites, twenty one tests,
  all pass. This is the code we are about to run."
- Step 2: "This is the flight software starting. It registers 52
  commands, including the four camera commands. The deployment
  talks over a serial link, so for the demo we give it a virtual
  serial pair instead of a real radio."
- Step 3: "This is the ground station. It connects over the
  virtual serial link and registers as the flight software side.
  The deployment is now talking to ground."
- Step 4: "Now three live commands. First a payload picture.
  Watch for the PictureStored event with the byte counts. Then a
  star picture, watch for StarFrameDelivered. Then two payload
  pictures at once. The second one is refused as Busy, because
  one camera does one job at a time, and it stores nothing."
- Step 5: "Here are the products on disk. Open the preview PNG
  to show the picture. The JPEG is the small preview for the
  20 kilobit per second downlink. The zlib file decompresses to
  exactly the captured frame, byte for byte."

The exact commands the script runs, if anyone asks what is
underneath:

```sh
# Virtual serial pair. The deployment takes one end, GDS the other.
socat PTY,link=/tmp/demo_ttyA,raw,echo=0 PTY,link=/tmp/demo_ttyB,raw,echo=0

# Deployment. The -r device does not exist on purpose: the RoarFM
# link is optional and the deployment keeps running without it.
fsw/lioness-sw/build-fprime-automatic-native/bin/Linux/LionessSw_FlatSat \
  -u /tmp/demo_ttyA -r /tmp/demo_ttyNOPE -b 115200

# Ground station. The --tts-addr 127.0.0.1 flag is required, see below.
# The port number is picked by run_demo.sh, which chooses a free port
# each run. A fixed port fails with "address already in use" when a
# previous run left the port in TIME_WAIT. 50050 below is an example
# only. If you start fprime-gds by hand, pick any free port and pass
# the same number to gds_drive.py.
fsw/lioness-sw/fprime-venv/bin/fprime-gds -n \
  --dictionary fsw/lioness-sw/build-fprime-automatic-native/LionessSw/FlatSat/Top/FlatSatTopologyDictionary.json \
  --communication-selection uart --uart-device /tmp/demo_ttyB --uart-baud 115200 \
  --uart-skip-port-check --tts-port 50050 --tts-addr 127.0.0.1 --no-zmq -g none
```

The ground station commands are sent by `demo/gds_drive.py`,
which uses the GDS Python API. The three commands it sends are:

- `FlatSat.payloadManager.TakePayloadPicture`, 1280, 720, BOTH
- `FlatSat.starTrackerManager.TakeStarPicture`, 1280, 720
- `FlatSat.payloadManager.TakePayloadPicture`, 1280, 720, BOTH,
  twice back to back

### The --tts-addr trap

This is the one thing that will waste your time if you miss it.
The stock `fprime-gds` command passes `--tts-addr 0.0.0.0` to its
comm adapter by default. On this machine, the comm adapter then
connects to the ground station and is reset in a loop, hundreds
of times a second, and never registers. The ground station never
sees the deployment. The fix is one flag: `--tts-addr 127.0.0.1`.
With it, the comm adapter registers as FSW and the link works.
This was verified on Nov 16, 2026, twice: with 0.0.0.0 the
adapter never registered, with 127.0.0.1 it registered at once
and the full command flow passed. The script already uses the
working flag. If you run `fprime-gds` by hand, add it.

### Laptop fallback order

If something fails, work down this list. Do not skip a step.

1. **Live GDS commands work.** This is the full demo. Use it.
2. **Live commands fail, but ctest passes.** Run the unit tests
   and show the pre-generated products in `demo/products/`. Say
   plainly that the products were captured live earlier by the
   same script, and the ground link is down today. The command is
   `ctest` in
   `fsw/lioness-sw/build-fprime-automatic-native-ut`, then open
   `demo/products/payload_1_preview.png`.
3. **ctest fails.** Show the deployment startup log. Start the
   deployment by hand on the virtual serial pair and point at the
   opcode registration lines. That still proves the topology
   builds and starts.
4. **The deployment does not start.** Show the frame checker on
   a real sample frame, or the hardware test plan. That is a
   different demo, the camera hardware one, and it stands on its
   own.

## The Pi demo

The Pi demo runs the same F Prime flow on the Raspberry Pi, plus
the real camera checks. Run it on the Pi with:

```sh
bash ~/csi-camera-fprime/demo/run_demo_pi.sh
```

### What to copy to the Pi beforehand

From the laptop:

```sh
rsync -av ~/workspace/csi-camera-fprime/ pi@<pi-host>:~/csi-camera-fprime/
```

That copies the sources, the demo scripts, the frame checker
(`tools/verify_real_frame.cpp`) and the hardware test plan
(`HARDWARE_TEST_PLAN.md`). Two things do NOT carry over and must
be made on the Pi:

- **The deployment binary.** The laptop binary is x86_64 and
  will not run on the Pi. The Pi script builds it natively if it
  is missing, which takes about 10 minutes, or you can
  cross-compile on the Mac and copy the binary over. Monish set
  up an ARM cross-compile toolchain for the Raspberry Pi 5 on his
  Mac on Oct 7, 2026. To use a cross-compiled binary, set
  `DEMO_BIN` to its path before running the script:
  `DEMO_BIN=/path/to/LionessSw_FlatSat bash demo/run_demo_pi.sh`.
- **The F Prime venv.** The laptop venv is x86_64 too. On the Pi,
  create it in `fsw/lioness-sw` with:
  `python3 -m venv fprime-venv && fprime-venv/bin/pip install fprime-tools`.
  The Pi script checks for it and prints this exact fix if it is
  missing.

Also on the Pi, install the tools the demo needs:

```sh
sudo apt install socat v4l-utils rpicam-apps g++
```

### What the Pi script does

Part A is the same F Prime demo as the laptop: checks, build if
needed, virtual serial pair, deployment, fprime-gds with
`--tts-addr 127.0.0.1`, three live commands, products into
`demo/products_pi/`. Part B is the real camera, and it only runs
if a camera is attached:

1. Checks `/dev/video0` exists and lists the camera with
   `v4l2-ctl --list-devices` and `libcamera-hello --list-cameras`.
2. Captures real sample frames with `rpicam-still`, 4608x2592 and
   1280x720, with raw data.
3. Builds `tools/verify_real_frame` on the Pi if needed and runs
   it on each frame. It reports whether the frame size matches
   packed RAW10, whether the driver pads rows and by how much,
   and whether the pixel values look like a real image. A padded
   stride is the finding to bring back: the F Prime stack assumes
   stride `width * 10 / 8`, so a padded stride means the stack
   must be told about it before it runs on the real camera.

One wrinkle to say out loud: `rpicam-still --raw` writes a DNG
file, not a plain raw file. The checker takes a plain raw file,
so the raw Bayer bytes must be extracted from the DNG first.
The Pi script reports SKIP with the exact extraction step if no
plain `.raw` file is present, instead of failing silently.

### Pi fallback order

1. **Live GDS commands plus real frame checks.** The full Pi
   demo. Use it.
2. **Live commands work, no camera attached.** Show Part A only
   and say the camera was not attached, so Part B was skipped.
   The F Prime demo still stands.
3. **Live commands fail, camera works.** Run the frame checker
   on the real sample frames. That is the hardware demo, and it
   is the part the laptop cannot do at all.
4. **Neither works.** Show the laptop demo products and the
   hardware test plan, and say what is left to fix on the Pi.

## Honest limits, say these out loud

- The deployment cannot be commanded from a keyboard. Commands
  need the UART ground station link, which is what fprime-gds
  provides. There is no TCP command interface in the topology.
- The cameras in the laptop demo are simulated. The simulated
  payload camera models the ArduCam IMX708, and the simulated
  star camera is 1280x720 RAW10, which is an assumption, not a
  confirmed fact. The real camera only enters through the
  Wednesday hardware test on the Pi.
- The storage is a host file stub that writes into
  `camera_storage/`, not flight storage. Downlink is out of
  scope and was not built.
- The full LOST star identification algorithm is not integrated.
  The star path delivers a raw frame to StarTracker, and that
  handoff is what the demo shows.
- The 120 ms simulated capture latency and the 500 ms timeout
  are chosen values, not target measurements. Real timing comes
  from the Pi measurements in the hardware test plan.
- The vendored F Prime copy has a one line fix for an upstream
  bug (`ActiveComponentBase::m_stage` is never initialized).
  The real repo needs the same line before flight.

## If they ask for a demo and you have two minutes

Run the laptop script, show the PictureStored event and the
preview PNG, and say the one sentence version: "The ground
station sent a picture command over a serial link, the flight
software captured a frame, made a small JPEG preview and a
lossless copy, stored both, and a second command to the same
camera while it was busy was refused." Then open
`demo/products/payload_1_preview.png` so they can see the
picture.
