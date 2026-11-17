#!/usr/bin/env python3
"""Drive the LionessSw FlatSat deployment over UART via the GDS comm adapter.

Run with the fprime venv python:
  fprime-venv/bin/python gds_drive.py

Expects, already running:
  - the deployment binary with -u /tmp/ttyV0
  - the GDS comm adapter bridging /tmp/ttyV1 to TCP ground on port 50050
    (python -m fprime_gds.executables.comm --communication-selection uart
     --uart-device /tmp/ttyV1 --uart-baud 115200 -d <build dir>)
  - the deployment working directory to be the directory this script runs in,
    so camera_storage/ lands here.

Sends, in order:
  1. TakePayloadPicture 1280x720 BOTH, await PictureStored
  2. TakeStarPicture 1280x720, await StarFrameDelivered
  3. Two TakePayloadPicture commands back to back, expect a CameraBusy event
     for the second one and no second set of products.
Prints PASS/FAIL per step and exits 0 only if all pass.
"""
import os
import sys
import time

WORKSPACE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(WORKSPACE, "fsw/lioness-sw/build-fprime-automatic-native")
DICT = os.path.join(BUILD, "LionessSw/FlatSat/Top/FlatSatTopologyDictionary.json")
TTS_URI = "tcp://127.0.0.1:" + (sys.argv[1] if len(sys.argv) > 1 else "50050")

from fprime_gds.common.models.dictionaries import Dictionaries
from fprime_gds.common.pipeline.standard import StandardPipeline
from fprime_gds.common.transport import ThreadedTCPSocketClient
from fprime_gds.common.utils.config_manager import ConfigManager
from fprime_gds.common.testing_fw.api import IntegrationTestAPI


def check_products(prefix, request_id, expect_all=True):
    """Check the three products for one request number in camera_storage/."""
    import zlib
    results = {}
    d = os.path.join(os.getcwd(), "camera_storage")
    jpg = os.path.join(d, f"payload_{request_id}_preview.jpg")
    png = os.path.join(d, f"payload_{request_id}_preview.png")
    z = os.path.join(d, f"payload_{request_id}_lossless.zlib")
    ok = True
    for path, magic, label in [
        (jpg, b"\xff\xd8\xff\xe0", "JPEG magic FF D8 FF E0"),
        (png, b"\x89PNG", "PNG magic 89 50 4E 47"),
    ]:
        if not os.path.exists(path):
            print(f"  [{prefix}] FAIL: missing {path}")
            ok = False
            continue
        with open(path, "rb") as f:
            head = f.read(4)
        size = os.path.getsize(path)
        if head != magic:
            print(f"  [{prefix}] FAIL: {path} bad magic {head.hex()}, size {size}")
            ok = False
        else:
            print(f"  [{prefix}] PASS: {os.path.basename(path)} {size} bytes, {label}")
        results[label] = size
    if not os.path.exists(z):
        print(f"  [{prefix}] FAIL: missing {z}")
        ok = False
    else:
        raw = zlib.decompress(open(z, "rb").read())
        size = os.path.getsize(z)
        if len(raw) != 1152000:
            print(f"  [{prefix}] FAIL: {z} decompresses to {len(raw)} bytes, expected 1152000")
            ok = False
        else:
            print(f"  [{prefix}] PASS: {os.path.basename(z)} {size} bytes, decompresses to 1152000 bytes")
    return ok


def main():
    print("Loading dictionary:", DICT)
    dictionaries = Dictionaries()
    dictionaries.load_dictionaries(DICT, None, None)
    config = ConfigManager.get_instance()
    if dictionaries.typedefs_name:
        for name, type_dict in dictionaries.typedefs_name.items():
            config.set_type(name, type_dict)
    if dictionaries.constant_name:
        for name, value in dictionaries.constant_name.items():
            config.set_constant(name, value)

    pipeline = StandardPipeline()
    pipeline.transport_implementation = ThreadedTCPSocketClient
    pipeline.setup(
        config=config,
        dictionaries=dictionaries,
        file_store="/tmp/root",
        logging_prefix="/tmp/gds_logs",
        data_logging_enabled=False,
    )
    # Connect with a bounded retry. The comm adapter may need a
    # few seconds to start listening, but a connect that never
    # succeeds must fail loudly instead of hanging the demo.
    print("Connecting to comm adapter at", TTS_URI)
    connected = False
    deadline = time.time() + 20
    while time.time() < deadline:
        try:
            pipeline.connect(TTS_URI)
            connected = True
            break
        except Exception as e:
            print(f"  connect retry: {e}")
            time.sleep(1.0)
    if not connected:
        print("  FAIL: could not connect to the comm adapter in 20 s")
        return 1
    time.sleep(1.0)
    api = IntegrationTestAPI(pipeline)
    all_ok = True

    # Wait for the deployment to be talking: look for any event in the history.
    print("Waiting for downlink dataflow...")
    try:
        api.wait_for_dataflow(timeout=15)
        print("  PASS: downlink dataflow seen")
    except Exception as e:
        print(f"  FAIL: no downlink dataflow in 15 s: {e}")
        pipeline.disconnect()
        return 1

    # Step 1: payload picture, BOTH.
    # The deployment numbers requests from when it started, so read the
    # request number out of the PictureStored event instead of assuming 1.
    print("Step 1: TakePayloadPicture 1280x720 BOTH")
    t0 = time.time()
    first_request = None
    try:
        stored = api.send_and_await_event(
            "FlatSat.payloadManager.TakePayloadPicture",
            [1280, 720, "BOTH"],
            events=["FlatSat.payloadManager.PictureStored"],
            timeout=10,
        )
        text = stored[0].get_display_text()
        print(f"  PASS: PictureStored event received in {time.time()-t0:.2f} s: {text}")
        import re
        m = re.search(r"Payload picture (\d+) stored", text)
        first_request = int(m.group(1)) if m else None
    except Exception as e:
        print(f"  FAIL: no PictureStored event: {e}")
        all_ok = False
    if first_request is not None:
        if not check_products("step1", first_request):
            all_ok = False
    else:
        print("  FAIL: could not read the request number from PictureStored")
        all_ok = False

    # Step 2: star picture.
    print("Step 2: TakeStarPicture 1280x720")
    t0 = time.time()
    try:
        api.send_and_await_event(
            "FlatSat.starTrackerManager.TakeStarPicture",
            [1280, 720],
            events=["FlatSat.starTrackerManager.StarFrameDelivered"],
            timeout=10,
        )
        print(f"  PASS: StarFrameDelivered event received in {time.time()-t0:.2f} s")
    except Exception as e:
        print(f"  FAIL: no StarFrameDelivered event: {e}")
        all_ok = False

    # Step 3: same-device double submit, expect CameraBusy on the second.
    # The two commands here are the next two request numbers after step 1.
    # The first of the pair stores its three products, the second is refused
    # as Busy and must store nothing.
    print("Step 3: two TakePayloadPicture back to back, expect Busy on second")
    try:
        api.send_command("FlatSat.payloadManager.TakePayloadPicture", [1280, 720, "BOTH"])
        api.send_command("FlatSat.payloadManager.TakePayloadPicture", [1280, 720, "BOTH"])
        busy = api.await_event(
            "FlatSat.payloadManager.CameraBusy",
            timeout=10,
        )
        print(f"  PASS: CameraBusy event received for second submit: {busy}")
    except Exception as e:
        print(f"  FAIL: no CameraBusy event: {e}")
        all_ok = False
    # Wait for the first of the two to finish, then check the files.
    time.sleep(2.0)
    d = os.path.join(os.getcwd(), "camera_storage")
    pair_first = first_request + 1 if first_request is not None else None
    pair_second = first_request + 2 if first_request is not None else None
    first = [f for f in os.listdir(d) if f.startswith(f"payload_{pair_first}_")] if (os.path.isdir(d) and pair_first is not None) else []
    second = [f for f in os.listdir(d) if f.startswith(f"payload_{pair_second}_")] if (os.path.isdir(d) and pair_second is not None) else []
    if len(first) == 3:
        print(f"  PASS: first of the pair stored payload_{pair_first} files: {sorted(first)}")
    else:
        print(f"  FAIL: first of the pair should have stored 3 payload_{pair_first} files, found: {first}")
        all_ok = False
    if second:
        print(f"  FAIL: second submit stored files: {second}")
        all_ok = False
    else:
        print(f"  PASS: no payload_{pair_second} files, second submit stored nothing")

    pipeline.disconnect()
    print("RESULT:", "ALL PASS" if all_ok else "FAILURES PRESENT")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
