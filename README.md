# CubeSat LIONESS

Welcome to the main repository for Columbia Space Initiative LIONESS CubeSat's flight software.

## Organization

```
.
├── .github     # CI/CD
├── fsw         # FPrime core flight software
│   └── lioness-sw
│       ├── LionessSw       # Core flight software FPrime components and implementation code
│       │   └── FlatSat     # Deployment of the core flight software for the FlatSat
│       └── README.md       # Core flight software documentation
├── gsw         # Ground software
├── hw          # Peripheral board firmware
├── sim         # Simulators
├── test        # Test and other miscellaneous code
└── README.md
```

## Core Flight Software Installation
These instructions sets up the FPrime project that will run on the main flight computer located in `fsw`.

Open a terminal at the project root.
```shell
cd fsw/lioness-sw
```
The following command will create and activate a virtual environment called `fprime-venv`.
```
python3 -m venv fprime-venv
. fprime-venv/bin/activate
```
Install requirements and build the project.
```
git submodule update --init --recursive
pip3 install -r requirements.txt
fprime-util generate
fprime-util build
```

## Core Flight Software Build
To build the core flight software, see [Flight Software README](fsw/lioness-sw/README.md)

---

# LIONESS camera stack in F Prime

The whole LIONESS camera stack, built in F Prime, tested on the
host in simulation with no hardware. This repo is a copy of the
cubesat-lioness repo at main `5b2fc29`, plus the camera work
described below. It lives at
https://github.com/NSmonish/csi-camera-fprime.

## Clone and build

```sh
git clone --recurse-submodules https://github.com/NSmonish/csi-camera-fprime.git
cd csi-camera-fprime
git apply --directory=fsw/lioness-sw/lib/fprime patches/fprime-m_stage.patch
```

The patch step matters: F Prime v4.1.1 never initializes
`ActiveComponentBase::m_stage` (see "Local patch to the
vendored F Prime" below), and the submodule points at
unpatched upstream. Then follow `MAC_SETUP.md` from step 2
(the venv) on a Mac, or `fsw/lioness-sw/README.md` on Linux.

## What this is

Monish's teammate stopped responding, so the managers are built
here too, not just capture. The stack covers the camera module
from the November 2026 CDR: two cameras, one shared V4L2 layer,
an ImageProcessing component, and two managers. Downlink is out
of scope. Storage is a host stub, not flight storage.

## Architecture

```
Ground (commands)
  |
  | TakePayloadPicture            TakeStarPicture
  v                               v
PayloadManager                  StarTrackerManager
  | captureSubmit                 | captureSubmit
  v                               v
  +-------------- Camera (active) --------------------+
  |  one job slot and one worker thread per device,   |
  |  so payload and star captures run at the same     |
  |  time; submit returns in about 0.001 ms, never    |
  |  blocks                                            |
  +--------------+----------------+-------------------+
                    |                                   |
        V4L2 HAL (no F Prime deps)            V4L2 HAL
                    |                                   |
            payload device                      star device
         ArduCam IMX708 sim              1280x720 RAW10 sim
         4608x2592 RAW10                        (assumption)
                    |
        processImage (BOTH / PREVIEW / LOSSLESS)
                    |
                    v
         ImageProcessing (active)
         previews: real JPEG and PNG (stb, quality 30)
         lossless: real zlib (the true archive)
                    |
        processDone
                    |
  +-----------------+-----------------+
  |                                   |
  v                                   v
PayloadManager                 StarTrackerManager
  | writes preview.jpg,           | hands the raw frame to
  | preview.png, lossless.zlib    | StarTracker, then returns
  | to the storage stub,          | the buffer to Camera
  | returns buffers to            |
  | ImageProcessing               |
  v
StorageStub (host files, camera_storage/)
```

The two managers share one Camera component and one V4L2
layer. Each manager has its own submit port on the Camera
(`captureSubmitPayload`, `captureSubmitStar`), and each camera
has its own device instance. The Camera has one job slot and
one worker thread per device, so a payload capture and a star
capture submitted at the same time both run and both
complete. A second submit for the same device while that
device is busy still returns Busy immediately, keeps no
buffer, and produces no completion. The device ring buffers
are the only frame storage: the Camera never copies a frame,
and every buffer is returned to the device before the
completion goes out.

## Components

- `LionessSw/V4L2/`: the V4L2 HAL, adapted from PR 55, with
  zero F Prime dependencies. `V4L2Camera` implements
  `CameraHal::ICameraDevice`. The blocking flaw in PR 55 is
  fixed: `dequeueFrame(int timeoutMs)` calls `poll()` on the
  device descriptor first and only calls `VIDIOC_DQBUF` when
  the descriptor is readable. `SimCameraDevice` is a simulated
  device behind the same interface, with the same frame pattern
  as the earlier verified simulation: RAW10 packed Bayer
  gradient plus a frame counter in the first four pixels of
  row zero. Payload camera models the ArduCam 12MP Module 3
  (Sony IMX708, 4608x2592). Star camera models a 1280x720
  RAW10 frame on Parallel CSI. That star format is an
  assumption, not a measured fact. Confirm it with the team.
- `LionessSw/Components/Camera/`: active component. Async
  `TakePicture` command plus two sync submit ports. Short
  validating submit, one worker thread per device, exactly
  one completion per accepted request, Busy per device,
  timeout, buffer release before completion. For RAW jobs
  the frame goes straight to the manager. For processed
  jobs the frame goes to ImageProcessing and the completion
  follows its reply.
- `LionessSw/Components/ImageProcessing/`: active component,
  per `sdd.md` (Issue 44). Short sync `processImage` submit,
  worker thread, one `ProcessDone` per accepted request.
  Previews are 320x240 8-bit grayscale, one JPEG and one
  PNG, both encoded by the vendored stb_image_write header
  (see below), JPEG at quality 30 by default. Lossless is
  real zlib. Output buffers are preallocated once at
  construction and returned through `returnBuffer`. The
  source frame is never written to. Failed operations report
  zero valid output bytes.
- `LionessSw/Components/PayloadManager/`: active component.
  Async `TakePayloadPicture` command. Asks the Camera for a
  capture, receives the completion, writes three products
  through `StorageStub`, returns the processed buffers to
  ImageProcessing. Product files are
  `payload_<id>_preview.jpg`, `payload_<id>_preview.png`,
  and `payload_<id>_lossless.zlib`.
- `LionessSw/Components/StarTrackerManager/`: active
  component. Async `TakeStarPicture` command. Asks the Camera
  for a RAW star capture, hands the raw frame to StarTracker,
  then returns the buffer to the Camera.
- `LionessSw/Components/StarTracker/`: the existing
  component from main, now wired into the topology with a
  real `processFrame` handler: it validates the frame size
  for RAW10 packed data, records it, and reports an event
  and telemetry. The full LOST star identification algorithm
  from `test/star-tracker` is NOT integrated. The delivered
  interface is the manager handing a raw frame to
  StarTracker.
- `LionessSw/Ports/ImagePorts.fpp`: shared types and ports:
  `CameraId`, `CaptureOperation`, `CaptureStatus`,
  `CaptureSubmit`, `CaptureDone`, `ProcessImage`,
  `ProcessDone`, `ReturnBuffer`, `ReturnFrame`,
  `ProcessFrame`.

Topology base IDs follow the `0xDSSCCxxx` convention:
camera `0x10006000`, imageProcessing `0x10007000`,
payloadManager `0x10008000`, starTrackerManager
`0x10009000`, starTracker `0x1001A000`.

## Image library: stb

The JPEG and PNG previews are encoded with
`stb_image_write.h`, vendored in
`LionessSw/Components/ImageProcessing/` (v1.18, downloaded
Nov 16, 2026 from
https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h).
It is a single header, public domain or MIT, and writes
both JPG and PNG with no system library, so the preview no
longer depends on libjpeg being installed. `stb_image.h`
is vendored alongside it for test decoding only. stb has
no JPEG to memory call, so the JPEG encoder writes through
a small callback that appends to the caller buffer
(`jpgWriteFunc` in `ImageCodec.cpp`).

The PNG is an 8-bit viewable preview, not a lossless
product. A PNG of a RAW10 frame cannot be byte for byte
lossless, because the 10-bit pixels are reduced to 8 bits
before encoding. The zlib product is the true byte for
byte archive; the PNG only shows what the frame looks
like. Do not downlink the PNG as the science data.

## What is real, what is a stub, what is simulated

- Real: the F Prime components, the port wiring, the
  topology, the V4L2 HAL interface and its poll fix, the
  zlib lossless codec, and the JPEG and PNG preview codecs
  (stb, no system library needed).
- Stub, labelled in code: `StorageStub` writes host files
  instead of flight storage.
- Simulated: both cameras. No real IMX708, no real V4L2
  driver, no exposure or autofocus, no real capture or
  processing time on target. Simulated latency is 120 ms
  and the default timeout is 500 ms; both are chosen
  values, not target measurements.
- Not integrated: the LOST star identification algorithm.
  StarTracker only validates and records frames.

## How to build and run

Prerequisites: the F Prime submodule at v4.1.1 (already
initialized here, shallow), Python 3.12, cmake, g++, and
the Python environment in `fsw/lioness-sw/fprime-venv`
(already created, requirements installed). System
package: `zlib1g-dev`. No libjpeg package is needed: the
previews use the vendored stb header.

```sh
cd fsw/lioness-sw
. fprime-venv/bin/activate
fprime-util generate
fprime-util build
```

Run the deployment (it stays up without UART hardware
attached; commands arrive over the UARTs, so host driving
uses the integration test below):

```sh
./build-fprime-automatic-native/bin/Linux/LionessSw_FlatSat
```

Unit tests plus the end-to-end host run:

```sh
fprime-util generate --ut
fprime-util build --ut
ctest --test-dir build-fprime-automatic-native-ut --output-on-failure
```

The end-to-end test
(`LionessSw/FlatSat/Top/test/ut/CameraStackTestMain.cpp`)
wires the real components exactly as `topology.fpp` does,
sends `TakePayloadPicture` and `TakeStarPicture` through
the real command ports at the same time, and checks that
all three payload products (preview.jpg, preview.png,
lossless.zlib) land in the storage stub while the raw star
frame reaches StarTracker, that the lossless product
decompresses byte for byte, that the JPEG and PNG decode
to the same 320x240 frame, that a second submit for the
same device while busy stores nothing, that a timed-out
star capture delivers no frame, and that buffers are
returned and reused.

## Hardware test, Wednesday Nov 18, 2026

See `HARDWARE_TEST_PLAN.md` in this workspace for the full
plan, with the commands to run on the Pi, the sample frames
to collect, and the results table to fill in on the day.
The frame checker is `tools/verify_real_frame.cpp`, built
as `tools/verify_real_frame`. It checks a real RAW10 frame
file against the stack's assumptions (size, stride, pixel
values, black rows) and writes viewable PNG previews. It
was tested on simulated frames: a 1280x720 frame and a
full resolution 4608x2592 frame both pass, a truncated file
fails cleanly with exit 1, and a frame with 64 bytes of row
padding is detected and reported with the stride. Do the
hardware test earlier in the day: the Razor team pitches
live at Fast Pitch at 5:00 PM the same day.

## Mapping to the real hardware port

- Replace the `SimCameraDevice` instances in
  `FlatSatTopology.cpp` with `V4L2Camera` instances and
  device paths (`/dev/video0` for the payload camera,
  the star camera path to be confirmed).
- Check the real IMX708 driver format and stride against
  a real sample frame: the real driver may pad rows, so
  real stride may differ from width times 10 over 8.
  `tools/verify_real_frame` does this check, and the
  hardware test plan records the stride to use.
- Confirm the star camera sensor, format, resolution and
  Parallel CSI device path with the team.
- Replace `StorageStub` with the real flight storage
  path, and the UART ground path with the real downlink.
- Measure capture and processing time and peak memory at
  full resolution on the IMX8X target, and re-check the
  500 ms timeout against those numbers.
- Apply the one line F Prime fix in "Local patch to the
  vendored F Prime" below to the real repo too, or the
  deployment keeps relying on zeroed heap pages.

## Local patch to the vendored F Prime

F Prime v4.1.1 never initializes
`ActiveComponentBase::m_stage`. The first task loop call
reads it, so a component that is not on fresh zeroed memory
can hit the default case assert in `s_taskStateMachine`.
The vendored copy in this workspace has a one line fix in
`fsw/lioness-sw/lib/fprime/Fw/Comp/ActiveComponentBase.hpp`:

```
Lifecycle m_stage = Lifecycle::CREATED;
```

The host end-to-end test stack allocates its components and
guards this patch: it aborted under ThreadSanitizer before
the fix and passes after it. The deployment heap allocates
its components, so it worked by luck before the fix. This
is a local patch to the vendored copy, not an upstream fix.
The same patch is saved as `patches/fprime-m_stage.patch`;
after cloning, apply it with `git apply
--directory=fsw/lioness-sw/lib/fprime
patches/fprime-m_stage.patch` (see "Clone and build" above).

## ThreadSanitizer

A separate build cache `build-fprime-tsan` runs all 6 test
executables under ThreadSanitizer with coverage off and
ASLR disabled (`setarch $(uname -m) -R`). See
`TSAN_SUPPRESSIONS.txt` and RESULTS.md: two known races
live outside the camera components (the F Prime GTestBase
event history is not thread safe for worker thread logs,
and F Prime v4.1.1 `Os/Posix/Task.cpp` copies the task name
after `pthread_create`). No component code is suppressed.

## Results

See `RESULTS.md` in this workspace. In short: the
FlatSat deployment builds, the unit tests and the
end-to-end host run pass (numbers in RESULTS.md), and
ASan/UBSan are clean on the host tests. This is still
not flight code: no hardware has been involved, and the
limits listed above still apply.
