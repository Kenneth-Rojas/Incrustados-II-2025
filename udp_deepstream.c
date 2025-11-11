#include <gst/gst.h>
#include <glib.h>

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data) {
    GMainLoop *loop = (GMainLoop *) data;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            g_print("End of stream\n");
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_ERROR: {
            gchar *debug;
            GError *error;
            gst_message_parse_error(msg, &error, &debug);
            g_printerr("Error: %s\n", error->message);
            g_free(debug);
            g_error_free(error);
            g_main_loop_quit(loop);
            break;
        }
        default:
            break;
    }
    return TRUE;
}

int main(int argc, char *argv[]) {
    GMainLoop *loop;
    GstElement *pipeline;
    GstBus *bus;
    guint bus_watch_id;

    gst_init(&argc, &argv);
    loop = g_main_loop_new(NULL, FALSE);

    // --- PIPELINE CORREGIDO ---
    const gchar *pipeline_desc =
        "filesrc location=/opt/nvidia/deepstream/deepstream/samples/streams/sample_1080p_h264.mp4 ! "
        "qtdemux ! h264parse ! nvv4l2decoder ! "
        "video/x-raw(memory:NVMM),format=NV12 ! queue ! mux.sink_0 "
        "nvstreammux name=mux width=1920 height=1080 batch-size=1 ! queue ! "
        "nvvideoconvert ! nvinfer "
        "config-file-path=/opt/nvidia/deepstream/deepstream-6.0/samples/configs/deepstream-app/config_infer_primary.txt "
        "model-engine-file=/opt/nvidia/deepstream/deepstream-6.0/samples/models/Primary_Detector/resnet10.caffemodel_b1_gpu0_fp16.engine ! "
        "queue ! nvdsosd process-mode=HW_MODE ! tee name=t "
        // Rama 1: display (ahora fakesink, para evitar cuelgue)
        "t. ! queue ! nvvideoconvert ! fakesink sync=false "
        // Si querés reactivar salida local en GUI, usa esta línea en lugar de la anterior:
        // "t. ! queue ! nvvideoconvert ! nvoverlaysink sync=false "
        // Rama 2: transmisión UDP
        "t. ! queue ! nvvideoconvert ! video/x-raw(memory:NVMM),format=I420 ! "
        "nvv4l2h264enc insert-sps-pps=true bitrate=4000000 ! h264parse ! rtph264pay ! "
        "udpsink host=192.168.10.1 port=5000 sync=false async=false";

    g_print("Creando pipeline...\n");
    pipeline = gst_parse_launch(pipeline_desc, NULL);
    if (!pipeline) {
        g_printerr("Error al crear el pipeline. Saliendo.\n");
        return -1;
    }

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref(bus);

    g_print("Iniciando pipeline DeepStream + UDP...\n");
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    g_main_loop_run(loop);

    g_print("Deteniendo pipeline\n");
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_source_remove(bus_watch_id);
    g_main_loop_unref(loop);

    return 0;
}
