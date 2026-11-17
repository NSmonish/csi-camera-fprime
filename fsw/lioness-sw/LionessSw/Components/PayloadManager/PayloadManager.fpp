module Components {

    @ PayloadManager component. Owns payload imaging: it asks
    @ the Camera component for a picture, receives the processed
    @ JPEG preview, PNG preview and lossless product, stores
    @ them through the storage stub, and returns the
    @ ImageProcessing buffers. The PNG is a viewable preview,
    @ not the lossless product.
    @ One active component. It never blocks on capture or
    @ encoding: it submits through a sync port that returns at
    @ once, and the result arrives later on the async
    @ captureDone port. Downlink itself is out of scope; the
    @ storage stub only writes files on the host.
    active component PayloadManager {

        @ Take a payload picture. The command is accepted at
        @ once when the camera is free; the stored product is
        @ reported later by event and telemetry.
        async command TakePayloadPicture(
            width: U32,
            height: U32,
            operation: LionessSw.CaptureOperation
        )

        @ Completion from the Camera component.
        async input port captureDone: LionessSw.CaptureDone

        @ Submit a capture request to the Camera component.
        output port captureSubmit: LionessSw.CaptureSubmit

        @ Return a processed buffer to ImageProcessing.
        output port returnOutput: LionessSw.ReturnBuffer

        @ A picture was stored successfully.
        event PictureStored(
            requestId: U32, previewBytes: U32, losslessBytes: U32,
            pngBytes: U32
        ) severity activity high id 0 format "Payload picture {} stored, preview {} bytes, lossless {} bytes, png {} bytes"

        @ A picture failed at capture or processing.
        event PictureFailed(
            requestId: U32, status: LionessSw.CaptureStatus
        ) severity warning high id 1 format "Payload picture {} failed, status {}"

        @ A picture could not be written to storage.
        event StorageFailed(
            requestId: U32
        ) severity warning high id 2 format "Payload picture {} could not be stored"

        @ The camera was busy when a picture was requested.
        event CameraBusy(
            requestId: U32
        ) severity warning low id 3 format "Payload picture {} camera busy"

        @ The picture request failed validation.
        event CameraInvalid(
            requestId: U32
        ) severity warning low id 4 format "Payload picture {} invalid"

        @ Pictures stored successfully.
        telemetry PicturesTaken: U32

        @ Pictures failed.
        telemetry PicturesFailed: U32

        @ Total product bytes written to storage.
        telemetry BytesStored: U64

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
