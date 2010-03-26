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
}

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
GeoImageSink gsink;

GMainLoop *loop;

#define USE_TAG_SETTER 1
#define USE_METADATAMUX 0

static gboolean
set_tag(GstElement *gstjpegenc, gpointer data)
{
GstTagSetter *e=GST_TAG_SETTER(data);
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
gst_element_send_event(imageenc, te);
#endif

gst_element_send_event(pipe, gst_event_new_new_segment(FALSE, 1.0, GST_FORMAT_BYTES, 0, -1, 0));

return TRUE;
}

void videosrc()
{
vsrc.pipe=gst_pipeline_new("pipeline");
vsrc.src=gst_element_factory_make("v4l2src", "video");
vsrc.queue=gst_element_factory_make("queue", "queue");
vsrc.tee=gst_element_factory_make("tee", "tee");
vsrc.xv=gst_element_factory_make("xvimagesink", "xv");
vsrc.sink=gst_element_factory_make("fakesink", "fakesink");

gst_bin_add_many(GST_BIN(pipe), src, queue, imageenc, metadata, sink, NULL);

g_assert(gst_element_link(vsrc.src, vsrc.queue));
g_assert(gst_element_link(vsrc.queue, vsrc.tee));
g_assert(gst_element_link(vsrc.tee, vsrc.sink));
}

void imagesink()
{
gsink.pipe=gst_pipeline_new("pipeline");
gsink.imageenc=gst_element_factory_make("jpegenc", "jpeg");

#ifdef USE_METADATAMUX
gsink.metadata=gst_element_factory_make("metadatamux", "meta");
#else
gsink.metadata=gst_element_factory_make("jifmux", "meta");
#endif

gsink.queue=gst_element_factory_make("queue", "queue");
gsink.sink=gst_element_factory_make("filesink", "sink");

gst_bin_add_many(GST_BIN(pipe), src, queue, imageenc, metadata, sink, NULL);

g_assert(gst_element_link(gsink.src, gsink.queue));
g_assert(gst_element_link(gsink.queue, gsink.imageenc));
g_assert(gst_element_link(gsink.imageenc, gsink.metadata));
g_assert(gst_element_link(gsink.metadata, gsink.sink));

g_object_set(gsink.imageenc, "quality", 65, NULL);
g_object_set(gsink.sink, "location", "gps.jpg", NULL);
g_signal_connect(imageenc, "frame-encoded", set_tag, gsink.metadata);
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
