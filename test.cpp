#include <gst/gst.h>
#include <gst/video/video.h>
#include <string.h>
#include <opencv2/opencv.hpp> // Thêm OpenCV nếu cần xử lý ảnh

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData {
    GstElement *pipeline, *video_source, *tee, *video_queue, *video_convert, *video_sink;
    GstElement *app_queue, *app_convert, *app_sink;

    GMainLoop *main_loop;  /* GLib's Main Loop */
} CustomData;

/* Callback to handle new pads from uridecodebin */
static void on_pad_added(GstElement *src, GstPad *new_pad, CustomData *data) {
    GstPad *sink_pad = gst_element_get_static_pad(data->tee, "sink");
    if (gst_pad_is_linked(sink_pad)) {
        g_print("Pad already linked, ignoring.\n");
        gst_object_unref(sink_pad);
        return;
    }

    /* Check the pad type (only link video pads) */
    GstCaps *new_pad_caps = gst_pad_get_current_caps(new_pad);
    if (!new_pad_caps) {
        g_print("Failed to get pad caps.\n");
        return;
    }

    GstStructure *new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    const gchar *new_pad_type = gst_structure_get_name(new_pad_struct);
    if (!g_str_has_prefix(new_pad_type, "video/x-raw")) {
        g_print("Ignoring non-video pad: %s\n", new_pad_type);
        gst_object_unref(sink_pad);
        gst_caps_unref(new_pad_caps);
        return;
    }

    /* Link the pad */
    GstPadLinkReturn ret = gst_pad_link(new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED(ret)) {
        g_print("Failed to link pads!\n");
    } else {
        g_print("Linked pad: %s\n", gst_pad_get_name(new_pad));
    }

    gst_object_unref(sink_pad);
    gst_caps_unref(new_pad_caps);
}

/* The appsink has received a buffer */
static GstFlowReturn new_sample(GstElement *sink, CustomData *data) {
    GstSample *sample;
    GstMapInfo info;

    /* Retrieve the buffer */
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (!sample) {
        return GST_FLOW_ERROR;
    }

    /* Get buffer and caps */
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstCaps *caps = gst_sample_get_caps(sample);

    if (!buffer || !caps) {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    /* Get frame information */
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    gint width, height;
    const gchar *format;
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);
    format = gst_structure_get_string(structure, "format");

    g_print("Frame received: Width=%d, Height=%d, Format=%s\n", width, height, format);

    /* Map buffer to access data */
    if (gst_buffer_map(buffer, &info, GST_MAP_READ)) {
        g_print("Buffer content (first 16 bytes): ");
        for (gsize i = 0; i < MIN(info.size, 16); i++) {
            g_print("%02X ", info.data[i]);
        }
        g_print("\n");

        // ---- TẠI ĐÂY BẠN CÓ THỂ XỬ LÝ HÌNH ẢNH ----
        // if (g_str_equal(format, "RGB")) {
        //     cv::Mat rgb_frame(height, width, CV_8UC3, (void*)info.data);
        //     // Xử lý thêm với rgb_frame
        // }

        gst_buffer_unmap(buffer, &info);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

/* This function is called when an error message is posted on the bus */
static void error_cb(GstBus *bus, GstMessage *msg, CustomData *data) {
    GError *err;
    gchar *debug_info;

    /* Print error details on the screen */
    gst_message_parse_error(msg, &err, &debug_info);
    g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
    g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error(&err);
    g_free(debug_info);

    g_main_loop_quit(data->main_loop);
}

int main(int argc, char *argv[]) {
    CustomData data;
    GstPad *tee_video_pad, *tee_app_pad;
    GstPad *queue_video_pad, *queue_app_pad;
    GstBus *bus;

    /* Initialize GStreamer */
    gst_init(&argc, &argv);

    /* Create the elements */
    data.video_source = gst_element_factory_make("uridecodebin", "video_source");
    data.tee = gst_element_factory_make("tee", "tee");
    data.video_queue = gst_element_factory_make("queue", "video_queue");
    data.video_convert = gst_element_factory_make("videoconvert", "video_convert");
    data.video_sink = gst_element_factory_make("autovideosink", "video_sink");
    data.app_queue = gst_element_factory_make("queue", "app_queue");
    data.app_convert = gst_element_factory_make("videoconvert", "app_convert");
    data.app_sink = gst_element_factory_make("appsink", "app_sink");

    /* Create the empty pipeline */
    data.pipeline = gst_pipeline_new("test-pipeline");

    if (!data.pipeline || !data.video_source || !data.tee || !data.video_queue ||
        !data.video_convert || !data.video_sink || !data.app_queue || !data.app_convert || !data.app_sink) {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    /* Configure video source */
    g_object_set(data.video_source, "uri", "https://videos.pexels.com/video-files/3764259/3764259-hd_1280_720_60fps.mp4", NULL);
    g_signal_connect(data.video_source, "pad-added", G_CALLBACK(on_pad_added), &data);

    /* Configure appsink */
    GstCaps *sink_caps = gst_caps_new_simple("video/x-raw",
        "format", G_TYPE_STRING, "RGB", // Định dạng RGB
        NULL);
    g_object_set(data.app_sink, "emit-signals", TRUE, "caps", sink_caps, NULL);
    g_signal_connect(data.app_sink, "new-sample", G_CALLBACK(new_sample), &data);
    gst_caps_unref(sink_caps);

    /* Add elements to the pipeline */
    gst_bin_add_many(GST_BIN(data.pipeline), data.video_source, data.tee, data.video_queue,
                     data.video_convert, data.video_sink, data.app_queue, data.app_convert, data.app_sink, NULL);

    /* Link elements */
    if (!gst_element_link_many(data.video_queue, data.video_convert, data.video_sink, NULL) ||
        !gst_element_link_many(data.app_queue, data.app_convert, data.app_sink, NULL)) {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    /* Manually link the Tee */
    tee_video_pad = gst_element_request_pad_simple(data.tee, "src_%u");
    queue_video_pad = gst_element_get_static_pad(data.video_queue, "sink");
    tee_app_pad = gst_element_request_pad_simple(data.tee, "src_%u");
    queue_app_pad = gst_element_get_static_pad(data.app_queue, "sink");

    if (!tee_video_pad || !queue_video_pad || !tee_app_pad || !queue_app_pad) {
        g_printerr("Failed to get pads.\n");
        return -1;
    }

    if (gst_pad_link(tee_video_pad, queue_video_pad) != GST_PAD_LINK_OK ||
        gst_pad_link(tee_app_pad, queue_app_pad) != GST_PAD_LINK_OK) {
        g_printerr("Tee could not be linked\n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    gst_object_unref(queue_video_pad);
    gst_object_unref(queue_app_pad);

    /* Set up the bus */
    bus = gst_element_get_bus(data.pipeline);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)error_cb, &data);
    gst_object_unref(bus);

    /* Start playing the pipeline */
    gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

    /* Create a GLib Main Loop and set it to run */
    data.main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(data.main_loop);

    /* Release resources */
    gst_element_release_request_pad(data.tee, tee_video_pad);
    gst_element_release_request_pad(data.tee, tee_app_pad);
    gst_object_unref(tee_video_pad);
    gst_object_unref(tee_app_pad);

    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
    return 0; 
}
