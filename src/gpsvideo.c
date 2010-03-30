#include "config.h"

#include <string.h>
#include <math.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gst/gst.h>

typedef struct _VideoSrc VideoSrc;
struct _VideoSrc {
	GstElement *pipe;
	GstElement *src;
	GstElement *queue;
	GstElement *tee;
	GstElement *xv;
	GstElement *sink;
};

typedef struct _GeoImageSink GeoImageSink;
struct _GeoImageSink {
	GstElement *pipe;
	GstElement *src;
	GstElement *queue;
	GstElement *imageenc;
	GstElement *metadata;
	GstElement *sink;
};

VideoSrc vsrc;
GeoImageSink gis;

GMainLoop *loop;

#define USE_TAG_SETTER 1
#define USE_METADATAMUX 0

static gboolean
set_tag(GstElement *gstjpegenc, gpointer data)
{
GeoImageSink *gs=(GeoImageSink *)data;
GstTagSetter *e=GST_TAG_SETTER(gs->metadata);
GstTagList *tl;
GstEvent *te;
gdouble lat, lon;
gchar *cmt;

lat=g_random_double_range(-90,90);
lon=g_random_double_range(-180,180);

g_debug("Geo: %f, %f", lat, lon);

cmt=g_strdup_printf("Geo: %f, %f", lat, lon);

#ifdef USE_TAG_SETTER
gst_tag_setter_reset_tags(e);
gst_tag_setter_add_tags(e,
	GST_TAG_MERGE_REPLACE, 
	GST_TAG_COMMENT, cmt, 
	GST_TAG_GEO_LOCATION_LATITUDE, lat,
	GST_TAG_GEO_LOCATION_LONGITUDE, lon, 
	GST_TAG_GEO_LOCATION_NAME, "Testing", NULL);
#else
tl=gst_tag_list_new_full(GST_TAG_GEO_LOCATION_LATITUDE, lat,
	GST_TAG_GEO_LOCATION_LONGITUDE, lon, 
	GST_TAG_GEO_LOCATION_NAME, "Testing",
	GST_TAG_COMMENT, cmt, 
	NULL);
te=gst_event_new_tag(tl);
gst_element_send_event(gs->imageenc, te);
#endif

gst_element_send_event(gs->pipe, gst_event_new_new_segment(FALSE, 1.0, GST_FORMAT_BYTES, 0, -1, 0));

return TRUE;
}

static void
videosrc()
{
vsrc.pipe=gst_pipeline_new("pipeline");
vsrc.src=gst_element_factory_make("v4l2src", "video");
vsrc.queue=gst_element_factory_make("queue", "queue");
vsrc.tee=gst_element_factory_make("tee", "tee");
vsrc.xv=gst_element_factory_make("xvimagesink", "xv");
vsrc.sink=gst_element_factory_make("fakesink", "fakesink");

gst_bin_add_many(GST_BIN(vsrc.pipe), vsrc.src, vsrc.queue, vsrc.tee, vsrc.xv, vsrc.sink, NULL);

g_assert(gst_element_link(vsrc.src, vsrc.queue));
g_assert(gst_element_link(vsrc.queue, vsrc.tee));
g_assert(gst_element_link(vsrc.tee, vsrc.sink));
g_assert(gst_element_link(vsrc.tee, vsrc.xv));
}

static void
imagesink()
{
gis.pipe=gst_pipeline_new("pipeline");
gis.imageenc=gst_element_factory_make("jpegenc", "jpeg");

#ifdef USE_METADATAMUX
gis.metadata=gst_element_factory_make("metadatamux", "meta");
#else
gis.metadata=gst_element_factory_make("jifmux", "meta");
#endif

gis.queue=gst_element_factory_make("queue", "queue");
gis.sink=gst_element_factory_make("filesink", "sink");

gst_bin_add_many(GST_BIN(gis.pipe), gis.src, gis.queue, gis.imageenc, gis.metadata, gis.sink, NULL);

g_assert(gst_element_link(gis.src, gis.queue));
g_assert(gst_element_link(gis.queue, gis.imageenc));
g_assert(gst_element_link(gis.imageenc, gis.metadata));
g_assert(gst_element_link(gis.metadata, gis.sink));

g_object_set(gis.imageenc, "quality", 65, NULL);
g_object_set(gis.sink, "location", "gps.jpg", NULL);
g_signal_connect(gis.imageenc, "frame-encoded", set_tag, &gis);
}

gint
main(gint argc, gchar **argv)
{
g_type_init();
gst_init(&argc, &argv);

videosrc();
imagesink();

gst_element_set_state(vsrc.pipe, GST_STATE_PLAYING);

loop=g_main_loop_new(NULL, TRUE);
g_timeout_add(5000, g_main_loop_quit, loop);

g_main_loop_run(loop);

return 0;
}
