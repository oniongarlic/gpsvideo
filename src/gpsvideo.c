#include "config.h"

#include <string.h>
#include <math.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gst/gst.h>

#include <geoclue/geoclue-position.h>

typedef struct _GeoImagePipe GeoImagePipe;
struct _GeoImagePipe {
	GstElement *pipe;
	GstElement *src;
	GstElement *queue;
	GstElement *capsfilter;
	GstElement *imageenc;
	GstElement *metadata;
	GstElement *tee;
	GstElement *tee_queue_1;
	GstElement *tee_queue_2;
	GstElement *videorate;
	GstElement *filesink;

	GstElement *textoverlay;
	GstElement *videosink;
};

GeoImagePipe gis;
GMainLoop *loop;

static void
geo_position_changed (GeocluePosition *position, GeocluePositionFields fields, int timestamp,
	double latitude, double longitude, double altitude, GeoclueAccuracy *accuracy, gpointer userdata)
{
g_print ("Position changed:\n");
if (fields & GEOCLUE_POSITION_FIELDS_LATITUDE && fields & GEOCLUE_POSITION_FIELDS_LONGITUDE) {
	g_print ("\t%f, %f\n\n", latitude, longitude);
} else {
	g_print ("No valid position\n");
}
}

static gboolean
set_tag(GstElement *gstjpegenc, gpointer data)
{
GeoImagePipe *gs=(GeoImagePipe *)data;
GstTagSetter *e=GST_TAG_SETTER(gs->metadata);
GstTagList *tl;
GstEvent *te;
gdouble lat, lon;
gchar *cmt;

lat=g_random_double_range(-90,90);
lon=g_random_double_range(-180,180);

g_debug("Geo: %f, %f\n", lat, lon);

cmt=g_strdup_printf("Geo: %f, %f", lat, lon);

g_object_set(gis.textoverlay, "text", cmt, NULL);

gst_tag_setter_reset_tags(e);
gst_tag_setter_add_tags(e,
	GST_TAG_MERGE_REPLACE,
	GST_TAG_COMMENT, cmt,
	GST_TAG_GEO_LOCATION_LATITUDE, lat,
	GST_TAG_GEO_LOCATION_LONGITUDE, lon,
	GST_TAG_GEO_LOCATION_NAME, "Testing", NULL);

return TRUE;
}

static gboolean generate_geotag(gpointer data)
{
set_tag(gis.metadata, &gis);

return TRUE;
}

static void
geoimagepipe()
{
gis.pipe=gst_pipeline_new("pipeline");

// Source
gis.src=gst_element_factory_make("v4l2src", "video");
gis.queue=gst_element_factory_make("queue", "queue");

// Filtering/Caps
gis.videorate=gst_element_factory_make("videorate", "videorate");
gis.capsfilter=gst_element_factory_make("capsfilter", "capsfilter");

// Framerate, 1 FPS
GstCaps *cr=gst_caps_from_string ("video/x-raw,framerate=1/1");
g_object_set(gis.capsfilter, "caps", cr, NULL);
gst_caps_unref(cr);

// Encoding
gis.imageenc=gst_element_factory_make("jpegenc", "jpeg");
gis.metadata=gst_element_factory_make("jifmux", "meta");

// Tee
gis.tee=gst_element_factory_make("tee", "tee");
gis.tee_queue_1=gst_element_factory_make("queue", "queue1");
gis.tee_queue_2=gst_element_factory_make("queue", "queue2");

// Overlay
gis.textoverlay=gst_element_factory_make("textoverlay", "textoverlay");

// Sink(s)
gis.filesink=gst_element_factory_make("multifilesink", "filesink");
gis.videosink=gst_element_factory_make("autovideosink", "videosink");

gst_bin_add_many(GST_BIN(gis.pipe), gis.src, gis.queue,
	gis.videorate, gis.capsfilter,
	gis.tee, gis.imageenc, gis.metadata,
	gis.tee_queue_1, gis.tee_queue_2, gis.filesink, gis.textoverlay, gis.videosink, NULL);

gst_element_link_many(gis.src, gis.queue, gis.videorate, gis.capsfilter, gis.tee, NULL);
gst_element_link_many(gis.tee, gis.tee_queue_1, gis.imageenc, gis.metadata, gis.filesink, NULL);
gst_element_link_many(gis.tee, gis.tee_queue_2, gis.textoverlay, gis.videosink, NULL);

// Setup
//g_object_set(gis.src, "num-buffers", 25, NULL);
g_object_set(gis.imageenc, "quality", 65, NULL);
g_object_set(gis.filesink, "location", "gps_%d.jpg", NULL);

g_object_set(gis.textoverlay, "text", "Starting up...", NULL);

set_tag(gis.metadata, &gis);
}

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data)
{
switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_EOS:
		g_print ("End of stream\n");
		g_main_loop_quit (loop);
	break;
	case GST_MESSAGE_WARNING:
	case GST_MESSAGE_ERROR: {
		gchar  *debug;
		GError *error;

		gst_message_parse_error (msg, &error, &debug);
		g_free (debug);

		g_printerr("Error: %s\n", error->message);
		g_error_free(error);
		g_main_loop_quit(loop);
	}
	break;
	default:
		g_print("Unhandled message %d\n", GST_MESSAGE_TYPE (msg));
	break;
}

return TRUE;
}

gboolean on_sigint(gpointer data)
{

g_return_val_if_fail(data, FALSE);

g_print ("SIGINT\n");

GeoImagePipe *g=(GeoImagePipe *)data;

gst_element_send_event(g->pipe, gst_event_new_eos());

g_main_loop_quit(loop);

return FALSE;
}

gint
main(gint argc, gchar **argv)
{
GeocluePosition *pos;
GstBus *bus;
int bus_watch_id;

g_type_init();
gst_init(&argc, &argv);

geoimagepipe();

bus = gst_pipeline_get_bus(GST_PIPELINE(gis.pipe));
bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
gst_object_unref(bus);

g_timeout_add(500, generate_geotag, NULL);

g_unix_signal_add(SIGINT, on_sigint, &gis.pipe);

pos=geoclue_position_new("org.freedesktop.Geoclue.Providers.Gypsy", "/org/freedesktop/Geoclue/Providers/Gypsy");

g_signal_connect(G_OBJECT (pos), "position-changed",G_CALLBACK (geo_position_changed), NULL);

gst_element_set_state(gis.pipe, GST_STATE_PLAYING);

loop=g_main_loop_new(NULL, TRUE);
//g_timeout_add(5000, g_main_loop_quit, loop);

g_main_loop_run(loop);
g_main_loop_unref(loop);

gst_object_unref(bus);
gst_element_set_state(gis.pipe, GST_STATE_NULL);
gst_object_unref(gis.pipe);

return 0;
}
