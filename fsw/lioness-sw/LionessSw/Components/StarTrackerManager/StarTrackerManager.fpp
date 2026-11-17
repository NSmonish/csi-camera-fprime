module Components {

    @ StarTrackerManager component. Owns star tracking imaging:
    @ it asks the Camera component for a raw picture from the
    @ star tracker camera, hands the raw frame to the StarTracker
    @ component, and returns the frame buffer to the Camera.
    @ One active component. It never blocks on capture: it
    @ submits through a sync port that returns at once, and the
    @ frame arrives later on the async captureDone port. The
    @ full LOST star identification algorithm is NOT integrated
    @ here; the delivered interface is this manager handing a
    @ raw frame to StarTracker. The star camera format used in
    @ simulation is 1280x720 RAW10, an assumption to confirm
    @ with the team.
    active component StarTrackerManager {

        @ Take a star tracker picture. The command is accepted
        @ at once when the camera is free; the frame is handed
        @ to StarTracker later.
        async command TakeStarPicture(
            width: U32,
            height: U32
        )

        @ Completion from the Camera component.
        async input port captureDone: LionessSw.CaptureDone

        @ Submit a capture request to the Camera component.
        output port captureSubmit: LionessSw.CaptureSubmit

        @ Return a raw frame buffer to the Camera component.
        output port returnFrame: LionessSw.ReturnFrame

        @ Deliver a raw frame to the StarTracker component.
        output port processFrame: LionessSw.ProcessFrame

        @ A star frame was handed to StarTracker.
        event StarFrameDelivered(
            requestId: U32, frameCounter: U32
        ) severity activity high id 0 format "Star frame {} delivered, counter {}"

        @ A star capture failed.
        event StarFrameFailed(
            requestId: U32, status: LionessSw.CaptureStatus
        ) severity warning high id 1 format "Star frame {} failed, status {}"

        @ The camera was busy when a frame was requested.
        event CameraBusy(
            requestId: U32
        ) severity warning low id 2 format "Star frame {} camera busy"

        @ The frame request failed validation.
        event CameraInvalid(
            requestId: U32
        ) severity warning low id 3 format "Star frame {} invalid"

        @ Star frames delivered to StarTracker.
        telemetry StarFramesTaken: U32

        @ Star frames failed.
        telemetry StarFramesFailed: U32

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
