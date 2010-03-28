#include <arv.h>
#include <arvgvinterface.h>
#include <arvgvstream.h>
#include <arvgvdevice.h>
#include <arvgcinteger.h>
#include <arvgccommand.h>
#include <arvdebug.h>
#ifdef ARAVIS_WITH_CAIRO
#include <cairo.h>
#endif
#include <stdlib.h>

static gboolean cancel = FALSE;

static void
set_cancel (int signal)
{
	cancel = TRUE;
}

static int arv_option_debug_level;
static gboolean arv_option_snaphot = FALSE;
static gboolean arv_option_auto_buffer = FALSE;
static int arv_option_width = -1;
static int arv_option_height = -1;
static int arv_option_horizontal_binning = -1;
static int arv_option_vertical_binning = -1;

static const GOptionEntry arv_option_entries[] =
{
	{ "snapshot",		's', 0, G_OPTION_ARG_NONE,
		&arv_option_snaphot,	"Snapshot", NULL},
	{ "auto",		'a', 0, G_OPTION_ARG_NONE,
		&arv_option_auto_buffer,	"AutoBufferSize", NULL},
	{ "width", 		'w', 0, G_OPTION_ARG_INT,
		&arv_option_width,		"Width", NULL },
	{ "height", 		'h', 0, G_OPTION_ARG_INT,
		&arv_option_height, 		"Height", NULL },
	{ "h-binning", 		'\0', 0, G_OPTION_ARG_INT,
		&arv_option_horizontal_binning,"Horizontal binning", NULL },
	{ "v-binning", 		'\0', 0, G_OPTION_ARG_INT,
		&arv_option_vertical_binning, 	"Vertical binning", NULL },
	{ "debug", 		'd', 0, G_OPTION_ARG_INT,
		&arv_option_debug_level, 	"Debug mode", NULL },
	{ NULL }
};

typedef enum {
	ARV_CAMERA_TYPE_BASLER,
	ARV_CAMERA_TYPE_PROSILICA
} ArvCameraType;

int
main (int argc, char **argv)
{
	ArvInterface *interface;
	ArvDevice *device;
	ArvStream *stream;
	ArvBuffer *buffer;
	GOptionContext *context;
	GError *error = NULL;
	char memory_buffer[100000];
	int i;
#ifdef ARAVIS_WITH_CAIRO
	gboolean snapshot_done = FALSE;
#endif

	g_thread_init (NULL);
	g_type_init ();

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, arv_option_entries, NULL);

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_option_context_free (context);
		g_print ("Option parsing failed: %s\n", error->message);
		g_error_free (error);
		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	arv_debug_enable (arv_option_debug_level);

	interface = arv_gv_interface_get_instance ();

	device = arv_interface_get_first_device (interface);
	if (device != NULL) {
		ArvGc *genicam;
		ArvGcNode *node;
		guint32 value;
		guint32 maximum;

		genicam = arv_device_get_genicam (device);

		if (arv_option_width > 0) {
			node = arv_gc_get_node (genicam, "Width");
			arv_gc_integer_set_value (ARV_GC_INTEGER (node), arv_option_width);
		}
		if (arv_option_height > 0) {
			node = arv_gc_get_node (genicam, "Height");
			arv_gc_integer_set_value (ARV_GC_INTEGER (node), arv_option_height);
		}
		if (arv_option_horizontal_binning > 0) {
			node = arv_gc_get_node (genicam, "BinningHorizontal");
			arv_gc_integer_set_value (ARV_GC_INTEGER (node), arv_option_horizontal_binning);
		}
		if (arv_option_vertical_binning > 0) {
			node = arv_gc_get_node (genicam, "BinningVertical");
			arv_gc_integer_set_value (ARV_GC_INTEGER (node), arv_option_vertical_binning);
		}

		node = arv_gc_get_node (genicam, "PayloadSize");
		value = arv_gc_integer_get_value (ARV_GC_INTEGER (node));
		g_print ("payload size  = %d (0x%x)\n", value, value);
		node = arv_gc_get_node (genicam, "SensorWidth");
		value = arv_gc_integer_get_value (ARV_GC_INTEGER (node));
		g_print ("sensor width  = %d\n", value);
		node = arv_gc_get_node (genicam, "SensorHeight");
		value = arv_gc_integer_get_value (ARV_GC_INTEGER (node));
		g_print ("sensor height = %d\n", value);
		node = arv_gc_get_node (genicam, "Width");
		value = arv_gc_integer_get_value (ARV_GC_INTEGER (node));
		maximum = arv_gc_integer_get_max (ARV_GC_INTEGER (node));
		g_print ("image width   = %d (max %d)\n", value, maximum);
		node = arv_gc_get_node (genicam, "Height");
		value = arv_gc_integer_get_value (ARV_GC_INTEGER (node));
		maximum = arv_gc_integer_get_max (ARV_GC_INTEGER (node));
		g_print ("image height  = %d (max %d)\n", value, maximum);
		node = arv_gc_get_node (genicam, "BinningHorizontal");
		value = arv_gc_integer_get_value (ARV_GC_INTEGER (node));
		maximum = arv_gc_integer_get_max (ARV_GC_INTEGER (node));
		g_print ("horizontal binning  = %d (max %d)\n", value, maximum);
		node = arv_gc_get_node (genicam, "BinningVertical");
		value = arv_gc_integer_get_value (ARV_GC_INTEGER (node));
		maximum = arv_gc_integer_get_max (ARV_GC_INTEGER (node));
		g_print ("vertical binning    = %d (max %d)\n", value, maximum);

		stream = arv_device_get_stream (device);
		if (arv_option_auto_buffer)
			arv_gv_stream_set_option (ARV_GV_STREAM (stream),
						  ARV_GV_STREAM_OPTION_SOCKET_BUFFER_AUTO,
						  0);

		for (i = 0; i < 30; i++)
			arv_stream_push_buffer (stream, arv_buffer_new (value, NULL));

		arv_device_read_register (device, ARV_GVBS_FIRST_STREAM_CHANNEL_PORT, &value);
		g_print ("stream port = %d (%d)\n", value, arv_gv_stream_get_port (ARV_GV_STREAM (stream)));

		arv_device_read_memory (device, 0x00014150, 8, memory_buffer);
		arv_device_read_memory (device, 0x000000e8, 16, memory_buffer);
		arv_device_read_memory (device,
					ARV_GVBS_USER_DEFINED_NAME,
					ARV_GVBS_USER_DEFINED_NAME_SIZE, memory_buffer);

		node = arv_gc_get_node (genicam, "AcquisitionStart");
		arv_gc_command_execute (ARV_GC_COMMAND (node));

		signal (SIGINT, set_cancel);

		do {
			g_usleep (100000);

			do  {
				buffer = arv_stream_pop_buffer (stream);
				if (buffer != NULL) {
#ifdef ARAVIS_WITH_CAIRO
					if (arv_option_snaphot &&
					    !snapshot_done &&
					    buffer->status == ARV_BUFFER_STATUS_SUCCESS) {
						snapshot_done = TRUE;

						cairo_surface_t *surface;

						surface = cairo_image_surface_create_for_data (buffer->data,
											       CAIRO_FORMAT_A8,
											       buffer->width,
											       buffer->height,
											       buffer->width);
						cairo_surface_write_to_png (surface, "test.png");
						cairo_surface_destroy (surface);
					}
#endif
					arv_stream_push_buffer (stream, buffer);
				}
			} while (buffer != NULL);
		} while (!cancel);

		arv_device_read_register (device, ARV_GVBS_FIRST_STREAM_CHANNEL_PORT, &value);
		g_print ("stream port = %d (%d)\n", value, arv_gv_stream_get_port (ARV_GV_STREAM (stream)));

		node = arv_gc_get_node (genicam, "AcquisitionStop");
		arv_gc_command_execute (ARV_GC_COMMAND (node));

		g_object_unref (stream);
		g_object_unref (device);
	} else
		g_print ("No device found\n");

	g_object_unref (interface);

	return 0;
}
