module Components {
    @ Processes images into star vectors and satellite orientation.
    @ The full LOST star identification algorithm from
    @ test/star-tracker is NOT integrated here yet. What this
    @ component does now is accept a raw frame from the
    @ StarTrackerManager, validate it, record it, and report it
    @ by event and telemetry. That handoff is the delivered
    @ interface; LOST integration is the next step.
    passive component StarTracker {

        @ Receive a raw frame from the StarTrackerManager.
        sync input port processFrame: LionessSw.ProcessFrame

        @ A raw frame was received and recorded.
        event FrameReceived(
            frameCounter: U32, width: U32, height: U32
        ) severity activity high id 0 format "Star frame {} received, {}x{}"

        @ A frame arrived with a size that does not match its
        @ dimensions for RAW10 packed data.
        event FrameInvalid(
            frameCounter: U32, frameBytes: U32
        ) severity warning high id 1 format "Star frame {} invalid, {} bytes"

        @ Raw frames received.
        telemetry FramesReceived: U32

        @ Last frame counter received.
        telemetry LastFrameCounter: U32

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry channels handling
        import Fw.Channel

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut

    }
}