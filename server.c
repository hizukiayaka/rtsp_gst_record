#include "common.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <gst/gst.h>
#include <glib.h>
#include "configparse.h"

static gboolean bus_call(GstBus * bus, GstMessage * msg, gpointer data);
static void on_pad_added(GstElement * element, GstPad * pad, gpointer data);
static gboolean update_record_dest(gpointer data);
static gchar *create_path(gchar *format, gchar *name, gchar *location);

static gchar *create_path(gchar *format, gchar *name, gchar *location){
	gchar *dst = NULL, *sub_dst = NULL;
	GDateTime *date = g_date_time_new_now_local();
	gchar *directory =  g_date_time_format(date, format);
	sub_dst = g_strdup_printf("%s/%s.mkv", directory, name);

	g_mkdir_with_parents(directory, 0755);
	/*
	g_snprintf(dst, 64, "%s", sub_dst);
	*/
	dst = g_strjoin(location, sub_dst, NULL);
	g_free(directory);
	g_free(sub_dst);

	return dst;
}

static gboolean update_record_dest(gpointer data){
	struct record_config *priv = (struct record_config *)data;
	GstEvent *event;

	event = gst_event_new_eos();
	priv->r.seqnum = gst_event_get_seqnum(event);
	GST_ERROR ("my eos seqnum %u", priv->r.seqnum);
	GST_ERROR ("going to send XXX");
	gst_element_send_event(priv->r.pipeline, event);


	return TRUE;
}

gpointer record(gpointer data){
	gchar *dst;
	GstElement *source, *session, *depay, *parse, *enmux;
	GstBus *bus;

	struct record_config *priv = (struct record_config *)data;

	dst = create_path(priv->dst_format, priv->name, priv->location);

	/* Create gstreamer elements */
	priv->r.pipeline = (void *)gst_pipeline_new("rtsp-record");

	source = gst_element_factory_make("rtspsrc", "rtsp-source");
	session =
	    gst_element_factory_make("rtpjitterbuffer", "session-manger");
	depay =
	    gst_element_factory_make("rtph264depay", "h264-depayloader");
	parse = gst_element_factory_make("h264parse", "video-parse");
	enmux = gst_element_factory_make("matroskamux", "mux");
	priv->r.sink = gst_element_factory_make("filesink", "save-disk");

	if (!priv->r.pipeline || !source || !session || !depay || !parse || !enmux
	    || !priv->r.sink) {
		g_printerr("One element could not be created. Exiting.\n");
		return NULL;
	}

	/* Set up the pipeline */

	/* we set the input filename to the source element */

	g_object_set(G_OBJECT(source), "location", priv->src, NULL);

	g_object_set(G_OBJECT(priv->r.sink), "location", dst, NULL);
	g_free(dst);

	bus = gst_pipeline_get_bus(GST_PIPELINE(priv->r.pipeline));
	priv->r.bus_watch_id = gst_bus_add_watch(bus, bus_call, data);
	gst_object_unref(bus);

	/* we add all elements into the pipeline */
	gst_bin_add_many(GST_BIN(priv->r.pipeline),
			 source, session, depay, parse, enmux, priv->r.sink, NULL);

	/* we link the elements together */
	gst_element_link(source, session);
	gst_element_link(session, depay);
	gst_element_link_many(depay, parse, enmux, priv->r.sink, NULL);
	g_signal_connect(source, "pad-added", G_CALLBACK(on_pad_added),
			 session);

	g_timeout_add_seconds(priv->period * 60, update_record_dest, data);
	gst_element_set_state(priv->r.pipeline, GST_STATE_PLAYING);
	priv->r.status = 1;

	return NULL;
}

static gboolean bus_call(GstBus * bus, GstMessage * msg, gpointer data)
{
	struct record_config *priv = (struct record_config *)data;

	gchar *debug;
	GError *error;
	gchar *dst = NULL;

	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_EOS:
		GST_ERROR ("my eos seqnum msg %u",gst_message_get_seqnum(msg));
		if (gst_message_get_seqnum(msg) == priv->r.seqnum){
			gst_element_set_state((GstElement *)priv->r.pipeline, GST_STATE_NULL);
			dst = create_path(priv->dst_format, priv->name, priv->location);
			g_object_set(G_OBJECT(priv->r.sink), "location", dst, NULL);
			g_free(dst);
			gst_element_set_state((GstElement *)priv->r.pipeline, GST_STATE_PLAYING);
		}
		else{
#if 0
			priv->r.status = 0;
			gst_element_set_state((GstElement *)priv->r.pipeline, GST_STATE_NULL);

			gst_object_unref(priv->r.pipeline);
#else
			PDEBUG("unmatch seq \n");

#endif
		}
		break;

	case GST_MESSAGE_ERROR:
		gst_message_parse_error(msg, &error, &debug);
		g_free(debug);
		
		if(!priv->r.err_msg){
			priv->r.err_msg = g_malloc(100 * sizeof(gchar));
			if (!priv->r.err_msg){
				priv->r.status = -2;
				break;
			}
		}

		g_strlcpy(priv->r.err_msg, error->message, ERR_MSG_SIZE);

		g_printerr("Error: %s\n", error->message);
		g_error_free(error);

		priv->r.status = -1;
		gst_element_set_state(priv->r.pipeline, GST_STATE_NULL);
		break;
		
	default:
		break;
	}

	return TRUE;
}

static void on_pad_added(GstElement * element, GstPad * pad, gpointer data)
{
	GstPad *sinkpad;
	GstElement *session = (GstElement *) data;

	sinkpad = gst_element_get_static_pad(session, "sink");

	gst_pad_link(pad, sinkpad);

	gst_object_unref(sinkpad);
}

static gpointer clean_up_recorder(gpointer data){
	struct record_config *priv = (struct record_config *)data;

	gst_element_set_state(priv->r.pipeline, GST_STATE_NULL);

	gst_object_unref(GST_OBJECT(priv->r.pipeline));

	g_source_remove(priv->r.bus_watch_id);

	g_free(priv->r.err_msg);
	g_free(data);
	return NULL;
}

inline static gint traverse_seq
(GSequence *seq, GThreadFunc func, gpointer loop){
	GSequenceIter *begin, *current, *next;
	gint traversed = 0;
	struct record_config *data;

	begin = g_sequence_get_begin_iter(seq);
	current = begin;
	next = current;

	while(!g_sequence_iter_is_end(next)){
		current = next;

		data = (struct record_config *)g_sequence_get(current);
		/*
		g_print("tr name is %s\n", data->name);
		g_print("tr src is %s\n", data->src);
		*/
		data->r.loop = loop;
		func(data);
		traversed++;

		next = g_sequence_iter_next(current);
	}
	return traversed;
}

int main(int argc, char *argv[])
{
	GMainLoop *loop;
	struct config_head *config ;
	/* Initialisation */
	gst_init(&argc, &argv);

#if 0
	if (-1 == daemon(1,0)){
		g_printerr("can't detach\n");
		exit(EXIT_FAILURE);
	}
#endif

	loop = g_main_loop_new(NULL, FALSE);

	/* config file */

	config = config_init(NULL);

	traverse_seq(config->record_list, record, loop);

	/* Iterate */
	g_main_loop_run(loop);


	/* Out of the main loop, clean up nicely */
	traverse_seq(config->record_list, clean_up_recorder, NULL);
	g_sequence_free(config->record_list);
	g_free(config->general);
	g_free(config);
	g_main_loop_unref(loop);

	return 0;
}
