## GStreamer Cookbook

The GStreamer API is difficult to work with. Remember, data in GStreamer flows through pipelines quite analogous to the way water flows through pipes.

This repository is a collection of C snippets using the GStreamer 1.0 API to perform basic video operations. 

## Capturing & Recording
- The webcam (v4l2src) as the input stream
- Autovideosink is used to display. In these examples, autovideosink is preceded by videoconvert.
- Recording is H.264 encoded using the x264enc element.
- Mux the recording with mp4mux to make the output an mp4 file.

#### Let's display a video
`gst-launch-1.0 v4l2src ! videoconvert ! autovideosink`

The most basic pipeline to display video from webcam to a window on the screen.


#### Now let's record a ideo
`gst-launch-1.0 v4l2src ! x264enc ! mp4mux ! filesink location=/home/xyz/Desktop/recorded.mp4 -e`

A basic pipeline to record video from webcam to a file on specified location. The `-e` tage instructs GStreamer to flush EoS(End of Stream) before closing the recorded stream. This allows proper closing of the saved file. 

#### Display Recorded Video
`gst-launch-1.0 filesrc location=/home/xyz/Desktop/recorded.mp4 ! decodebin ! videoconvert ! autovideosink`

A pipeline to display the file saved using the above pipeline. This uses the element `decodebin` which is a super-powerful all-knowing decoder. It decodes any stream of a legal format to the format required by the next element in the pipeline. 
