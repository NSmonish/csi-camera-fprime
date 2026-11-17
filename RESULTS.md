# LIONESS camera stack in F Prime: measured results

Date: Nov 16, 2026. Updated the same day with two changes:
per-device job slots in Camera, and stb for the JPEG and PNG
previews. Workspace: `~/workspace/csi-camera-fprime/`
(a copy of columbiaspace/cubesat-lioness main at 5b2fc29, plus the
F Prime v4.1.1 submodule at 94b4457). Nothing was pushed to GitHub.
The original clones `~/workspace/csi-lioness-check`,
`~/workspace/csi-pr55`, and `~/workspace/csi-camera-capture/` were
not modified (verified by git status at the end of this file).

## What was built

- V4L2 HAL (`LionessSw/V4L2/`): PR 55 V4L2Camera adapted to a
  `CameraHal::ICameraDevice` interface, with the blocking flaw
  fixed. `dequeueFrame(int timeoutMs)` calls `poll()` first and
  only calls VIDIOC_DQBUF when the descriptor is readable. A
  simulated device (`SimCameraDevice`) stands in for hardware.
  The HAL has no F Prime dependencies.
- Camera (`LionessSw/Components/Camera/`): active component.
  Async TakePicture command, short validating submit, one
  worker thread per device, one completion per accepted
  request, Busy per device, timeout, buffer release before
  completion. A payload capture and a star capture submitted
  at the same time both run and both complete. A second
  submit for the same device while that device is busy still
  returns Busy immediately, keeps no buffer, and produces no
  completion.
- ImageProcessing (`LionessSw/Components/ImageProcessing/`):
  active component per sdd.md (Issue 44). Shared types in
  `LionessSw/Ports/ImagePorts.fpp`. Previews are real JPEG
  and PNG, both 320x240 8-bit grayscale, both encoded by the
  vendored stb_image_write header (v1.18, no system library),
  JPEG at quality 30 by default. The libjpeg path and the
  downsample stub fallback are gone. Lossless is real zlib.
  Source is never written to. The PNG is a viewable preview,
  not a lossless product: stb PNG is 8-bit, so a PNG of a
  RAW10 frame is not byte for byte lossless. The zlib product
  stays the true byte for byte archive.
- PayloadManager (`LionessSw/Components/PayloadManager/`):
  active. Requests payload captures, stores three products
  (preview.jpg, preview.png, lossless.zlib) through a storage
  stub (host files, labelled a stub, not flight storage).
  Downlink is out of scope.
- StarTrackerManager (`LionessSw/Components/StarTrackerManager/`):
  active. Requests raw star captures through the same shared
  V4L2 layer (second simulated camera instance) and hands the
  raw frame to StarTracker. The full LOST star ID algorithm was
  NOT integrated; the delivered interface is the raw frame
  handoff.
- StarTracker (existing, modified): gained a real processFrame
  handler (validates RAW10 packed size, records the frame, emits
  an event and telemetry) and is now instantiated in the
  topology.
- Topology: all components registered, base IDs 0x10006000
  (camera), 0x10007000 (imageProcessing), 0x10008000
  (payloadManager), 0x10009000 (starTrackerManager),
  0x1001A000 (starTracker), wired in topology.fpp as the
  diagram in README.md shows.

## Build

Native build, `fprime-util build`: exit 0.
FlatSat deployment binary:
`fsw/lioness-sw/build-fprime-automatic-native/bin/Linux/LionessSw_FlatSat`,
2,705,976 bytes. It starts on the host and stays up without
UART hardware (UART open errors are expected, no UARTs are
attached). 52 opcodes register at startup, including the four
new commands, verified from the event log:

```
Opcode 0x10006000 registered to port 10 slot 32   (TakePicture)
Opcode 0x10007000 registered to port 12 slot 41   (SetPreviewQuality)
Opcode 0x10008000 registered to port 14 slot 47   (TakePayloadPicture)
Opcode 0x10009000 registered to port 16 slot 48   (TakeStarPicture)
```

UT build, `fprime-util build --ut`: exit 0. ASan, LeakSanitizer
and UBSan are ON by default in this build; TSan is OFF.

## Unit tests and end-to-end run (ASan / LSan / UBSan build)

`ctest` in `build-fprime-automatic-native-ut`, 21 tests total,
run 5 times in a row after the final fixes, 6 of 6 suites pass
every time:

```
1/6 Test #1: LionessSw_Components_StarTracker_ut_exe ..........   Passed    0.09 sec
2/6 Test #2: LionessSw_Components_Camera_ut_exe ...............   Passed    1.04 sec
3/6 Test #3: LionessSw_Components_ImageProcessing_ut_exe ......   Passed    1.04 sec
4/6 Test #4: LionessSw_Components_PayloadManager_ut_exe .......   Passed    0.08 sec
5/6 Test #5: LionessSw_Components_StarTrackerManager_ut_exe ...   Passed    0.09 sec
6/6 Test #6: LionessSw_FlatSat_Top_ut_exe .....................   Passed    1.22 sec

100% tests passed, 0 tests failed out of 6
Total Test time (real) =   4.24 sec
```

Breakdown: StarTracker 2 tests, Camera 7 (new: ConcurrentBoth,
payload BOTH plus star RAW at the same time, both complete),
ImageProcessing 5, PayloadManager 3, StarTrackerManager 3,
Top end-to-end 1. Total test time 4.01 to 4.45 sec across runs.

The end-to-end run drives a payload capture and a star
capture through the real command ports at the same time, and
verifies: all three payload products land in the storage stub
while the raw star frame reaches StarTracker, the JPEG and
PNG decode to the same 320x240 frame (sampled mean pixel
difference 4.6 grey levels, 94 percent within 16, worst 98
at the sharp sawtooth edges; the test bounds are mean at most
8 and at least 90 percent within 16), exactly one completion
per accepted request, Busy rejection of a second submit for
the same device (EXECUTION_ERROR, no payload_2 files), a
timed-out star capture that delivers no frame, recovery by
the next capture, and buffer reuse by a later payload
picture (requestId 3).

Measured end-to-end products (1280x720 frame, from the
passing run, verified after the run):

```
payload_1_preview.jpg:   6,816 bytes, starts FF D8 FF E0 (real JPEG, stb)
payload_1_preview.png:   1,661 bytes, starts 89 50 4E 47 (real PNG, stb)
payload_1_lossless.zlib: 15,677 bytes, decompresses to 1,152,000 bytes
                         = 1280 x 720 x 10 / 8, byte for byte equal
                         to the simulated frame (counter 1)
```

The JPEG grew from 6,345 bytes (libjpeg-turbo) to 6,816
bytes (stb) at the same quality 30: different encoder, same
frame, same quality setting. The PNG is 8-bit, so it is a
viewable preview only; the zlib product stays the true
byte for byte archive.

## ThreadSanitizer

Separate build cache `build-fprime-tsan`, generated with
`-DENABLE_SANITIZER_THREAD=ON -DENABLE_SANITIZER_ADDRESS=OFF
-DENABLE_SANITIZER_LEAK=OFF
-DENABLE_SANITIZER_UNDEFINED_BEHAVIOR=OFF
-DFPRIME_ENABLE_UT_COVERAGE=OFF`. Coverage had to be off: with
gcov on, TSan reported races in gcov coverage counters
(`__gcov0._ZN2Fw12SerializableC2Ev`), which are false
positives from the TSan plus gcov interaction, not program
data. Plain `ctest` does not work for this cache: the TSan
runtime fails to start on this VM unless ASLR is disabled, so
each executable is run under `setarch $(uname -m) -R`.

Final result, all 6 executables run under setarch with
`TSAN_OPTIONS=suppressions=TSAN_SUPPRESSIONS.txt`:

```
StarTracker       exit=0  [ PASSED ] 2 tests.  0 TSan warnings
Camera            exit=0  [ PASSED ] 7 tests.  0 TSan warnings
ImageProcessing   exit=0  [ PASSED ] 5 tests.  0 TSan warnings
PayloadManager    exit=0  [ PASSED ] 3 tests.  0 TSan warnings
StarTrackerManager exit=0 [ PASSED ] 3 tests.  0 TSan warnings
Top               exit=0  [ PASSED ] 1 test.   0 TSan warnings
```

The Top executable was run 5 times in a row under TSan after
the final fixes: 5 of 5 pass, 0 warnings each time.

The suppression file covers only two known races that live
outside the camera components. No component code is
suppressed.

1. F Prime GTestBase event history. The generated
   `*TesterBase` classes record events in plain vectors and a
   plain `eventsSize` counter with no lock. The Camera and
   ImageProcessing workers log events from their worker
   threads. That is the design in sdd.md (the worker sends the
   completion) and it is safe in flight, where the log port
   goes to the event logger queue, which is built for
   concurrent loggers. The UT harness is the only consumer
   that is not thread safe. Evidence: the raced address is
   `this->eventsSize` in `CameraTesterBase::logIn_*`, written
   by the worker thread and by the main thread.

2. F Prime v4.1.1 `Os/Posix/Task.cpp`. `PosixTask::create`
   calls `pthread_create` first and only afterwards copies
   the task name into the handle, while the new thread's entry
   calls `set_task_name` on the same buffer. Upstream
   framework race, present in every F Prime deployment that
   starts tasks. Benign in practice: both sides write the
   same name bytes.

## Two real bugs found by the sanitizer runs, both fixed

### 1. Uninitialized `ActiveComponentBase::m_stage` (upstream F Prime)

The Top end-to-end test aborted deterministically under TSan
at `Fw/Comp/ActiveComponentBase.cpp:101`, the default case of
the lifecycle switch in `s_taskStateMachine`. Root cause:
F Prime v4.1.1 never initializes `m_stage`. No constructor,
`init()`, or `start()` sets it to CREATED. The first task
loop call reads uninitialized memory. It works only when the
memory happens to be zero (CREATED is 0). The deployment heap
allocates its components, so it worked by luck on fresh zeroed
heap pages. The host E2E test stack allocates its components,
and under TSan the stack slots hold nonzero bytes, so the
assert fired on the first run.

Fix, in the vendored copy only:
`lib/fprime/Fw/Comp/ActiveComponentBase.hpp`, one line:

```
Lifecycle m_stage = Lifecycle::CREATED;
```

The E2E test keeps its Stack stack allocated on purpose, so
this test guards the patch: if it regresses, the test aborts
here. The real repo needs the same one line (or an upstream
F Prime fix) before flight. The patch is marked in the file
with a comment dated Nov 16, 2026.

### 2. E2E test observation ordering and teardown (test bug, fixed)

Under TSan the Top test failed once at
`CameraStackTestMain.cpp:273`: `getStarFramesTaken()` was 0
right after `waitUntil(framesReceived == 1)` returned true.
The StarTrackerManager increments `starFramesTaken_` after
it hands the frame to StarTracker, in the same handler, so
the test observed the downstream counter before the manager
counter was incremented. Same pattern for the payload
pictures: the files are written before `picturesTaken_` is
incremented. Fixed in the test, not the components: the wait
predicates now wait on the manager counters, which are
incremented last in the completion handlers. The StarTracker
counters are now `std::atomic` (they were plain U32, written
on the Camera worker thread and read from the test thread).
Teardown was also hardened: `~Stack` now stops the Camera
and ImageProcessing worker threads after the task threads
are joined, so a job that is still in flight finishes while
every component still exists. Before that fix, an early test
failure could destroy a manager while the Camera worker was
still about to call into it, which TSan caught as a
condition variable and vptr race during destruction.

## Earlier flake, already fixed

One `CameraTest.Busy` failure was observed in a full suite
run before the Camera race fix (5176 ms, `waitForPayloadCompletion`
timeout). Root cause was a real race in Camera.cpp: the worker
called `processImage_out` before recording the pending job,
so a fast processDone reply could arrive before the pending
job was recorded and was dropped as unknown. Fixed in the
component: the pending job is now recorded under the mutex
before `processImage_out` is called. After the fix the Camera
suite passed 6 of 6 five times in a row, and has not flaked
since (including all runs recorded above); it is now 7 tests
with the new ConcurrentBoth test.

## Environment notes

- `libjpeg-turbo8-dev` was installed Nov 16, 2026 for the
  original real JPEG preview, but the package had been
  removed from this VM between sessions (header and linker
  symlink gone, apt could not locate the package). It was
  reinstalled from the cached .deb
  (`/var/cache/apt/archives/libjpeg-turbo8-dev_2.1.5-2ubuntu2_amd64.deb`)
  with `dpkg -i`, no network needed. That dependency is now
  gone: the JPEG preview uses the vendored stb header, and
  the libjpeg path plus the downsample stub fallback were
  removed the same day. Two fixes were needed while wiring
  stb in: stb_image_write.h v1.18 has no
  `stbi_write_jpg_to_mem`, so the JPEG encoder writes
  through a callback (`stbi_write_jpg_to_func`) that
  appends to the caller buffer; and the JPEG to PNG pixel
  check bounds had to be measured, not guessed (sampled
  mean 4.6, 94 percent within 16 grey levels, worst 98 at
  the sharp sawtooth edges, on the gradient frame at
  quality 30), because quality 30 JPEG ringing near sharp
  edges is far larger than a naive 8 grey level bound.
- TSan on this VM needs ASLR disabled:
  `setarch $(uname -m) -R <binary>`. Without it the TSan
  runtime aborts at startup with "unexpected memory mapping".
  That is an ASLR clash in the TSan runtime on this VM, not a
  code defect. The same clash was seen in the earlier plain
  C++ simulation work.

## Design finding, fixed Nov 16, 2026

The Camera used to hold one job at a time across both
cameras. Two managers firing at once meant the second submit
returned Busy and the request was dropped, with
EXECUTION_ERROR to the commanding side. That was a limit of
the Camera component, not of V4L2: each device already had
its own V4L2 buffer ring, and V4L2 queues buffers per
device, not requests from two managers.

Fixed: the Camera now has one job slot and one worker thread
per device. A payload capture and a star capture submitted
at the same time both run and both complete, verified by the
new Camera unit test (ConcurrentBoth) and by the Top
end-to-end test, which now fires both managers at once. A
second submit for the same device while that device is busy
still returns Busy immediately, keeps no buffer, and
produces no completion, and the end-to-end test still
verifies that case. There is still no queue and no retry:
if a manager fires while its own device is busy, that
request is dropped with EXECUTION_ERROR. For the mission, a
dropped star frame is recovered by the next scheduled star
capture. Do not treat the same-device Busy rejection as a
failure.

## What is still not flight code

- No real IMX708, no real V4L2 driver, no exposure or
  autofocus. The 120 ms simulated latency and 500 ms timeout
  are chosen values, not target measurements. Processing times
  are host timings.
- RAW10 packing must be checked against a real sample frame;
  the real driver may pad rows, so real stride may differ
  from width times 10 over 8.
- No target timing, no radiation, thermal, power, or
  long duration proof.
- The full LOST star identification algorithm from
  `test/star-tracker/` is NOT integrated into the F Prime
  StarTracker component. The delivered interface is the
  manager to StarTracker raw frame handoff.
- Storage is a host file stub, not flight storage. Downlink
  is out of scope and was not built.
- The star tracker camera format is an assumption: 1280x720
  RAW10 on Parallel CSI. The team never gave the star sensor
  or its format. Confirm it before flight.
- Previews are real JPEG and PNG from the vendored stb
  header, so no system image library is needed. Lossless is
  real zlib. The PNG is an 8-bit viewable preview, not a
  lossless product: a PNG of a RAW10 frame cannot be byte
  for byte lossless. The zlib product is the true archive.
- The E2E executable disables LeakSanitizer for itself only
  (`__lsan_disable()` in main, guarded by
  `#if defined(__SANITIZE_ADDRESS__)`): the four component
  message queues are freed by the protected
  `QueuedComponentBase::deinit()`, which only the per-component
  Tester friends can call, and this harness is not one. The
  queues are the framework's process lifetime allocations.
  Leak checking lives in the unit tests, which call deinit
  and are leak clean.
- The deployment has no host command path except the UARTs,
  so the end-to-end run drives the command ports directly
  through the test harness. The deployment binary itself was
  verified to start and stay up, and its four new commands
  register in the command dispatcher (see above).

## Verification that the originals are untouched

```
~/workspace/csi-lioness-check: clean, main 5b2fc29
~/workspace/csi-pr55:          clean, libcamera-combined 6fb56f3
~/workspace/csi-camera-capture: no changes made this session
```

Nothing was pushed to GitHub. In the new workspace, the
modified tracked files are the LionessSw CMakeLists, the
Components CMakeLists, the StarTracker component, the
FlatSat topology files, the Ports CMakeLists, the top level
README.md, and the vendored F Prime patch listed above.
New untracked directories are `LionessSw/Components/Camera/`,
`ImageProcessing/` (including the vendored `stb_image_write.h`
v1.18 and `stb_image.h`), `PayloadManager/`,
`StarTrackerManager/`, `LionessSw/Components/StarTracker/test/`,
`LionessSw/FlatSat/Top/test/`, `LionessSw/Ports/ImagePorts.fpp`,
`LionessSw/V4L2/`, plus `TSAN_SUPPRESSIONS.txt` at the top
of the workspace.
