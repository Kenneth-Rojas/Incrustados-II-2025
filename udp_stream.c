#include <gst/gst.h>
#include <glib.h>

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
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
            g_free(debug);

            g_printerr("Error: %s\n", error->message);
            g_error_free(error);

            g_main_loop_quit(loop);
            break;
        }

        default:
            break;
    }

    return TRUE;
}

int main(int argc, char *argv[])
{
    GMainLoop *loop;
    GstElement *pipeline, *source, *conv, *encoder, *pay, *sink;
    GstBus *bus;
    guint bus_watch_id;

    /* Inicialización */
    gst_init(&argc, &argv);
    loop = g_main_loop_new(NULL, FALSE);

    /* Crear elementos */
    pipeline = gst_pipeline_new("udp-stream");
    source   = gst_element_factory_make("v4l2src", "source");           // cámara
    conv     = gst_element_factory_make("videoconvert", "conv");        // conversión de formato
    encoder  = gst_element_factory_make("x264enc", "encoder");           // codificador H264
    pay      = gst_element_factory_make("rtph264pay", "pay");            // empaquetador RTP
    sink     = gst_element_factory_make("udpsink", "sink");              // envío UDP

    if (!pipeline || !source || !conv || !encoder || !pay || !sink) {
        g_printerr("No se pudieron crear los elementos. Saliendo.\n");
        return -1;
    }

    /* Configurar destino UDP */
    g_object_set(G_OBJECT(sink), "host", "192.168.1.50", "port", 5000, NULL); // Cambiar IP y puerto según tu receptor

    /* Añadir bus y watch */
    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref(bus);

    /* Añadir elementos al pipeline */
    gst_bin_add_many(GST_BIN(pipeline), source, conv, encoder, pay, sink, NULL);

    /* Linkear elementos */
    if (!gst_element_link_many(source, conv, encoder, pay, sink, NULL)) {
        g_printerr("No se pudieron linkear los elementos.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    /* Iniciar streaming */
    g_print("Iniciando streaming UDP...\n");
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    /* Loop principal */
    g_main_loop_run(loop);

    /* Cleanup */
    g_print("Deteniendo pipeline\n");
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_source_remove(bus_watch_id);
    g_main_loop_unref(loop);

    return 0;
}
