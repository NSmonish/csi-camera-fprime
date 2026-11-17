module Components {

    @ Camera component. Owns the two cameras through the shared
    @ V4L2 HAL layer (CameraHal::ICameraDevice): the payload camera
    @ on MIPI CSI and the star tracker camera on Parallel CSI.
    @ One active component. Each camera has its own job slot and
    @ its own worker thread, so a payload capture and a star
    @ capture can run at the same time. V4L2 queues buffers per
    @ device, it does not queue requests from two managers, so
    @ the per-device slots live here, not in the HAL. A short
    @ synchronous submit validates the request and reserves the
    @ job for that one device; a second submit for the same
    @ device while it is busy returns Busy. A worker thread
    @ polls the device with a timeout, and exactly one
    @ completion is sent per accepted request. The blocking flaw
    @ in PR 55 is fixed in the HAL: dequeue waits on poll() with
    @ a timeout and only calls VIDIOC_DQBUF when a frame is
    @ ready. Processed frames go through ImageProcessing; raw
    @ frames go straight to the StarTrackerManager. The caller
    @ never blocks: managers call the sync submit port and get
    @ Accepted, Busy or Invalid back at once.
    active component Camera {

        @ Take a picture from the ground. The command is accepted
        @ at once when the camera is free; the picture itself
        @ arrives later through the captureDone port of the
        @ matching manager, with an event and telemetry here.
        async command TakePicture(
            cameraId: LionessSw.CameraId,
            width: U32,
            height: U32,
            operation: LionessSw.CaptureOperation,
            timeoutMs: U32
        )

        @ Short validating submit from the PayloadManager.
        sync input port captureSubmitPayload: LionessSw.CaptureSubmit

        @ Short validating submit from the StarTrackerManager.
        sync input port captureSubmitStar: LionessSw.CaptureSubmit

        @ Completion from ImageProcessing, one per accepted job.
        async input port processDone: LionessSw.ProcessDone

        @ Return a raw frame buffer when done with it.
        sync input port returnFrame: LionessSw.ReturnFrame

        @ Completion to the PayloadManager.
        output port captureDonePayload: LionessSw.CaptureDone

        @ Completion to the StarTrackerManager.
        output port captureDoneStar: LionessSw.CaptureDone

        @ Submit a captured frame to ImageProcessing.
        output port processImage: LionessSw.ProcessImage

        @ A capture started.
        event CaptureStarted(
            requestId: U32, cameraId: LionessSw.CameraId
        ) severity activity high id 0 format "Capture {} started on camera {}"

        @ A capture completed successfully.
        event CaptureComplete(
            requestId: U32, frameCounter: U32
        ) severity activity high id 1 format "Capture {} done, frame {}"

        @ A capture timed out with no frame.
        event CaptureTimeout(
            requestId: U32
        ) severity warning high id 2 format "Capture {} timed out"

        @ A capture failed.
        event CaptureFailed(
            requestId: U32, status: LionessSw.CaptureStatus
        ) severity warning high id 3 format "Capture {} failed, status {}"

        @ A submission arrived while a job was outstanding.
        event CaptureBusy(
            requestId: U32
        ) severity warning low id 4 format "Capture {} busy"

        @ A submission failed validation.
        event CaptureInvalid(
            requestId: U32
        ) severity warning low id 5 format "Capture {} invalid"

        @ A buffer was returned that this component does not own.
        event UnknownBufferReturned(
        ) severity warning low id 6 format "Unknown buffer returned to Camera"

        @ Captures completed successfully.
        telemetry CapturesTaken: U32

        @ Captures failed or timed out.
        telemetry CapturesFailed: U32

        @ Submissions rejected as busy.
        telemetry CapturesBusy: U32

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
