#include <gst/gst.h>
#include <glib.h>

/* --- Función para manejar mensajes del bus --- */
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

/* --- Callback para el pad dinámico del demuxer --- */
static void on_pad_added(GstElement *src, GstPad *pad, gpointer data) {
    GstElement *parser = (GstElement *) data;
    GstPad *sinkpad = gst_element_get_static_pad(parser, "sink");

    if (gst_pad_is_linked(sinkpad)) {
        g_print("Pad del parser ya está linkeado, se omite.\n");
        gst_object_unref(sinkpad);
        return;
    }

    if (gst_pad_link(pad, sinkpad) != GST_PAD_LINK_OK)
        g_printerr("No se pudo linkear demuxer con parser.\n");
    else
        g_print("Pad dinámico linkeado correctamente.\n");

    gst_object_unref(sinkpad);
}

/* --- Función principal --- */
int main(int argc, char *argv[]) {
    GMainLoop *loop;
    GstElement *pipeline, *source, *demuxer, *parser, *pay, *sink;
    GstBus *bus;
    guint bus_watch_id;

    gst_init(&argc, &argv);
    loop = g_main_loop_new(NULL, FALSE);

    /* Crear elementos */
    pipeline = gst_pipeline_new("udp-file-stream");
    source   = gst_element_factory_make("filesrc", "file-source");
    demuxer  = gst_element_factory_make("qtdemux", "demuxer");
    parser   = gst_element_factory_make("h264parse", "parser");
    pay      = gst_element_factory_make("rtph264pay", "payloader");
    sink     = gst_element_factory_make("udpsink", "udp-sink");

    if (!pipeline || !source || !demuxer || !parser || !pay || !sink) {
        g_printerr("No se pudieron crear los elementos. Saliendo.\n");
        return -1;
    }

    /* Configurar propiedades */
    g_object_set(G_OBJECT(source),
                 "location", "/opt/nvidia/deepstream/deepstream/samples/streams/sample_1080p_h264.mp4",
                 NULL);

    g_object_set(G_OBJECT(sink),
                 "host", "192.168.1.50",   // ⚠️ Cambia esta IP al host receptor
                 "port", 5000,
                 "sync", FALSE,
                 "async", FALSE,
                 NULL);

    /* Conectar bus */
    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref(bus);

    /* Añadir elementos al pipeline */
    gst_bin_add_many(GST_BIN(pipeline), source, demuxer, parser, pay, sink, NULL);

    /* Conectar callback del pad dinámico */
    g_signal_connect(demuxer, "pad-added", G_CALLBACK(on_pad_added), parser);

    /* Enlazar los elementos */
    if (!gst_element_link(source, demuxer)) {
        g_printerr("Error al linkear source y demuxer.\n");
        return -1;
    }

    if (!gst_element_link_many(parser, pay, sink, NULL)) {
        g_printerr("Error al linkear parser, payloader y sink.\n");
        return -1;
    }

    /* Iniciar reproducción */
    g_print("Transmitiendo video por UDP...\n");
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    /* Loop principal */
    g_main_loop_run(loop);

    /* Limpieza */
    g_print("Deteniendo pipeline.\n");
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_source_remove(bus_watch_id);
    g_main_loop_unref(loop);

    return 0;
}
