# ImagePorts.fpp
# Shared camera types and ports for the LIONESS camera stack.
# Used by Camera, ImageProcessing, PayloadManager and
# StarTrackerManager. See the design doc in sdd.md (Issue 44) and
# the README in the workspace root.
# Style rule for this project: plain punctuation only, no em dashes.

module LionessSw {

    # Which physical camera a request is for. Payload camera is
    # the ArduCam IMX708 on MIPI CSI. Star tracker camera is on
    # Parallel CSI. Its exact sensor and format were never given
    # by the team, so the simulation uses 1280x720 RAW10 for it.
    # Confirm the real star camera format before flight.
    enum CameraId {
        PAYLOAD = 0
        STAR_TRACKER = 1
    }

    # What ImageProcessing should produce from a captured frame.
    # RAW means no processing: the raw frame goes straight to the
    # StarTrackerManager. Only the star tracker path uses RAW.
    enum CaptureOperation {
        PREVIEW = 0
        LOSSLESS = 1
        BOTH = 2
        RAW = 3
    }

    # Result of a capture or processing job.
    enum CaptureStatus {
        OK = 0
        TIMEOUT = 1
        BUSY = 2
        INVALID = 3
        CAPTURE_ERROR = 4
    }

    # Short validating submit to the Camera component. Runs on the
    # caller thread and returns at once with Accepted (OK), Busy
    # or Invalid. Accepted means exactly one CaptureDone will
    # follow. Busy and Invalid mean no completion will follow and
    # no buffer was retained. Never blocks on capture or encoding.
    port CaptureSubmit(
        requestId: U32,
        cameraId: CameraId,
        width: U32,
        height: U32,
        operation: CaptureOperation,
        timeoutMs: U32
    ) -> CaptureStatus

    # One completion per accepted capture request. Carries metadata
    # plus the image data as Fw.Buffer values. preview, png and
    # lossless are the ImageProcessing outputs, valid only when
    # the matching operation was requested and status is OK.
    # preview is the JPEG preview, png is the PNG preview of the
    # same 320x240 8-bit grayscale frame, and lossless is the
    # zlib stream. The PNG is a viewable preview, not a lossless
    # product: PNG is 8-bit, so it cannot hold the RAW10 frame
    # byte for byte. source is the raw frame, valid only for RAW
    # captures. Empty buffers have size 0. Buffers are returned
    # by the receiver: processed buffers go back through
    # ReturnBuffer, raw frames through ReturnFrame.
    port CaptureDone(
        requestId: U32,
        status: CaptureStatus,
        cameraId: CameraId,
        width: U32,
        height: U32,
        frameBytes: U32,
        frameCounter: U32,
        previewBytes: U32,
        losslessBytes: U32,
        pngBytes: U32,
        sourceChecksum: U64,
        preview: Fw.Buffer,
        lossless: Fw.Buffer,
        png: Fw.Buffer,
        source: Fw.Buffer
    )

    # Short validating submit to ImageProcessing. Runs on the
    # caller thread and returns at once. Accepted means exactly
    # one ProcessDone will follow. The source buffer must stay
    # valid until that completion arrives. cameraId travels
    # with the job and comes back on ProcessDone, so the Camera
    # can match the completion to the right device slot: the
    # two managers number their requests independently, so a
    # requestId alone is not unique across cameras.
    port ProcessImage(
        requestId: U32,
        cameraId: CameraId,
        width: U32,
        height: U32,
        operation: CaptureOperation,
        source: Fw.Buffer
    ) -> CaptureStatus

    # One completion per accepted processing job. preview holds
    # the JPEG preview bytes, png holds the PNG preview bytes
    # for the same 320x240 8-bit grayscale frame, and lossless
    # holds the zlib stream. The PNG is a viewable preview, not
    # a lossless product: it is 8-bit, so it cannot hold the
    # RAW10 frame byte for byte. All three buffers are owned by
    # ImageProcessing and must be returned through ReturnBuffer
    # after the receiver is done with them.
    port ProcessDone(
        requestId: U32,
        cameraId: CameraId,
        status: CaptureStatus,
        width: U32,
        height: U32,
        previewBytes: U32,
        losslessBytes: U32,
        pngBytes: U32,
        sourceChecksum: U64,
        preview: Fw.Buffer,
        lossless: Fw.Buffer,
        png: Fw.Buffer
    )

    # Return a processed buffer to ImageProcessing when the
    # receiver is done with it.
    port ReturnBuffer(
        buffer: Fw.Buffer
    )

    # Return a raw frame buffer to the Camera component when the
    # receiver is done with it.
    port ReturnFrame(
        buffer: Fw.Buffer
    )

    # Deliver a raw frame to the StarTracker component.
    port ProcessFrame(
        frame: Fw.Buffer,
        width: U32,
        height: U32,
        frameCounter: U32
    )

}
