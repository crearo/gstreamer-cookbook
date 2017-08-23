#include <string.h>
#include <gst/gst.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include <cstring>
#include <sys/time.h>
#include <sstream>
#include <cstdlib>


GMainLoop *loop;
GstElement *filebin;
GstElement *pipeline, *file_queue, *matroskamux, *filesink;                      //// matroska mux working
GstBus *bus;
GstPad *mux_sink_pad=NULL;
gulong probeID;

bool end = false;

bool record_vid = false;

void RemoveOldProbe()
{
    GstPad *file_queue_src;
    file_queue_src = gst_element_get_static_pad(file_queue, "src");
    gst_pad_remove_probe(file_queue_src, probeID);
    gst_object_unref(file_queue_src);
    
}

void CleanExitfile()
{
    gst_element_set_state(filebin, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(pipeline), filebin);

}

void CreateNextfile()
{    
    //Create next mux Element
    matroskamux = gst_element_factory_make("matroskamux", "matroskamux");

    //Create next File Sink or fake sink 
    if(record_vid)
    {
        static std::string fileLocPattern = "/home/rakesh/Desktop/recording%s.mkv";
    
        char present_time[20];
        time_t rawtime;
        struct tm * timeinfo;
        time (&rawtime);
        timeinfo = localtime(&rawtime);

        strftime(present_time,sizeof(present_time),"%d-%m-%Y_%H-%M-%S",timeinfo);

        char buff[80];    
        memset(buff, 0, sizeof(buff));
        sprintf(buff, fileLocPattern.c_str(), present_time);

        filesink = gst_element_factory_make("filesink", "filesink");
        g_object_set(G_OBJECT(filesink),
                      "async", false,
                      "location", buff,
                       NULL);
        printf("next_FILEsink attached\n");
    }
    else
    {
        filesink = gst_element_factory_make("fakesink", "fakesink");
        g_object_set(G_OBJECT(filesink),
                      "async", false,
                       NULL); 
         printf("next_FAKEsink attached\n");
    }

    //Create next filebin
    filebin = gst_bin_new("nextfilebin");    
    g_object_set(G_OBJECT(filebin),
                 "message-forward", true,
                  NULL);
    
    gst_bin_add_many(GST_BIN(filebin), matroskamux, filesink, NULL);

    gst_element_link_many(matroskamux, filesink, NULL);
     
    //Add nextfilebin to pipeline
    gst_bin_add(GST_BIN(pipeline), filebin);
    
    //get request pad from mux
    mux_sink_pad = gst_element_get_request_pad(matroskamux, "video_%u");    
       
    //link ghostpad of mux_sink to filebin 
    GstPad *ghostpad = gst_ghost_pad_new("sink", mux_sink_pad);
    gst_element_add_pad(filebin, ghostpad);

    gst_object_unref(GST_OBJECT(mux_sink_pad));    
        
    //Get src pad from encoder
    GstPad *file_queue_src = gst_element_get_static_pad(file_queue, "src");

    //Link encoder to filebin
    if (gst_pad_link(file_queue_src, ghostpad) != GST_PAD_LINK_OK )
    {
        printf("file_queue_src cannot be linked to next mux_sink_pad.\n");
    }
    gst_object_unref(file_queue_src);  

    gst_element_sync_state_with_parent(filebin);
          
}      


static GstPadProbeReturn pad_probe_cb (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
    GstPad *binsinkpad = gst_element_get_static_pad(filebin, "sink");
    gst_pad_unlink(pad, binsinkpad);
    gst_pad_send_event(binsinkpad, gst_event_new_eos());
    gst_object_unref(binsinkpad);

    return GST_PAD_PROBE_OK;
}

static gboolean switch_record(gpointer user_data)
{
        GstPad *file_queue_src;
        file_queue_src = gst_element_get_static_pad(file_queue, "src");
        
        probeID = gst_pad_add_probe (file_queue_src, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                pad_probe_cb, user_data, NULL);
        gst_object_unref(file_queue_src);  

        return TRUE;
}


void ProperExit()           
{  
   	    end = true;
        record_vid = false;
        switch_record(loop);        
        //gst_element_send_event (pipeline, gst_event_new_eos());        
        printf("Recording has %s\n",record_vid ? "Started" : "Stopped");       
        printf("Clean Exited RECORD Gstpipeline\n");       
  
}

void SwitchRecord()           
{ 
		record_vid = !record_vid;
		switch_record(loop);        
        printf("Recording has %s\n", record_vid ? "Started" : "Stopped");
}


static gboolean bus_cb (GstBus *bus, GstMessage *msg, gpointer user_data)
{
    GMainLoop *mloop = (GMainLoop *)user_data;    
    switch (GST_MESSAGE_TYPE (msg)) 
    {
    case GST_MESSAGE_ERROR:
    {
        GError *err = NULL;
        gchar *dbg;

        gst_message_parse_error (msg, &err, &dbg);
        gst_object_default_error (msg->src, err, dbg);
        g_error_free (err);
        g_free (dbg);
              
        g_main_loop_quit (mloop);
        break;
    }
    case GST_EVENT_EOS:
    {   
        printf("EOS message received\n");
        //printf("Stopping GSTpipeline\n");
        //g_main_loop_quit (loop);
        break;
    }

    case GST_MESSAGE_ELEMENT:
    {           
            const GstStructure *struc = gst_message_get_structure (msg);

            if (gst_structure_has_name (struc, "GstBinForwarded"))
            {
                GstMessage *forward_msg = NULL;

                gst_structure_get (struc, "message", GST_TYPE_MESSAGE, &forward_msg, NULL);
                if (GST_MESSAGE_TYPE (forward_msg) == GST_MESSAGE_EOS)
                {
                    g_print ("EOS received from %s\n",
                            GST_OBJECT_NAME (GST_MESSAGE_SRC (forward_msg)));
                
                    CleanExitfile();                
                    CreateNextfile();
                    if(!end)
                        RemoveOldProbe();
                
                }
                gst_message_unref (forward_msg);
            }
            
     }  
     break;      
        
    default:
        break;
    }
    return TRUE;
}

////////////////// dynamic RTSP pipeline

static void on_pad_added (GstElement *element, GstPad *pad, gpointer data)
{
    GstPad *sinkpad;
    GstElement *decoder = (GstElement *) data;
    /* We can now link this pad with the rtsp-decoder sink pad */
    g_print ("Dynamic pad created, linking source/demuxer\n");
    sinkpad = gst_element_get_static_pad (decoder, "sink");
    gst_pad_link (pad, sinkpad);
    gst_object_unref (sinkpad);
}
    
static void cb_new_rtspsrc_pad(GstElement *element,GstPad*pad,gpointer  data)
{
gchar *name;
GstCaps * p_caps;
gchar * description;
GstElement *p_rtph264depay;

name = gst_pad_get_name(pad);
g_print("A new pad %s was created\n", name);

// here, you would setup a new pad link for the newly created pad
// sooo, now find that rtph264depay is needed and link them?
p_caps = gst_pad_get_pad_template_caps (pad);

description = gst_caps_to_string(p_caps);
//printf("%s\n",p_caps,", ",description,"\n");
g_free(description);

p_rtph264depay = GST_ELEMENT(data);

// try to link the pads then ...
if(!gst_element_link_pads(element, name, p_rtph264depay, "sink"))
{
    printf("Failed to link elements 3\n");
    printf("trying again now.\n");
}
else
    printf("successfull in linking elements 3\n");

g_free(name);
}


void signal_handler(int signal) {
    
    if(signal == 2)
        SwitchRecord();
    else
    {    

        ProperExit();
        usleep(100000);
        exit(0);
    }
}

////////////////////////

int RecordInit_Run()
{
    GstElement *rtspsrc, *capsfilter, *rtph264depay, *h264parse, *tee, *stream_queue, *stream_decoder, *videoconvert, *ximagesink;

    guint unused;
	gst_init (NULL, NULL);
    loop = g_main_loop_new (NULL, FALSE);

    pipeline = gst_pipeline_new ("pipeline");  
	
	rtspsrc = gst_element_factory_make("rtspsrc", "rtspsrc");
	g_object_set(G_OBJECT(rtspsrc), 
					"location", "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov",
					NULL);

    /// CAPSFILTER PART
    capsfilter = gst_element_factory_make("capsfilter", "caps");
    GstCaps *rtspcaps;
    rtspcaps = gst_caps_from_string("application/x-rtp, media=(string)video, payload=(int)96, clock-rate=(int)90000, encoding-name=(string)H264");

    g_object_set(G_OBJECT(capsfilter), "caps", rtspcaps,
                         NULL);
         
    ////////////
    tee = gst_element_factory_make("tee", "tee");
    rtph264depay = gst_element_factory_make("rtph264depay","rtph264depay");
    h264parse = gst_element_factory_make("h264parse","h264parse");
	stream_decoder = gst_element_factory_make("avdec_h264", "h264dec");
	matroskamux = gst_element_factory_make("matroskamux", "matroskamuxer");
	
	videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
	ximagesink = gst_element_factory_make("ximagesink", "displaysink");
	stream_queue = gst_element_factory_make("queue", "stream_queue");
	file_queue = gst_element_factory_make("queue", "file_queue");

	if (!pipeline || !rtspsrc || !tee || !matroskamux || !videoconvert || !ximagesink || !file_queue || !stream_decoder  || !stream_queue) {
		g_error("Failed to create elements");
		return -1;
	}      

    GstPadTemplate *tee_src_pad_template;
    GstPad *stream_pad, *queue_file_pad;
    GstPad *tee_stream_pad, *tee_file_pad;

	if(record_vid)
	{	
            filesink = gst_element_factory_make("filesink", "firstfilesink");  

            static std::string fileLocPattern = "/home/rakesh/Desktop/recording%s.mkv";    
            char present_time[20];
            time_t rawtime;
            struct tm * timeinfo;
            time (&rawtime);
            timeinfo = localtime(&rawtime);

            strftime(present_time,sizeof(present_time),"%d-%m-%Y_%H-%M-%S",timeinfo);

            char buff[80];    
            memset(buff, 0, sizeof(buff));
            sprintf(buff, fileLocPattern.c_str(), present_time); 
            //printf("name%s\n",buff);   
         
            g_object_set(G_OBJECT(filesink),
                "async", false,
		        "location", buff,
                NULL);
            g_print("firstfilesink made \n");
    }
    else
    {
            filesink = gst_element_factory_make("fakesink", "firstfakesink");
            g_object_set(G_OBJECT(filesink),
                      "async", false,
                       NULL); 
            g_print("firstfakesink made \n");
    }

        // Initial file bin generation
        filebin = gst_bin_new("init_file_bin");
        g_object_set(G_OBJECT(filebin),
                     "message-forward", true,
                      NULL);     

        gst_bin_add_many(GST_BIN(filebin), matroskamux, filesink, NULL);
        gst_element_link_many(matroskamux, filesink, NULL);         
       ///// modified for rtsp linking

        gst_bin_add_many (GST_BIN (pipeline), rtspsrc, rtph264depay, NULL);//h264parse, tee, file_queue, filebin, stream_queue, stream_decoder, videoconvert, ximagesink, NULL);
        
        g_signal_connect(rtspsrc, "pad-added", G_CALLBACK(cb_new_rtspsrc_pad),rtph264depay);

        gst_bin_add_many (GST_BIN (pipeline),h264parse,NULL);

        if(!gst_element_link(rtph264depay,h264parse))
            printf("\ndepay and parse not linked\n");


        //if(!gst_element_link_many (rtspsrc, rtph264depay, h264parse,NULL))
          //  printf("error link_many_rtsp\n");

        gst_bin_add_many (GST_BIN (pipeline), tee, file_queue, filebin, stream_queue, stream_decoder, videoconvert, ximagesink, NULL);

        gst_element_link_filtered (h264parse, tee, NULL);      

        if(!gst_element_link_many (stream_queue, stream_decoder, videoconvert, ximagesink, NULL))
            printf("error link many stream_queue\n");
        //gst_element_link_many (file_queue, file_encoder, NULL);
                
        //Manually linking the Tee 
        if ( !(tee_src_pad_template = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (tee), "src_%u"))) 
        {
            gst_object_unref (pipeline);
            g_critical ("Unable to get tee pad template");
            return 0;  
        }

        tee_stream_pad = gst_element_request_pad (tee, tee_src_pad_template, NULL, NULL);
        g_print ("Obtained request pad %s for tee stream branch.\n", gst_pad_get_name (tee_stream_pad));
        stream_pad = gst_element_get_static_pad (stream_queue, "sink");

        tee_file_pad = gst_element_request_pad (tee, tee_src_pad_template, NULL, NULL);
        g_print ("Obtained request pad %s for tee file branch.\n", gst_pad_get_name (tee_file_pad));
        queue_file_pad = gst_element_get_static_pad (file_queue, "sink");

        /* Link the tee to the stream_pad */
        if (gst_pad_link (tee_stream_pad, stream_pad) != GST_PAD_LINK_OK )
        {
           g_critical ("Tee for stream could not be linked.\n");
           gst_object_unref (pipeline);
            return 0;
        }
 
        /* Link the tee to the file_queue  */
        if (gst_pad_link (tee_file_pad, queue_file_pad) != GST_PAD_LINK_OK) 
        {
          g_critical ("Tee for file could not be linked.\n");
          gst_object_unref (pipeline);
          return 0;
        }
    
        gst_object_unref (stream_pad);
        gst_object_unref (queue_file_pad);             
             

        //get request pad from mux       
        GstPadTemplate * mux_sink_padTemplate;       
        if( !(mux_sink_padTemplate = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(matroskamux), "video_%u")) )
        {
            printf("Unable to get source pad template from muxing element\n");
        }

        //Obtain request pad from mux
        mux_sink_pad = gst_element_request_pad(matroskamux, mux_sink_padTemplate, NULL, NULL);  

        if(mux_sink_pad ==NULL)
            printf("error mux_sink_pad\n");
         
        //Add a copy of mux sink pad to bin
        GstPad *ghostpad = gst_ghost_pad_new("sink", mux_sink_pad);

        if(!gst_element_add_pad(filebin, ghostpad))
            printf("error ghost pad could not be added to bin\n");

        gst_object_unref(GST_OBJECT(mux_sink_pad));
        
        
        //Get src pad from queue element
        GstPad *file_queue_src = gst_element_get_static_pad(file_queue, "src");

        //Link to ghostpad
        if (gst_pad_link(file_queue_src, ghostpad) != GST_PAD_LINK_OK )
        {

            printf("file_queue_src cannot be linked to mux_sink_pad.\n");
        }

        g_signal_connect(rtph264depay, "pad-added", G_CALLBACK(on_pad_added), h264parse);


    gst_element_sync_state_with_parent(filebin);
    //gst_pad_set_active(ghostpad,true);

    gst_bus_add_watch (GST_ELEMENT_BUS (pipeline), bus_cb, loop);
    
    struct sigaction act;
    act.sa_handler = signal_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
              
    gst_element_set_state (pipeline, GST_STATE_PLAYING);
 
    sigaction(SIGINT, &act, 0); 
    sigaction(SIGTSTP, &act, 0);       
    sigaction(SIGABRT, &act, 0);
    sigaction(SIGQUIT, &act, 0);
    
    g_main_loop_run (loop);    
           
    return 1;
                      
} 


int main(int argc, char *argv[])
{    
	RecordInit_Run();
	
	return 0;
}
