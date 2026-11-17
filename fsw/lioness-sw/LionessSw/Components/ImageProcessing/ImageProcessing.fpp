module Components {

    @ ImageProcessing component (Issue 44). Receives a captured
    @ image buffer and produces a small JPEG preview and a PNG
    @ preview for deciding which images to downlink, or a
    @ losslessly compressed version recoverable byte for byte,
    @ or both. Both previews show the same 320x240 8-bit
    @ grayscale frame, encoded with stb_image_write. The PNG is
    @ a viewable preview, not a lossless product: PNG is 8-bit,
    @ so it cannot hold the RAW10 frame byte for byte. The
    @ source image is left intact so it remains usable by
    @ StarTracker. One active component: a short synchronous
    @ submit validates the request and reserves one job, a
    @ worker thread performs the encoding and sends one
    @ completion per accepted request. Further submissions
    @ return BUSY while a job is outstanding.
    active component ImageProcessing {

        @ Set the JPEG quality for future previews (1 to 100).
        @ Default 30, the temporary development value from sdd.md.
        async command SetPreviewQuality(quality: U32)

        @ Short validating submit. Runs on the caller thread and
        @ returns at once. See Ports/ImagePorts.fpp.
        sync input port processImage: LionessSw.ProcessImage

        @ Return a processed buffer when done with it.
        sync input port returnBuffer: LionessSw.ReturnBuffer

        @ One completion per accepted job.
        output port processDone: LionessSw.ProcessDone

        @ A job completed successfully.
        event JobComplete(
            requestId: U32, previewBytes: U32, losslessBytes: U32
        ) severity activity high id 0 format "Image job {} done, preview {} bytes, lossless {} bytes"

        @ A job failed, with zero valid output bytes.
        event JobFailed(
            requestId: U32, status: LionessSw.CaptureStatus
        ) severity warning high id 1 format "Image job {} failed, status {}"

        @ A submission arrived while a job was outstanding.
        event JobBusy(
            requestId: U32
        ) severity warning low id 2 format "Image job {} busy"

        @ A submission failed validation.
        event JobInvalid(
            requestId: U32
        ) severity warning low id 3 format "Image job {} invalid"

        @ A buffer was returned that this component does not own.
        event UnknownBufferReturned(
        ) severity warning low id 4 format "Unknown buffer returned to ImageProcessing"

        @ Jobs completed successfully.
        telemetry JobsCompleted: U32

        @ Jobs failed.
        telemetry JobsFailed: U32

        @ Submissions rejected as busy.
        telemetry JobsBusy: U32

        @ Whether previews in this build are real JPEG (1).
        @ Always 1 now that stb_image_write is vendored: the
        @ preview no longer depends on a system JPEG library.
        telemetry PreviewIsJpeg: U32

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending command registrations
        command reg port cmdRegOut

        @ Port for receiving commands
        command recv port cmdIn

        @ Port for sending command responses
        command resp port cmdResponseOut

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut

    }
}
