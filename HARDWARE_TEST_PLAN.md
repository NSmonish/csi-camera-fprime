# LIONESS camera hardware test plan: Wednesday Nov 18, 2026

Goal: take the camera stack that passes in simulation and prove it
against the real ArduCam 12MP Camera Module 3 on a real Raspberry
Pi. Simulation proves the logic, not the physics. Wednesday proves
the physics: the real driver format and stride, real exposure and
autofocus, real capture and processing time, and the real V4L2 path.

Note on the day: the Razor team pitches live at Fast Pitch at 5:00
PM in Davis Auditorium the same day. Do the hardware test earlier
in the day, and keep the sample frames and logs, so nothing depends
on the evening.

## Hardware to bring

- ArduCam 12MP Camera Module 3, TNBA1392, UC-A37 Rev B, believed
  B0312 series, Sony IMX708, with its ribbon cable. Check the cable
  seats fully at both ends, blue side facing the right way, before
  powering the Pi.
- Raspberry Pi 5 with a power supply that can hold 5V under load,
  and a microSD card with Raspberry Pi OS Bookworm or Trixie.
  libcamera and Picamera2 support the IMX708 on Bullseye, Bookworm
  and Trixie.
- A laptop with the `csi-camera-fprime` workspace on it, so sample
  frames can be checked on the spot with `tools/verify_real_frame`.
- A lens cap or a dark cloth, for the black frame check in step 3.
- A well lit scene and a printed test pattern if you have one, for
  the exposure and focus checks.

## Before you capture anything

Run these on the Pi. Each one has a pass condition. Stop and fix
the step that fails before moving on.

### 1. The camera is detected

```sh
v4l2-ctl --list-devices
libcamera-hello --list-cameras
```

Pass: the IMX708 shows up, normally as `/dev/video0` or as
`imx708` in the libcamera list. Fail: no camera found. Check the
ribbon cable seating, the camera connector on the Pi, and that
the OS is Bookworm or newer. Do not proceed on a guess.

### 2. A live preview works

```sh
rpicam-hello -t 5000
```

Pass: a preview window shows a live image for 5 seconds. Fail:
black preview or an error. Check the lens cap is off, the cable,
and the libcamera version. Note the exposure behaviour: does the
image settle quickly, or does it hunt.

### 3. A still capture works, and the raw file is real

```sh
rpicam-still -o test.jpg --raw -t 2000
ls -la test.jpg test.dng
```

Pass: both files exist and are non-empty. The `.dng` file is the
raw Bayer data. Fail: only the JPEG exists, or the raw file is
empty. Record the exact command and error.

## Sample frames to collect

Collect one frame in each mode the stack supports, plus one black
frame. Save them with names that say what they are. These files
are the evidence for the whole test, so keep them.

| File | Command | What it proves |
|---|---|---|
| `frame_4608x2592.raw` | `rpicam-still --raw --width 4608 --height 2592 -o frame_4608.jpg -t 2000` then take the `.dng` raw, or capture raw via V4L2 | Full resolution RAW10 size and stride |
| `frame_2304x1296.raw` | same, `--width 2304 --height 1296` | 2304x1296p56 mode |
| `frame_1536x864.raw` | same, `--width 1536 --height 864` | 1536x864p120 mode |
| `frame_black.raw` | same as the 1280x720 capture, lens cap on | Black level and dead pixels |

If `rpicam-still --raw` gives you a DNG instead of a plain raw
file, extract the raw Bayer bytes from the DNG (the DNG holds the
same RAW10 data with a header), and note the exact extraction
step in the results. The checker tool below takes a plain raw
file, not a DNG.

## Check each sample frame on the laptop

The checker is `tools/verify_real_frame.cpp`, already built as
`tools/verify_real_frame` in this workspace. Build it again if
needed with:

```sh
cd tools
g++ -std=c++14 -O2 -Wall -Wextra \
  -I../fsw/lioness-sw/LionessSw/Components/ImageProcessing \
  verify_real_frame.cpp -o verify_real_frame
```

Run it on every sample frame:

```sh
./verify_real_frame frame_4608x2592.raw 4608 2592 frame_4608
./verify_real_frame frame_2304x1296.raw 2304 1296 frame_2304
./verify_real_frame frame_1536x864.raw 1536 864 frame_1536
./verify_real_frame frame_black.raw 1280 720 frame_black
```

Pass for a normal frame: exit 0, size matches packed RAW10 with
no row padding, pixel values look like a real image, first,
middle and last rows non-black, and the preview PNGs look right
when you open them. The black frame should fail the pixel checks
in the expected way: all black, min and max 0. That failure is
the black frame working as intended.

If a normal frame fails the size check, the driver is padding
rows or the format is not RAW10 packed. Run the checker again
with the stride from `v4l2-ctl`:

```sh
v4l2-ctl --device /dev/video0 --get-fmt-video
./verify_real_frame frame_4608x2592.raw 4608 2592 <strideFromV4l2> frame_4608
```

If the padded stride passes, record the stride. The F Prime
stack assumes stride `width * 10 / 8`, so a padded stride means
the stack must be told about the padding before it runs on this
camera. That is a one line change in the Camera component, not a
redesign, but do not run the stack until it is made.

Also record from `v4l2-ctl --device /dev/video0 --list-formats-ext`:
the exact pixel format the driver reports (expect SRGGB10),
the sizes it offers, and the frame intervals. If the driver
adjusts a requested format, record what it adjusts to.

## Run the F Prime stack on the Pi

Two ways to get the stack onto the Pi. Use whichever is ready
on the day.

- Native build on the Pi: copy the `csi-camera-fprime`
  workspace to the Pi, install the F Prime requirements there,
  then `fprime-util generate` and `fprime-util build`. Slow but
  simple, and it proves the stack builds on the target OS.
- Cross-compile from the Mac: Monish set up an ARM cross-compile
  toolchain for the Raspberry Pi 5 on his Mac on Oct 7, 2026.
  Build there and copy the deployment binary to the Pi.

Then, on the Pi, run the deployment binary and drive the same
commands the end-to-end test drives on the host:

- TakePicture, payload, 1280x720. Expect a preview.jpg, a
  preview.png and a lossless.zlib in storage, and exactly one
  completion.
- TakeStarPicture, 1280x720. Expect the raw frame to reach
  StarTracker, and no preview or lossless product for the star
  frame.
- Fire both at the same time. Expect both to succeed, one
  completion each. This is the per-device fix from Nov 16, and
  Wednesday is the first time it runs on real devices.
- Fire the same device twice at once. Expect the second to
  return Busy with no file written.

Record the measured times for each step: capture time, preview
time, lossless time, and total time. Compare them with the host
simulation timings in RESULTS.md (capture about 122 ms,
processing about 12 ms at 1280x720). The Pi numbers will be
different. Different is expected. What matters is whether they
fit the time budget the team agrees for the flight computer.

## Focus and exposure checks

- Autofocus: point at a near object, capture, point at a far
  object, capture. Both previews should be sharp. If the focus
  hunts or sticks, record it. The IMX708 has PDAF autofocus,
  and the stack does not control focus yet, so this is a
  finding to bring back, not a failure of the stack.
- Exposure: capture the same scene in bright light and in dim
  light. Check the preview PNGs. Note whether the camera
  auto-exposes, and how long it takes to settle.

## Results to bring back

Fill in this table on the day. Every row is a fact the team
needs, not a judgement.

| Check | Result | Measured value |
|---|---|---|
| Camera detected (`v4l2-ctl`, libcamera) | pass / fail | device path and driver name |
| Live preview | pass / fail | |
| Still capture with raw | pass / fail | file sizes |
| 4608x2592 frame, checker exit code | pass / fail | size, stride, mean |
| 2304x1296 frame, checker exit code | pass / fail | size, stride, mean |
| 1536x864 frame, checker exit code | pass / fail | size, stride, mean |
| Black frame | pass / fail | min, max |
| Driver pixel format | | format from v4l2-ctl |
| Driver stride | | bytes per row |
| F Prime build on Pi | pass / fail | build time, method used |
| Payload capture on Pi | pass / fail | capture, preview, lossless times |
| Star capture on Pi | pass / fail | capture time |
| Both at once on Pi | pass / fail | both completions seen |
| Same device twice | pass / fail | second returns Busy |
| Autofocus near and far | sharp / hunts / stuck | |
| Exposure bright and dim | settles / hunts | settle time |

## Go and no-go

Go: the checker passes on the sample frames with the packed
stride, or with a recorded padded stride that the stack is
updated for, and the F Prime stack runs on the Pi with the
captures above passing. The stack is then ready for the next
step, which is the real V4L2 HAL path with poll plus timeout
against the real device, and timing measurements on the target.

No-go, fix first: the checker fails on size for every frame,
the driver reports a different pixel format than SRGGB10, the
stack does not build on the Pi, or a capture on the Pi hangs
instead of completing or timing out. A hang is the one result
that must not be worked around on the day. Record it, stop the
run, and bring the log back.

## What to do with the sample frames after Wednesday

Copy the sample frames and the filled results table into this
workspace, under `hardware/2026-11-18/`, with the checker
output for each frame. Then update the stack's stride
assumption if the driver pads rows, and replace the simulated
120 ms latency and 500 ms timeout with the measured Pi numbers
in the docs. The next hardware step after that is the real V4L2
HAL against `/dev/video0`, then timing on the flight computer.
