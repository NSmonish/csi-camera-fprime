# Mac setup: LIONESS camera stack on a MacBook (Apple Silicon)

This gets the camera stack in `csi-camera-fprime` running on your
MacBook, so you can run the demo there on Wednesday Nov 18, 2026.
It covers the laptop demo only. The Pi demo has its own script,
`demo/run_demo_pi.sh`, and its own section in `DEMO_GUIDE.md`.

Times below are rough estimates, not promises. The long steps are
the downloads and the first build.

## Getting the code

Two ways to get the folder. Pick one.

From GitHub (the code lives at
https://github.com/NSmonish/csi-camera-fprime):

```sh
cd ~
git clone --recurse-submodules https://github.com/NSmonish/csi-camera-fprime.git
cd csi-camera-fprime
git apply --directory=fsw/lioness-sw/lib/fprime patches/fprime-m_stage.patch
```

The patch step matters. F Prime v4.1.1 never initializes
`ActiveComponentBase::m_stage`, and the submodule points at
unpatched upstream, so the build needs that one line applied.
Without it the deployment can hit an assert on startup.

From the bundle (if you were given the tar.gz file instead):

```sh
cd ~
mkdir -p csi-camera-fprime
cd csi-camera-fprime
tar -xzf ~/Downloads/csi-camera-fprime-mac-2026-11-16.tar.gz
```

The bundle already has the patch applied and the F Prime
source inside, so it needs no clone and no patch step. The
bundle has no folder inside it, just the files, so you must
extract it from inside the `csi-camera-fprime` folder as
above. Extracting it from your home folder scatters the
files into your home folder instead.

Either way, you end up with `~/csi-camera-fprime` on your Mac.

## 0. What you need before you start

- A MacBook with Apple Silicon (M1, M2, M3 or M4) and macOS 13 or newer.
- About 2 GB free disk space for the build, plus the bundle itself.
- Internet access. The Python install downloads F Prime tools from the internet.
- A terminal. Use the built in Terminal app.

## 1. Install the prerequisites (about 10 minutes)

Install the Xcode Command Line Tools. This gives you the C compiler.

```sh
xcode-select --install
```

If it says the tools are already installed, move on.

Install Homebrew if you do not have it. Homebrew is the Mac package
manager, and it is how you install socat.

```sh
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

Then install socat. The demo needs socat to make the virtual serial
pair between the flight software and the ground station.

```sh
brew install socat
```

Check what you have. Install Python 3.12 with Homebrew if you do
not have it yet. Do NOT use Python 3.14: the pinned
pydantic_core==2.23.4 has no prebuilt wheel for Python 3.14, so
pip fails trying to build it from source. The Linux build used
Python 3.12.

```sh
brew install python@3.12
/opt/homebrew/bin/python3.12 --version
socat -V | head -1
```

You want Python 3.12, for example 3.12.16. If `python3.12` is not
found on your PATH, use the full path `/opt/homebrew/bin/python3.12`
as shown above.

You do not need to install cmake or ninja separately. They come in
through pip in step 4, from the project requirements file.

## 2. Get the bundle onto your Mac (about 5 minutes)

Download the bundle file `csi-camera-fprime-mac.tar.gz` from the link
you were given. It is about 30 MB. Save it in your Downloads folder.

The bundle holds the full source tree, the F Prime framework source,
the demo scripts, the guides, the hardware plan, the frame checker,
and the demo products. It does NOT hold the Linux build folders, the
Linux Python venv, or any log files. Those are Linux only and would
not work on your Mac anyway.

Check the download. The file should be there and non empty.

```sh
ls -la ~/Downloads/csi-camera-fprime-mac.tar.gz
```

## 3. Extract it (about 1 minute)

```sh
cd ~
tar -xzf ~/Downloads/csi-camera-fprime-mac.tar.gz
```

This creates `~/csi-camera-fprime` on your Mac. Check it.

```sh
ls ~/csi-camera-fprime
```

You should see `DEMO_GUIDE.md`, `MAC_SETUP.md`, `demo`, `fsw`,
`tools`, and the rest.

If you already extracted the previous bundle, do not extract the
new one over it. The old bundle had two folders that differ only
by capitalisation (`Components/Camera` and `Components/camera`),
and macOS treats those as the same folder, which broke the test
build. Delete the old camera folder and the old build folders
first, then extract the new bundle:

```sh
rm -rf ~/csi-camera-fprime/fsw/lioness-sw/LionessSw/Components/camera
rm -rf ~/csi-camera-fprime/fsw/lioness-sw/build-fprime-automatic-native
rm -rf ~/csi-camera-fprime/fsw/lioness-sw/build-fprime-automatic-native-ut
rm -rf ~/csi-camera-fprime/fsw/lioness-sw/build-artifacts
cd ~
tar -xzf ~/Downloads/csi-camera-fprime-mac.tar.gz
```

Your Python venv (`fprime-venv`) is untouched by those deletes,
so you do not need to reinstall the Python tools. After the fresh
extract, re-run `fprime-util generate` and `fprime-util build`
from step 6, then the test build from step 7.

## 4. About the F Prime submodule: do NOT run git submodule update

Normal F Prime instructions say to run `git submodule update
--init --recursive`. Skip that step here. This bundle has no git
history in it, so that command has nothing to update, and it would
fail.

The F Prime framework source is already inside the bundle, in
`fsw/lioness-sw/lib/fprime`. You do not need to download it again.
The build in step 5 uses it as it is.

## 5. Create the Mac venv and install the tools (about 10 minutes)

Go into the project folder and make a fresh Python venv. This is
the Mac venv. It replaces the Linux one, which was left out of the
bundle on purpose. Make the venv with Python 3.12, not the system
python3. If your Mac python3 is 3.14, the pip install below will
fail on pydantic_core.

```sh
cd ~/csi-camera-fprime/fsw/lioness-sw
/opt/homebrew/bin/python3.12 -m venv fprime-venv
. fprime-venv/bin/activate
```

Your prompt should now start with `(fprime-venv)`. Install the
requirements. This downloads fprime-tools 4.1.0 (which gives you
`fprime-util` and `fprime-gds`), plus cmake and ninja, plus the
other pinned packages.

```sh
pip install -r requirements.txt
```

This takes a few minutes and prints a lot of lines. It is done
when the prompt comes back with no error at the end. Check the
tools are there.

```sh
fprime-util version-check
fprime-gds --help | head -3
```

If `pip install` fails on one package, read the last error lines
and paste them back. Do not try to fix it by installing packages
one by one unless the error names a specific package.

## 6. Build the deployment (about 5 to 15 minutes, first time)

Note: the V4L2 camera code used to include the Linux-only header
`linux/videodev2.h`, which does not exist on macOS and broke the
Mac build. This bundle fixes that with
`LionessSw/V4L2/videodev2_compat.h`, which uses the real Linux
header on Linux and matching definitions on macOS. The Mac build
now works with no extra step from you.

Stay in `fsw/lioness-sw` with the venv active. Generate, then build.

```sh
fprime-util generate
fprime-util build
```

The first build takes the longest, because it compiles the whole
F Prime framework plus the camera components. Later builds are
faster. It is done when the last line says the build finished with
exit 0, or words to that effect.

Check the binary is where the demo expects it. On macOS it lands
under `bin/Darwin`, not `bin/Linux`.

```sh
ls -la build-fprime-automatic-native/bin/Darwin/LionessSw_FlatSat
```

If that file is missing, the build did not finish. Read the error
at the end of the build output and paste it back.

## 7. Build the unit tests (about 5 to 15 minutes, first time)

Still in `fsw/lioness-sw` with the venv active.

```sh
fprime-util generate --ut --disable-sanitizers
fprime-util build --ut
```

The `--disable-sanitizers` flag is required on macOS. Without
it, the test programs never start: the AddressSanitizer runtime
deadlocks with the macOS loader during startup (it spins at
100% CPU inside `__asan::AsanInitFromRtl`, before `main()` runs,
confirmed by `sample` Nov 16, 2026). The tests still run and
check everything; only the extra memory-error checking is off
on the Mac. The Linux build keeps the sanitizers on.

Then run the tests.

```sh
cd build-fprime-automatic-native-ut
ctest
```

You should see 6 of 6 suites pass, 21 tests, in about 5 seconds.
That is the same result as on Linux. If a test fails, paste the
failing test name and the last lines of its output back.

Go back to the project folder for the demo.

```sh
cd ~/csi-camera-fprime/fsw/lioness-sw
```

## 8. Run the demo (about 30 seconds)

One command, from anywhere.

```sh
bash ~/csi-camera-fprime/demo/run_demo.sh
```

The script detects macOS on its own and uses the Darwin binary.
It prints PASS or FAIL for each step and should end with 11
passed, 0 failed. It leaves the captured products in
`demo/products/`. Open `demo/products/payload_1_preview.png`
to show the picture.

If the script says socat is missing, go back to step 1 and run
`brew install socat`. If it says the deployment binary is missing,
go back to step 6 and check the build finished. If a live GDS
command fails, read `demo/.run/gds_drive.log` and paste the last
lines back.

## 9. The frame checker (optional, for the hardware test)

The frame checker is already in the bundle at
`tools/verify_real_frame.cpp`. It needs no F Prime build. Build it
with the Mac compiler.

```sh
cd ~/csi-camera-fprime/tools
g++ -std=c++14 -O2 -Wall -Wextra \
  -I../fsw/lioness-sw/LionessSw/Components/ImageProcessing \
  verify_real_frame.cpp -o verify_real_frame
```

Run it on a real sample frame as described in
`HARDWARE_TEST_PLAN.md`.

## What to do when something fails

Paste back three things: the exact command you ran, the last 20
lines of output, and which step number you were on. That is
enough to tell whether it is a missing tool, a download problem,
or a build error.

Common failures and their fixes:

- `xcode-select: error: command line tools are already installed`.
  This is not an error. Move on.
- `brew: command not found`. Homebrew did not finish installing,
  or its folder is not on your PATH. Run the Homebrew install
  command again and follow its printed instructions about PATH.
- `pip install` fails with a network error. Check your internet
  connection and run the same command again.
- `fprime-util: command not found`. The venv is not active. Run
  `. fprime-venv/bin/activate` again from `fsw/lioness-sw`.
- `cmake: command not found` during the build. The pip install in
  step 5 did not finish. Run it again and check for errors.
- The demo says the binary is missing at `bin/Linux`. You are
  running the old script. Use the script from this bundle, which
  picks `bin/Darwin` on macOS.
- The demo hangs at the ground station step. Stop it with Ctrl-C
  and run it again. The script picks a fresh port each run, so a
  second run usually works.
