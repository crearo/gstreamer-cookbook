#include <string.h>
#include <gst/gst.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*

	GST_DEBUG=3 \
	gst-launch-1.0 videotestsrc pattern=ball background-color=0x80808080 \
	num-buffers=100 "video/x-raw,framerate=5/1" ! \
	tee name=t \
	t. ! queue ! x264enc tune=zerolatency ! matroskamux ! filesink location=264.mkv \
	t. ! autovideosink

See also:
	https://gstreamer.freedesktop.org/documentation/tutorials/basic/dynamic-pipelines.html?gi-language=c
	https://coaxion.net/blog/2014/01/gstreamer-dynamic-pipelines/
	https://github.com/sdroege/gst-snippets/blob/217ae015aaddfe3f7aa66ffc936ce93401fca04e/dynamic-filter.c
	https://gstreamer.freedesktop.org/documentation/x264/index.html?gi-language=c#x264enc-page
*/

static GMainLoop *loop;
static GstElement *pipeline, *src, *tee, *encoder, *muxer, *filesink, *videoconvert, *videosink, *queue_record, *queue_display;
static GstBus *bus;
static GstPad *teepad;
static gboolean recording = FALSE;
static gint counter = 0;
static char *file_path;

static gboolean
message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
	GError *err = NULL;
	gchar *name, *debug = NULL;

	switch (GST_MESSAGE_TYPE (message)) {
		case GST_MESSAGE_ERROR:

			name = gst_object_get_path_string (message->src);
			gst_message_parse_error (message, &err, &debug);

			g_printerr ("ERROR: from element %s: %s\n", name, err->message);
			if (debug != NULL)
				g_printerr ("Additional debug info:\n%s\n", debug);

			g_error_free (err);
			g_free (debug);
			g_free (name);

			g_main_loop_quit (loop);
			break;
		case GST_MESSAGE_WARNING:
			name = gst_object_get_path_string (message->src);
			gst_message_parse_warning (message, &err, &debug);

			g_printerr ("ERROR: from element %s: %s\n", name, err->message);
			if (debug != NULL)
				g_printerr ("Additional debug info:\n%s\n", debug);

			g_error_free (err);
			g_free (debug);
			g_free (name);
			break;
		case GST_MESSAGE_EOS:
			g_print ("Got EOS\n");
			g_main_loop_quit (loop);
			gst_element_set_state (pipeline, GST_STATE_NULL);
			g_main_loop_unref (loop);
			gst_object_unref (pipeline);
			exit(0);
			break;
		default:
			break;
	}

	return TRUE;
}

static GstPadProbeReturn unlink_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
	g_print("Unlinking...");
	GstPad *sinkpad;
	sinkpad = gst_element_get_static_pad (queue_record, "sink");
	gst_pad_unlink (teepad, sinkpad);
	gst_object_unref (sinkpad);

	gst_element_send_event(encoder, gst_event_new_eos()); 

	sleep(1);
	gst_bin_remove(GST_BIN (pipeline), queue_record);
	gst_bin_remove(GST_BIN (pipeline), encoder);
	gst_bin_remove(GST_BIN (pipeline), muxer);
	gst_bin_remove(GST_BIN (pipeline), filesink);

	gst_element_set_state(queue_record, GST_STATE_NULL);
	gst_element_set_state(encoder, GST_STATE_NULL);
	gst_element_set_state(muxer, GST_STATE_NULL);
	gst_element_set_state(filesink, GST_STATE_NULL);

	gst_object_unref(queue_record);
	gst_object_unref(encoder);
	gst_object_unref(muxer);
	gst_object_unref(filesink);

	gst_element_release_request_pad (tee, teepad);
	gst_object_unref (teepad);

	g_print("Unlinked\n");

	return GST_PAD_PROBE_REMOVE;
}

void stopRecording() {
	g_print("stopRecording\n");
	gst_pad_add_probe(teepad, GST_PAD_PROBE_TYPE_IDLE, unlink_cb, NULL, (GDestroyNotify) g_free);
	recording = FALSE;
}

void startRecording() {
	g_print("startRecording\n");
	GstPad *sinkpad;
	GstPadTemplate *templ;

	templ = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(tee), "src_%u");
	teepad = gst_element_request_pad(tee, templ, NULL, NULL);
	queue_record = gst_element_factory_make("queue", "queue_record");
	encoder = gst_element_factory_make("x264enc", NULL);
	muxer = gst_element_factory_make("mp4mux", NULL);
	filesink = gst_element_factory_make("filesink", NULL);
	char *file_name = (char*) malloc(255 * sizeof(char));
	sprintf(file_name, "%s%d.mp4", file_path, counter++);
	g_print("Recording to file %s", file_name);
	g_object_set(filesink, "location", file_name, NULL);
	g_object_set(encoder, "tune", 4, NULL); // zerolatency
	free(file_name);

	gst_bin_add_many(GST_BIN(pipeline), gst_object_ref(queue_record), gst_object_ref(encoder), gst_object_ref(muxer), gst_object_ref(filesink), NULL);
	gst_element_link_many(queue_record, encoder, muxer, filesink, NULL);

	gst_element_sync_state_with_parent(queue_record);
	gst_element_sync_state_with_parent(encoder);
	gst_element_sync_state_with_parent(muxer);
	gst_element_sync_state_with_parent(filesink);

	sinkpad = gst_element_get_static_pad(queue_record, "sink");
	gst_pad_link(teepad, sinkpad);
	gst_object_unref(sinkpad);

	recording = TRUE;
}

void sigintHandler(int unused) {
	g_print("You ctrl-c!\n");
	if (recording)
		stopRecording();
	else
		startRecording();
}

int main(int argc, char *argv[])
{

	if (argc != 2) {
		g_printerr("Enter commandline argument folder path to save recorded video to.\nExample : ./a.out /home/xyz/Desktop/\n");
		return -1;
	}

	if (strstr(argv[1],".mp4") != NULL) {
		g_printerr("Please specify folder path only.\nExample : ./a.out /home/xyz/Desktop/\n");
		return -1;
	}

	file_path = (char*) malloc(255 * sizeof(char));
	file_path = argv[1];

	signal(SIGINT, sigintHandler);
	gst_init (&argc, &argv);

	pipeline = gst_pipeline_new(NULL);
	src = gst_element_factory_make("videotestsrc", NULL);
	g_object_set(src, "pattern", 18, NULL); // GST_VIDEO_TEST_SRC_BALL
	g_object_set(src, "background-color", 0x80808080, NULL);
	g_object_set(src, "do-timestamp", TRUE, NULL);

	tee = gst_element_factory_make("tee", NULL);
	queue_display = gst_element_factory_make("queue", "queue_display");
	videoconvert = gst_element_factory_make("videoconvert", NULL);
	videosink = gst_element_factory_make("autovideosink", NULL);

	if (!pipeline || !src || !tee || !videoconvert || !videosink || !queue_display) {
		g_error("Failed to create elements");
		return -1;
	}

	gst_bin_add_many(GST_BIN(pipeline), src, tee, queue_display, videoconvert, videosink, NULL);
	if (!gst_element_link_many(src, tee, NULL) 
		|| !gst_element_link_many(tee, queue_display, videoconvert, videosink, NULL)) {
		g_error("Failed to link elements");
		return -2;
	}

	startRecording();
	loop = g_main_loop_new(NULL, FALSE);

	bus = gst_pipeline_get_bus(GST_PIPELINE (pipeline));
	gst_bus_add_signal_watch(bus);
	g_signal_connect(G_OBJECT(bus), "message", G_CALLBACK(message_cb), NULL);
	gst_object_unref(GST_OBJECT(bus));

	gst_element_set_state(pipeline, GST_STATE_PLAYING);

	g_print("Starting loop\n");
	g_main_loop_run(loop);

	return 0;
}
