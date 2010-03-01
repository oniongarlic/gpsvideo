#include "config.h"

#include <string.h>
#include <math.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gst/gst.h>

GstElement *pipe;
GstElement *src;
GstElement *queue;
GstElement *imageenc;
GstElement *metadata;
GstElement *sink;

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

gint
main(gint argc, gchar **argv)
{
g_type_init();
gst_init(&argc, &argv);

pipe=gst_pipeline_new("pipeline");
src=gst_element_factory_make("v4l2src", "video");
queue=gst_element_factory_make("queue", "queue");
imageenc=gst_element_factory_make("jpegenc", "jpeg");

#ifdef USE_METADATAMUX
metadata=gst_element_factory_make("metadatamux", "meta");
#else
metadata=gst_element_factory_make("jifmux", "meta");
#endif

queue=gst_element_factory_make("queue", "queue");
sink=gst_element_factory_make("multifilesink", "sink");

gst_bin_add_many(GST_BIN(pipe), src, queue, imageenc, metadata, sink, NULL);

g_assert(gst_element_link(src, queue));
g_assert(gst_element_link(queue, imageenc));
g_assert(gst_element_link(imageenc, metadata));
g_assert(gst_element_link(metadata, sink));

g_object_set(imageenc, "quality", 65, NULL);
g_object_set(sink, "location", "out/gps-%05d.jpg", NULL);
g_object_set(sink, "post-messages", TRUE, NULL);

g_signal_connect(imageenc, "frame-encoded", set_tag, metadata);

gst_element_set_state(pipe, GST_STATE_PLAYING);

loop=g_main_loop_new(NULL, TRUE);
g_timeout_add(5000, g_main_loop_quit, loop);

g_main_loop_run(loop);

return 0;
}
