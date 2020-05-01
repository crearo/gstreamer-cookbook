#include <string.h>
#include <gst/gst.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

// v4l2src ! tee name=t t. ! x264enc ! mp4mux ! filesink location=/home/rish/Desktop/okay.264 t. ! videoconvert ! autovideosink

static GMainLoop *loop;
static GstElement *pipeline, *src, *tee, *encoder, *muxer, *filesink, *videoconvert, *videosink, *queue_record, *queue_display;
static GstBus *bus;

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
			g_print("Saved video file\n");
			exit(0);
			break;

		default:
			break;
	}

	return TRUE;
}

void sigintHandler(int unused)
{
	g_print("Sending EoS!\n");
	gst_element_send_event(pipeline, gst_event_new_eos()); 
}

int main(int argc, char *argv[])
{

	if (argc != 2) {
		g_printerr("Enter commandline argument file-path to save recorded video to.\nExample : ./a.out /home/xyz/Desktop/recorded.mp4\n");
		return -1;
	}

	signal(SIGINT, sigintHandler);
	gst_init (&argc, &argv);

	pipeline = gst_pipeline_new(NULL);
	src = gst_element_factory_make("v4l2src", NULL);
	tee = gst_element_factory_make("tee", "tee");
	encoder = gst_element_factory_make("x264enc", NULL);
	muxer = gst_element_factory_make("mp4mux", NULL);
	filesink = gst_element_factory_make("filesink", NULL);
	videoconvert = gst_element_factory_make("videoconvert", NULL);
	videosink = gst_element_factory_make("autovideosink", NULL);
	queue_display = gst_element_factory_make("queue", "queue_display");
	queue_record = gst_element_factory_make("queue", "queue_record");

	if (!pipeline || !src || !tee || !encoder || !muxer || !filesink || !videoconvert || !videosink || !queue_record || !queue_display) {
		g_error("Failed to create elements");
		return -1;
	}

	g_object_set(filesink, "location", argv[1], NULL);
	g_object_set(encoder, "tune", 4, NULL); /* important, the encoder usually takes 1-3 seconds to process this. Queue buffer is generally upto 1 second. Hence, set tune=zerolatency (0x4) */

	gst_bin_add_many(GST_BIN(pipeline), src, tee, queue_record, encoder, muxer, filesink, queue_display, videoconvert, videosink, NULL);
	if (!gst_element_link_many(src, tee, NULL) 
		|| !gst_element_link_many(tee, queue_record, encoder, muxer, filesink, NULL)
		|| !gst_element_link_many(tee, queue_display, videoconvert, videosink, NULL)) {
		g_error("Failed to link elements");
		return -2;
	}

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
