# Demo products

These files were captured live by `demo/run_demo.sh` on Nov 16, 2026,
through the real GDS UART link, not by a unit test. A fresh deployment
numbers its requests from 1, so the first payload picture is request 1
and the first of the double submit pair is request 2.

- `payload_1_preview.jpg`, 6,816 bytes, real JPEG, starts FF D8 FF E0.
- `payload_1_preview.png`, 1,661 bytes, real PNG, 320x240, 8 bit.
- `payload_1_lossless.zlib`, 15,677 bytes, decompresses to exactly
  1,152,000 bytes, byte for byte equal to the simulated 1280x720 RAW10
  frame.
- `payload_2_preview.jpg`, `payload_2_preview.png`,
  `payload_2_lossless.zlib`: the same three products for the first
  command of the double submit pair. There are no `payload_3_*` files,
  because the second command of that pair was refused as Busy and
  stored nothing.

The frames are simulated. The cameras in the deployment are software
models of the ArduCam IMX708, not the real camera. See DEMO_GUIDE.md.
