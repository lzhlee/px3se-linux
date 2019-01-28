/*
    TODO review
    Examples:
    ./gst-camerabin2-test --image-width=2048 --image-height=1536
    ./gst-camerabin2-test --mode=2 --capture-time=10 --image-width=848 --image-height=480 --view-framerate-num=2825 \
    --view-framerate-den=100

    gst-camerabin2-test --help
    Usage:
    gst-camerabin2-test [OPTION...]

    camerabin command line test application.

    Help Options:
    -h, --help                        Show help options
    --help-all                        Show all help options
    --help-gst                        Show GStreamer Options

    Application Options:
    --ev-compensation                 EV compensation (-2.5..2.5, default = 0)
    --aperture                        Aperture (size of lens opening, default = 0 (auto))
    --flash-mode                      Flash mode (default = 0 (auto))
    --scene-mode                      Scene mode (default = 6 (auto))
    --exposure                        Exposure (default = 0 (auto))
    --iso-speed                       ISO speed (default = 0 (auto))
    --white-balance-mode              White balance mode (default = 0 (auto))
    --colour-tone-mode                Colour tone mode (default = 0 (auto))
    --directory                       Directory for capture file(s) (default is current directory)
    --mode                            Capture mode (default = 0 (image), 1 = video)
    --capture-time                    Time to capture video in seconds (default = 10)
    --capture-total                   Total number of captures to be done (default = 1)
    --zoom                            Zoom (100 = 1x (default), 200 = 2x etc.)
    --wrapper-source                  Camera source wrapper used for setting the video source
    --video-source                    Video source used in still capture and video recording
    --video-device                    Video device to be set on the video source (e.g. /dev/video0)
    --audio-source                    Audio source used in video recording
    --image-pp                        List of image post-processing elements separated with comma
    --viewfinder-sink                 Viewfinder sink (default = fakesink)
    --image-width                     Width for capture (only used if the caps
    arguments aren't set)
    --image-height                    Height for capture (only used if the caps
    arguments aren't set)
    --view-framerate-num              Framerate numerator for viewfinder
    --view-framerate-den              Framerate denominator for viewfinder
    --preview-caps                    Preview caps (e.g. video/x-raw-rgb,width=320,height=240)
    --viewfinder-filter               Filter to process all frames going to viewfinder sink
    --x-width                         X window width (default = 320)
    --x-height                        X window height (default = 240)
    --no-xwindow                      Do not create XWindow
    --encoding-target                 Video encoding target name
    --encoding-profile                Video encoding profile name
    --encoding-profile-filename       Video encoding profile filename
    --image-capture-caps              Image capture caps (e.g. video/x-raw-rgb,width=640,height=480)
    --viewfinder-caps                 Viewfinder caps (e.g. video/x-raw-rgb,width=640,height=480)
    --video-capture-caps              Video capture caps (e.g. video/x-raw-rgb,width=640,height=480)
    --performance-measure             Collect timing information about the
    captures and provides performance statistics at the end
    --performance-targets             A list of doubles that are the performance target
    times for each of the measured timestamps. The order is
    startup time, change mode time, shot to save, shot to snapshot,
    shot to shot, preview to precapture, shot to buffer.
    e.g. 3.5,1.0,5.0,2.5,5.0,1.5,1.0
    * Startup time -> time it takes for camerabin to reach playing
    * Change mode time -> time it takes for camerabin to change to the selected
    mode in playing
    * Shot to save -> time it takes from start-capture to having the image saved
    to disk
    * Shot to snapshot -> time it takes from start-capture to getting a snapshot
    * Shot to shot -> time from one start-capture to the next one
    * Preview to precapture -> time it takes from getting the snapshot to the
    next buffer that reaches the viewfinder
    * Shot to buffer -> time it takes from start-capture to the moment a buffer
    is pushed out of the camera source

    typedef enum {
  GST_LEVEL_NONE = 0,
  GST_LEVEL_ERROR = 1,
  GST_LEVEL_WARNING = 2,
  GST_LEVEL_FIXME = 3,
  GST_LEVEL_INFO = 4,
  GST_LEVEL_DEBUG = 5,
  GST_LEVEL_LOG = 6,
  GST_LEVEL_TRACE = 7,
  GST_LEVEL_MEMDUMP = 9,
  GST_LEVEL_COUNT
} GstDebugLevel;

  */
#include "camerawidgets.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <gst/video/videooverlay.h>

#define CIF_CAMERA

QPushButton *cameraWidgets::m_capture;
QPushButton *cameraWidgets::m_mode;
QWidget *cameraWidgets::m_frame;
QCameraViewfinder *cameraWidgets::m_viewfinder;

cameraWidgets::cameraWidgets(QWidget *parent):baseWidget(parent)
{
    setObjectName("cameraWidgets");
    setStyleSheet("#cameraWidgets{background:rgb(10,10,10);}");
    init();
    initLayout();
}

cameraWidgets::~cameraWidgets()
{
    qDebug() << "cameraWidgets destroy";
    /* free */
    {
    GList *iter;

    for (iter = capture_times; iter; iter = g_list_next (iter)) {
        g_slice_free (CaptureTiming, iter->data);
    }
        g_list_free (capture_times);
    }

    g_string_free (filename, TRUE);
    g_free (wrappersrc_name);
    g_free (videosrc_name);
    g_free (videodevice_name);
    g_free (audiosrc_name);
    g_free (imagepp_name);
    g_free (vfsink_name);
    g_free (gep_targetname);
    g_free (gep_profilename);
    g_free (gep_filename);
    g_free (performance_targets_str);
    qDebug() << "~cameraWidgets end";
}

void cameraWidgets::showEvent(QShowEvent *event) {
    qDebug() << "cameraWidgets::showEvent~~~~~~~~~~~";
    //openCamera();
    OpenCameraThread *workerThread = new OpenCameraThread(this);
    connect(workerThread, SIGNAL(resultReady(QString)), this, SLOT(handleResults(QString)));
    connect(workerThread, SIGNAL(finished()), workerThread, SLOT(deleteLater()));
    workerThread->start();
}

void cameraWidgets::hideEvent(QHideEvent *event) {
    qDebug() << "cameraWidgets::hideEvent";
}
void cameraWidgets::openCamera()
{
    qDebug() << "openCamera start";
    
    gst_debug_set_default_threshold(GST_LEVEL_FIXME);
    gst_debug_set_threshold_for_name ("camerabin", GST_LEVEL_DEBUG);

    mode == MODE_IMAGE;
    recording = FALSE;
    ready_for_capture = FALSE;

#ifdef CIF_CAMERA
    m_cif_camera = new RkCifCamera(this);
    if (!m_cif_camera)
        return;
	m_cif_camera->start();
#else
    if (setup_pipeline()) {
        g_idle_add ((GSourceFunc) run_preview_pipeline, NULL);
    }
#endif
    qDebug() << "openCamera end";
}

void cameraWidgets::closeCamera()
{
#ifdef CIF_CAMERA
    if (m_cif_camera) {
        m_cif_camera->stop();
        delete m_cif_camera;
        m_cif_camera = NULL;
    }
#else
    cleanup_pipeline ();
#endif
    /* performance */
    if (performance_measure) {
        print_performance_data ();
    }
}   

void cameraWidgets::slot_cameraerror(QCamera::Error value)
{
    qDebug() << "camera error:" << value;
}

void cameraWidgets::slot_pressed()
{
    qDebug() << "capture btn pressed";
    m_mode->setText("Switching Mode");
}

void cameraWidgets::slot_released()
{
    qDebug() << "capture btn released";
}

void cameraWidgets::slot_captureerror(int id, QCameraImageCapture::Error error, const QString &errorString)
{
    qDebug() << "id:" << id << " capture error:" << errorString;
}

void cameraWidgets::readyForCapture(bool ready)
{
    qDebug() << "readyForCapture:" << ready;
}

void cameraWidgets::processCapturedImage(int requestId,const QImage & img)
{
    qDebug() << requestId << " processCapturedImage: " << img;
}

void cameraWidgets::imageSaved(int id,const QString & filename)
{
    qDebug() << id << "imageSaved = " << filename;
}

void cameraWidgets::slot_capture()
{
    qDebug() << "capture btn clicked";
    if (mode == MODE_IMAGE || (mode == MODE_VIDEO && !recording)) {
        if (camerabin && ready_for_capture) {
            qDebug() << "start " << (mode == MODE_IMAGE ? "capture" : "recorder");
            g_idle_add ((GSourceFunc) run_taking_pipeline, NULL);                 
        }
    } else {
        qDebug() << "stop recorder";
        if (camerabin)
            stop_capture(NULL);
    }
}

void cameraWidgets::slot_modeswitch()
{
    qDebug() << "mode switch btn clicked";
    switchMode();
}

void cameraWidgets::updateRecorderState(QMediaRecorder::State state)
{
    switch (state) {
    case QMediaRecorder::StoppedState:
        //m_recorder->setText("start recorder");
        break;
    case QMediaRecorder::PausedState:
        //m_recorder->setText("start recorder");
        break;
    case QMediaRecorder::RecordingState:
        //m_recorder->setText("stop recorder");
        break;
    }
}

void cameraWidgets::updateRecordTime()
{
    qDebug() << "Recorded " << m_mediaRecorder->duration()/1000 << " sec";
    qDebug() << "Recorded format:" << m_mediaRecorder->containerFormat();
    qDebug() << "Recorded videoSettings:"  << m_mediaRecorder->videoSettings().codec();
}

void cameraWidgets::displayRecorderError()
{
    qDebug() << "Capture error:" << m_mediaRecorder->errorString();
}

void cameraWidgets::handleResults(const QString &) {
    qDebug() << "Thread run over";
}

GstPadProbeReturn
cameraWidgets::camera_src_get_timestamp_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer udata)
{
  CaptureTiming *timing;

  timing = (CaptureTiming *) g_list_first (capture_times)->data;
  timing->camera_capture = gst_util_get_timestamp ();

  return GST_PAD_PROBE_REMOVE;
}

GstPadProbeReturn
cameraWidgets::viewfinder_get_timestamp_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer udata)
{
  CaptureTiming *timing;

  timing = (CaptureTiming *) g_list_first (capture_times)->data;
  timing->precapture = gst_util_get_timestamp ();

  return GST_PAD_PROBE_REMOVE;
}

GstBusSyncReply
cameraWidgets::sync_bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  const GstStructure *st;
  const GValue *image;
  GstBuffer *buf = NULL;
  gchar *preview_filename = NULL;
  FILE *f = NULL;
  size_t written;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ELEMENT:{
      st = gst_message_get_structure (message);
      if (st) {
         if (gst_message_has_name (message, "prepare-window-handle")) {
            GST_FIXME ("prepare-window-handle");
            if (viewfinder_sink && GST_IS_VIDEO_OVERLAY(viewfinder_sink)) {
                m_winid = m_frame->winId();
                qDebug() << "===m_viewfinder winid:" << m_winid;
                //gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(viewfinder_sink), m_winid);
            }
         } else if (gst_message_has_name (message, "prepare-xwindow-id")) {
            GST_FIXME ("prepare-xwindow-id");
         } else if (gst_structure_has_name (st, "preview-image")) {
            CaptureTiming *timing;

            GST_FIXME ("preview-image");

            timing = (CaptureTiming *) g_list_first (capture_times)->data;
            timing->got_preview = gst_util_get_timestamp ();

            {
            /* set up probe to check when the viewfinder gets data */
            GstPad *pad = gst_element_get_static_pad (viewfinder_sink, "sink");

            viewfinder_probe_id =
                gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
                viewfinder_get_timestamp_probe, NULL, NULL);

            gst_object_unref (pad);
            }

            /* extract preview-image from msg */
            image = gst_structure_get_value (st, "buffer");
            qDebug() << "gst_structure_get_value image:" << image;
            if (image) {
                buf = gst_value_get_buffer (image);
                preview_filename = g_strdup_printf ("/mnt/sdcard/test_vga.rgb");
                f = g_fopen (preview_filename, "w");
                qDebug() << "preview_filename open:" << f; 
                if (f) {
                  GstMapInfo map;

                  gst_buffer_map (buf, &map, GST_MAP_READ);
                  written = fwrite (map.data, map.size, 1, f);
                  gst_buffer_unmap (buf, &map);
                  if (!written) {
                    g_print ("error writing file\n");
                  }
                  fclose (f);
                } else {
                  g_print ("error opening file for raw image writing\n");
                }
                g_free (preview_filename);
            }
        }
      }
      break;
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_MESSAGE_SRC (message) == (GstObject *) camerabin) {
        GstState newstate;

        gst_message_parse_state_changed (message, NULL, &newstate, NULL);
        if (newstate == GST_STATE_PLAYING) {
          startup_time = gst_util_get_timestamp ();
        }
      }
      break;
    }
    default:
      /* unhandled message */
      break;
  }
  return GST_BUS_PASS;
}

gboolean cameraWidgets::bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *debug;

      gst_message_parse_error (message, &err, &debug);
      g_print ("Error: %s\n", err->message);
      g_clear_error (&err);
      g_free (debug);

      /* Write debug graph to file */
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (camerabin),
          GST_DEBUG_GRAPH_SHOW_ALL, "camerabin.error");

      //g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_IS_BIN (GST_MESSAGE_SRC (message))) {
        GstState oldstate, newstate;

        gst_message_parse_state_changed (message, &oldstate, &newstate, NULL);
        GST_FIXME_OBJECT (GST_MESSAGE_SRC (message), "state-changed: %s -> %s",
            gst_element_state_get_name (oldstate),
            gst_element_state_get_name (newstate));
      }
      break;
    case GST_MESSAGE_EOS:
      /* end-of-stream */
      GST_FIXME ("got eos() - should not happen");
      //g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_ELEMENT:
      if (GST_MESSAGE_SRC (message) == (GstObject *) camerabin) {
        const GstStructure *structure = gst_message_get_structure (message);

        if (gst_structure_has_name (structure, "image-done")) {
          CaptureTiming *timing;
#ifndef GST_DISABLE_GST_DEBUG
          const gchar *fname = gst_structure_get_string (structure, "filename");

          GST_FIXME ("image done: %s", fname);
#endif
          timing = (CaptureTiming *) g_list_first (capture_times)->data;
          timing->capture_done = gst_util_get_timestamp ();

          if (capture_count < capture_total) {
            g_idle_add ((GSourceFunc) run_preview_pipeline, NULL);
          } else {
            //g_main_loop_quit (loop);
          }
        }
      }
      break;
    default:
      /* unhandled message */
      break;
  }
  return TRUE;
}

void cameraWidgets::switchMode() {
    cleanup_pipeline();
    mode = mode == MODE_IMAGE ? MODE_VIDEO : MODE_IMAGE;
    if (setup_pipeline()) {
        g_idle_add ((GSourceFunc) run_preview_pipeline, NULL);
    }
}

void cameraWidgets::cleanup_pipeline (void)
{
  if (camerabin) {
    //if (m_viewfinderInterface)
    //    m_viewfinderInterface->stopRenderer();
    GST_FIXME_OBJECT (camerabin, "stopping and destroying");
    GstElement *camera_source = NULL;
    g_object_get (camerabin, "camera-source", &camera_source, NULL);
    if (camera_source)
        g_signal_handler_disconnect (camera_source, ready_for_capture_id);
    gst_element_set_state (camerabin, GST_STATE_NULL);
    gst_object_unref (camerabin);
    camerabin = NULL;
  }
  if (viewfinder_sink)
     gst_object_unref(viewfinder_sink);
}

GstElement *
cameraWidgets::create_ipp_bin (void)
{
  GstElement *bin = NULL, *element = NULL;
  GstPad *pad = NULL;
  gchar **elements;
  GList *element_list = NULL, *current = NULL, *next = NULL;
  int i;

  bin = gst_bin_new ("ippbin");

  elements = g_strsplit (imagepp_name, ",", 0);

  for (i = 0; elements[i] != NULL; i++) {
    element = gst_element_factory_make (elements[i], NULL);
    if (element) {
      element_list = g_list_append (element_list, element);
      gst_bin_add (GST_BIN (bin), element);
    } else
      GST_WARNING ("Could create element %s for ippbin", elements[i]);
  }

  for (i = 1; i < g_list_length (element_list); i++) {
    current = g_list_nth (element_list, i - 1);
    next = g_list_nth (element_list, i);
    gst_element_link ((GstElement * ) current->data, (GstElement * ) next->data);
  }

  current = g_list_first (element_list);
  pad = gst_element_get_static_pad ((GstElement * ) current->data, "sink");
  gst_element_add_pad (bin, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (GST_OBJECT (pad));

  current = g_list_last (element_list);
  pad = gst_element_get_static_pad ((GstElement * ) current->data, "src");
  gst_element_add_pad (bin, gst_ghost_pad_new ("src", pad));
  gst_object_unref (GST_OBJECT (pad));

  g_list_free (element_list);
  g_strfreev (elements);

  return bin;
}

GstEncodingProfile *
cameraWidgets::load_encoding_profile (void)
{
  GstEncodingProfile *prof = NULL;
  GstEncodingTarget *target = NULL;
  GError *error = NULL;

  /* if profile file was given, try to load profile from there */
  if (gep_filename && gep_profilename) {
    target = gst_encoding_target_load_from_file (gep_filename, &error);
    if (!target) {
      GST_WARNING ("Could not load target %s from file %s", gep_targetname,
          gep_filename);
      if (error) {
        GST_WARNING ("Error from file loading: %s", error->message);
        g_clear_error (&error);
      }
    } else {
      prof = gst_encoding_target_get_profile (target, gep_profilename);
      if (prof)
        GST_FIXME ("Loaded encoding profile %s from %s", gep_profilename,
            gep_filename);
      else
        GST_WARNING
            ("Could not load specified encoding profile %s from file %s",
            gep_profilename, gep_filename);
    }
    /* if we could not load profile from file then try to find one from system */
  } else if (gep_profilename && gep_targetname) {
    prof = gst_encoding_profile_find (gep_targetname, gep_profilename, NULL);
    if (prof)
      GST_FIXME ("Loaded encoding profile %s from target %s", gep_profilename,
          gep_targetname);
  } else
    GST_FIXME
        ("Encoding profile not set, using camerabin default encoding profile");

  return prof;
}

gboolean
cameraWidgets::setup_pipeline_element (GstElement * element, const gchar * property_name,
    const gchar * element_name, GstElement ** res_elem)
{
  gboolean res = TRUE;
  GstElement *elem = NULL;

  if (element_name) {
    GError *error = NULL;

    elem = gst_parse_launch (element_name, &error);
    if (elem) {
      g_object_set (element, property_name, elem, NULL);
      g_object_unref (elem);
    } else {
      GST_WARNING ("can't create element '%s' for property '%s'", element_name,
          property_name);
      if (error) {
        GST_ERROR ("%s", error->message);
        g_clear_error (&error);
      }
      res = FALSE;
    }
  } else {
    GST_FIXME ("no element for property '%s' given", property_name);
  }
  if (res_elem)
    *res_elem = elem;
  return res;
}


void cameraWidgets::set_camerabin_caps_from_string (void)
{
  GstCaps *caps = NULL;
  if (image_capture_caps_str != NULL) {
    caps = gst_caps_from_string (image_capture_caps_str);
    if (GST_CAPS_IS_SIMPLE (caps) && image_width > 0 && image_height > 0) {
      gst_caps_set_simple (caps, "width", G_TYPE_INT, image_width, "height",
          G_TYPE_INT, image_height, NULL);
    }
    GST_FIXME ("setting image-capture-caps: %" GST_PTR_FORMAT, caps);
    g_object_set (camerabin, "image-capture-caps", caps, NULL);
    gst_caps_unref (caps);
  }

  if (viewfinder_caps_str != NULL) {
    caps = gst_caps_from_string (viewfinder_caps_str);
    if (GST_CAPS_IS_SIMPLE (caps) && view_framerate_num > 0
        && view_framerate_den > 0) {
      gst_caps_set_simple (caps, "framerate", GST_TYPE_FRACTION,
          view_framerate_num, view_framerate_den, NULL);
    }
    GST_FIXME ("setting viewfinder-caps: %" GST_PTR_FORMAT, caps);
    g_object_set (camerabin, "viewfinder-caps", caps, NULL);
    gst_caps_unref (caps);
  }

  if (video_capture_caps_str != NULL) {
    caps = gst_caps_from_string (video_capture_caps_str);
    GST_FIXME ("setting video-capture-caps: %" GST_PTR_FORMAT, caps);
    g_object_set (camerabin, "video-capture-caps", caps, NULL);
    gst_caps_unref (caps);
  }

}

gboolean cameraWidgets::setup_pipeline (void)
{
  gboolean res = TRUE;
  GstBus *bus;
  GstElement *sink = NULL, *ipp = NULL;
  GstEncodingProfile *prof = NULL;
  GIOChannel *key_control = NULL;
  GstElement *camera_source = NULL;
  gboolean idle = FALSE;

  initial_time = gst_util_get_timestamp ();

  camerabin = gst_element_factory_make ("camerabin", NULL);
  if (NULL == camerabin) {
    g_warning ("can't create camerabin element\n");
    goto error;
  }

  //key_control = g_io_channel_unix_new (fileno (stdin));
  //g_io_add_watch (key_control, G_IO_IN, (GIOFunc) keypress, NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (camerabin));
  /* Add sync handler for time critical messages that need to be handled fast */
  gst_bus_set_sync_handler (bus, sync_bus_callback, NULL, NULL);
  /* Handle normal messages asynchronously */
  gst_bus_add_watch (bus, bus_callback, NULL);
  gst_object_unref (bus);

  GST_FIXME_OBJECT (camerabin, "camerabin created");

  if (camerabin_flags)
    gst_util_set_object_arg (G_OBJECT (camerabin), "flags", camerabin_flags);
  else
    gst_util_set_object_arg (G_OBJECT (camerabin), "flags", "");

  if (videosrc_name) {
    GstElement *wrapper;
    GstElement *videosrc;

    if (wrappersrc_name)
      wrapper = gst_element_factory_make (wrappersrc_name, NULL);
    else
      wrapper = gst_element_factory_make ("wrappercamerabinsrc", NULL);

    if (setup_pipeline_element (wrapper, "video-source", videosrc_name, NULL)) {
      g_object_set (camerabin, "camera-source", wrapper, NULL);
      g_object_unref (wrapper);
    } else {
      GST_WARNING ("Failed to set videosrc to %s", videosrc_name);
    }

    g_object_get (wrapper, "video-source", &videosrc, NULL);
    if (videodevice_name == NULL)
      videodevice_name = g_strdup("/dev/video0");
    if (videosrc && videodevice_name &&
        g_object_class_find_property (G_OBJECT_GET_CLASS (videosrc),
            "device")) {
      g_object_set (videosrc, "device", videodevice_name, NULL);
    }
  }

  /* configure used elements */
  res &=
      setup_pipeline_element (camerabin, "audio-source", audiosrc_name, NULL);
  res &=
      setup_pipeline_element (camerabin, "viewfinder-sink", vfsink_name, &sink);
  res &=
      setup_pipeline_element (camerabin, "viewfinder-filter", viewfinder_filter,
      NULL);
  
  if (imagepp_name) {
    ipp = create_ipp_bin ();
    if (ipp) {
      g_object_set (camerabin, "image-filter", ipp, NULL);
      g_object_unref (ipp);
    } else
      GST_WARNING ("Could not create ipp elements");
  }

  prof = create_ogg_profile();//create_mp4_profile ();
  if (prof) {
    g_object_set (G_OBJECT (camerabin), "video-profile", prof, NULL);
    gst_encoding_profile_unref (prof);
  }

  GST_FIXME_OBJECT (camerabin, "elements created");

  if (sink) {
    g_object_set (sink, "sync", FALSE, NULL);
    gst_object_ref_sink(sink);
  } else {
    /* Get the inner viewfinder sink, this uses fixed names given
     * by default in camerabin */
    sink = gst_bin_get_by_name (GST_BIN (camerabin), "vf-bin");
    g_assert (sink);
    gst_object_unref (sink);

    sink = gst_bin_get_by_name (GST_BIN (sink), "vfbin-sink");
    g_assert (sink);
    gst_object_unref (sink);
  }
  viewfinder_sink = sink;

  qDebug() << "=======GST_IS_VIDEO_OVERLAY:" << GST_IS_VIDEO_OVERLAY(viewfinder_sink);

  GST_FIXME_OBJECT (camerabin, "elements configured");

  /* configure a resolution and framerate */
  if (image_width > 0 && image_height > 0) {
    if (mode == MODE_VIDEO) {
      GstCaps *caps = NULL;
      if (view_framerate_num > 0)
        caps = gst_caps_new_full (gst_structure_new ("video/x-raw",
                "width", G_TYPE_INT, image_width,
                "height", G_TYPE_INT, image_height,
                "framerate", GST_TYPE_FRACTION, view_framerate_num,
                view_framerate_den, NULL), NULL);
      else
        caps = gst_caps_new_full (gst_structure_new ("video/x-raw",
                "width", G_TYPE_INT, image_width,
                "height", G_TYPE_INT, image_height, NULL), NULL);

      g_object_set (camerabin, "video-capture-caps", caps, NULL);
      gst_caps_unref (caps);
    } else {
      GstCaps *caps = gst_caps_new_full (gst_structure_new ("video/x-raw",
              "width", G_TYPE_INT, image_width,
              "height", G_TYPE_INT, image_height, NULL), NULL);

      g_object_set (camerabin, "image-capture-caps", caps, NULL);
      gst_caps_unref (caps);
    }
  }

  set_camerabin_caps_from_string ();

  /* change to the wrong mode if timestamping if performance mode is on so
   * we can change it back and measure the time after in playing */
  if (performance_measure) {
    g_object_set (camerabin, "mode",
        mode == MODE_VIDEO ? MODE_IMAGE : MODE_VIDEO, NULL);
  }

  if (GST_STATE_CHANGE_FAILURE ==
      gst_element_set_state (camerabin, GST_STATE_READY)) {
    g_warning ("can't set camerabin to ready\n");
    goto error;
  }
  GST_FIXME_OBJECT (camerabin, "camera ready");

  if (GST_STATE_CHANGE_FAILURE ==
      gst_element_set_state (camerabin, GST_STATE_PLAYING)) {
    g_warning ("can't set camerabin to playing\n");
    goto error;
  }

  GST_FIXME_OBJECT (camerabin, "camera started");

  g_object_get (camerabin, "idle", &idle, NULL);
  
  qDebug() << "camera idle:" << idle;

  g_object_get (camerabin, "camera-source", &camera_source, NULL);
  if (camera_source) {
    ready_for_capture_id = g_signal_connect(camera_source, "notify::ready-for-capture",
      (GCallback) notify_readyforcapture, camera_source);
    g_object_get (camera_source, "ready-for-capture", &ready_for_capture, NULL);
    qDebug() << "ready_for_capture_id:" << ready_for_capture_id << ",ready for captrue:" << ready_for_capture;
  }

  /* do the mode change timestamping if performance mode is on */
  if (performance_measure) {
    change_mode_before = gst_util_get_timestamp ();
    g_object_set (camerabin, "mode", mode, NULL);
    change_mode_after = gst_util_get_timestamp ();
  }

  return TRUE;
error:
  cleanup_pipeline ();
  return FALSE;
}

void
cameraWidgets::stop_capture_cb (GObject * self, GParamSpec * pspec, gpointer user_data)
{
  qDebug() << "stop_capture_cb";
  gboolean idle = FALSE;

  g_object_get (camerabin, "idle", &idle, NULL);
  qDebug() << "camerabin idle:" << idle;

  if (idle) {
    if (capture_count < capture_total) {
      g_idle_add ((GSourceFunc) run_preview_pipeline, NULL);
    } else {
      recording = FALSE;
      m_capture->setText("start Recorder");
      //g_main_loop_quit (loop);
    }
  }

  g_signal_handler_disconnect (camerabin, stop_capture_cb_id);
}

gboolean
cameraWidgets::stop_capture (gpointer user_data)
{
  stop_capture_cb_id =  g_signal_connect (camerabin, "notify::idle",
      (GCallback) stop_capture_cb, camerabin);
  g_signal_emit_by_name (camerabin, "stop-capture", 0);
  return FALSE;
}

void cameraWidgets::notify_readyforcapture(GObject * obj,GParamSpec * pspec,gpointer user_data) {
    g_object_get (obj, "ready-for-capture", &ready_for_capture, NULL);
        qDebug() << "notify_readyforcapture:" << ready_for_capture;
    if (mode == MODE_VIDEO && ready_for_capture) {
        recording = FALSE;
        m_capture->setText("start Recorder");
    }
}

void cameraWidgets::set_metadata (GstElement * camera)
{
  GstTagSetter *setter = GST_TAG_SETTER (camera);
  GstDateTime *datetime;
  gchar *desc_str;

  datetime = gst_date_time_new_now_local_time ();

  desc_str = g_strdup_printf ("captured by %s", g_get_real_name ());

  gst_tag_setter_add_tags (setter, GST_TAG_MERGE_REPLACE,
      GST_TAG_DATE_TIME, datetime,
      GST_TAG_DESCRIPTION, desc_str,
      GST_TAG_TITLE, "rockchip camera capture",
      GST_TAG_GEO_LOCATION_LONGITUDE, 1.0,
      GST_TAG_GEO_LOCATION_LATITUDE, 2.0,
      GST_TAG_GEO_LOCATION_ELEVATION, 3.0,
      GST_TAG_DEVICE_MANUFACTURER, "gst-camerabin-test manufacturer",
      GST_TAG_DEVICE_MODEL, "gst-camerabin-test model", NULL);

  g_free (desc_str);
  gst_date_time_unref (datetime);
}

gboolean cameraWidgets::run_taking_pipeline (gpointer user_data)
{
  gchar *filename_str = NULL;
  GstElement *video_source = NULL;
  const gchar *filename_suffix;
  CaptureTiming *timing;

    /* Construct filename */
  if (mode == MODE_VIDEO)
    filename_suffix = ".mp4";
  else
    filename_suffix = ".jpg";
  filename_str =
      g_strdup_printf ("%s/test_%04u%s", filename->str, capture_count,
      filename_suffix);
  GST_FIXME ("Setting filename: %s", filename_str);
  g_object_set (camerabin, "location", filename_str, NULL);
  g_free (filename_str);

  g_object_get (camerabin, "camera-source", &video_source, NULL);
  if (video_source) {
    if (GST_IS_ELEMENT (video_source) && GST_IS_PHOTOGRAPHY (video_source)) {
      /* Set GstPhotography interface options. If option not given as
         command-line parameter use default of the source element. */
      if (scene_mode != SCENE_MODE_NONE)
        g_object_set (video_source, "scene-mode", scene_mode, NULL);
      if (ev_compensation != EV_COMPENSATION_NONE)
        g_object_set (video_source, "ev-compensation", ev_compensation, NULL);
      if (aperture != APERTURE_NONE)
        g_object_set (video_source, "aperture", aperture, NULL);
      if (flash_mode != FLASH_MODE_NONE)
        g_object_set (video_source, "flash-mode", flash_mode, NULL);
      if (exposure != EXPOSURE_NONE)
        g_object_set (video_source, "exposure", exposure, NULL);
      if (iso_speed != ISO_SPEED_NONE)
        g_object_set (video_source, "iso-speed", iso_speed, NULL);
      if (wb_mode != WHITE_BALANCE_MODE_NONE)
        g_object_set (video_source, "white-balance-mode", wb_mode, NULL);
      if (color_mode != COLOR_TONE_MODE_NONE)
        g_object_set (video_source, "colour-tone-mode", color_mode, NULL);
    }
    g_object_unref (video_source);
  } else {
    video_source = gst_bin_get_by_name (GST_BIN (camerabin), "camerasrc");
    gst_object_unref (video_source);
  }
  g_object_set (camerabin, "zoom", zoom / 100.0f, NULL);

  capture_count++;

  timing = g_slice_new0 (CaptureTiming);
  capture_times = g_list_prepend (capture_times, timing);

  /* set pad probe to check when buffer leaves the camera source */
  if (mode == MODE_IMAGE) {
    GstPad *pad;

    pad = gst_element_get_static_pad (video_source, "imgsrc");
    camera_probe_id = gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
        camera_src_get_timestamp_probe, NULL, NULL);

    gst_object_unref (pad);
  }
  timing->start_capture = gst_util_get_timestamp ();
  g_signal_emit_by_name (camerabin, "start-capture", 0);

  if (mode == MODE_VIDEO) {
    recording = TRUE;
    m_capture->setText("stop Recorder");
    //g_timeout_add ((capture_time * 1000), (GSourceFunc) stop_capture, NULL);
  }


  return FALSE;
}

gboolean cameraWidgets::run_preview_pipeline (gpointer user_data)
{
  qDebug() << "run_preview_pipeline";
  GstCaps *preview_caps = NULL;

  g_object_set (camerabin, "mode", mode, NULL);

  if (preview_caps_name != NULL) {
    preview_caps = gst_caps_from_string (preview_caps_name);
    if (preview_caps) {
      g_object_set (camerabin, "post-previews", TRUE, NULL);
      g_object_set (camerabin, "preview-caps", preview_caps, NULL);
      GST_FIXME ("Preview caps set");
    } else
      GST_FIXME ("Preview caps set but could not create caps from string");
  }

  set_metadata (camerabin);

//  m_capture->setText(mode == MODE_IMAGE ? "Take Picture" : "Start Recorder");
//  m_mode->setText(mode == MODE_IMAGE ? "Capture Image Mode" : "Video Recorder Mode");

  return FALSE;
}

void
cameraWidgets::parse_target_values (void)
{
  gdouble startup = 0, change_mode = 0, shot_to_save = 0, shot_to_snapshot = 0;
  gdouble shot_to_shot = 0, preview_to_precapture = 0, shot_to_buffer = 0;

  if (performance_targets_str == NULL)
    return;

  /*
     startup time, change mode time, shot to save, shot to snapshot,
     shot to shot, preview to precapture, shot to buffer.
   */
  sscanf (performance_targets_str, "%lf,%lf,%lf,%lf,%lf,%lf,%lf",
      &startup, &change_mode, &shot_to_save,
      &shot_to_snapshot, &shot_to_shot, &preview_to_precapture,
      &shot_to_buffer);

  target_startup = (GstClockTime) (startup * GST_SECOND);
  target_change_mode = (GstClockTime) (change_mode * GST_SECOND);
  target_shot_to_save = (GstClockTime) (shot_to_save * GST_SECOND);
  target_shot_to_snapshot = (GstClockTime) (shot_to_snapshot * GST_SECOND);
  target_shot_to_shot = (GstClockTime) (shot_to_shot * GST_SECOND);
  target_preview_to_precapture =
      (GstClockTime) (preview_to_precapture * GST_SECOND);
  target_shot_to_buffer = (GstClockTime) (shot_to_buffer * GST_SECOND);
}

void
cameraWidgets::print_performance_data (void)
{
  GList *iter;
  gint i = 0;
  GstClockTime last_start = 0;
  CaptureTimingStats max;
  CaptureTimingStats min;
  CaptureTimingStats avg;
  CaptureTimingStats avg_wo_first;
  GstClockTime shot_to_shot;

  if (!performance_measure)
    return;

  parse_target_values ();

  /* Initialize stats */
  min.shot_to_shot = -1;
  min.shot_to_save = -1;
  min.shot_to_snapshot = -1;
  min.preview_to_precapture = -1;
  min.shot_to_buffer = -1;
  memset (&avg, 0, sizeof (CaptureTimingStats));
  memset (&avg_wo_first, 0, sizeof (CaptureTimingStats));
  memset (&max, 0, sizeof (CaptureTimingStats));

  g_print ("-- Performance results --\n");
  g_print ("Startup time: %" TIME_FORMAT "; Target: %" TIME_FORMAT "\n",
      TIME_ARGS (startup_time - initial_time), TIME_ARGS (target_startup));
  g_print ("Change mode time: %" TIME_FORMAT "; Target: %" TIME_FORMAT "\n",
      TIME_ARGS (change_mode_after - change_mode_before),
      TIME_ARGS (target_change_mode));

  g_print
      ("\n   | Shot to save |Shot to snapshot| Shot to shot |"
      "Preview to precap| Shot to buffer\n");
  capture_times = g_list_reverse (capture_times);
  for (iter = capture_times; iter; iter = g_list_next (iter)) {
    CaptureTiming *t = (CaptureTiming *) iter->data;
    CaptureTimingStats stats;

    stats.shot_to_save = SHOT_TO_SAVE (t);
    stats.shot_to_snapshot = SHOT_TO_SNAPSHOT (t);
    stats.shot_to_shot = i == 0 ? 0 : t->start_capture - last_start;
    stats.preview_to_precapture = PREVIEW_TO_PRECAPTURE (t);
    stats.shot_to_buffer = SHOT_TO_BUFFER (t);

    PRINT_STATS (i, &stats);

    if (i != 0) {
      capture_timing_stats_add (&avg_wo_first, &stats);
    }
    capture_timing_stats_add (&avg, &stats);

    if (stats.shot_to_save < min.shot_to_save) {
      min.shot_to_save = stats.shot_to_save;
    }
    if (stats.shot_to_snapshot < min.shot_to_snapshot) {
      min.shot_to_snapshot = stats.shot_to_snapshot;
    }
    if (stats.shot_to_shot < min.shot_to_shot && stats.shot_to_shot > 0) {
      min.shot_to_shot = stats.shot_to_shot;
    }
    if (stats.preview_to_precapture < min.preview_to_precapture) {
      min.preview_to_precapture = stats.preview_to_precapture;
    }
    if (stats.shot_to_buffer < min.shot_to_buffer) {
      min.shot_to_buffer = stats.shot_to_buffer;
    }


    if (stats.shot_to_save > max.shot_to_save) {
      max.shot_to_save = stats.shot_to_save;
    }
    if (stats.shot_to_snapshot > max.shot_to_snapshot) {
      max.shot_to_snapshot = stats.shot_to_snapshot;
    }
    if (stats.shot_to_shot > max.shot_to_shot) {
      max.shot_to_shot = stats.shot_to_shot;
    }
    if (stats.preview_to_precapture > max.preview_to_precapture) {
      max.preview_to_precapture = stats.preview_to_precapture;
    }
    if (stats.shot_to_buffer > max.shot_to_buffer) {
      max.shot_to_buffer = stats.shot_to_buffer;
    }

    last_start = t->start_capture;
    i++;
  }

  if (i == 0)
    return;

  if (i > 1)
    shot_to_shot = avg.shot_to_shot / (i - 1);
  else
    shot_to_shot = GST_CLOCK_TIME_NONE;
  capture_timing_stats_div (&avg, i);
  avg.shot_to_shot = shot_to_shot;
  if (i > 1)
    capture_timing_stats_div (&avg_wo_first, i - 1);
  else {
    memset (&avg_wo_first, 0, sizeof (CaptureTimingStats));
  }

  g_print ("\n    Stats             |     MIN      |     MAX      |"
      "     AVG      | AVG wo First |   Target     | Diff \n");
  g_print ("Shot to shot          | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIME_FORMAT " | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIMEDIFF_FORMAT "\n",
      TIME_ARGS (min.shot_to_shot), TIME_ARGS (max.shot_to_shot),
      TIME_ARGS (avg.shot_to_shot),
      TIME_ARGS (avg_wo_first.shot_to_shot),
      TIME_ARGS (target_shot_to_shot),
      TIMEDIFF_ARGS (TIME_DIFF (avg.shot_to_shot, target_shot_to_shot)));
  g_print ("Shot to save          | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIME_FORMAT " | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIMEDIFF_FORMAT "\n",
      TIME_ARGS (min.shot_to_save), TIME_ARGS (max.shot_to_save),
      TIME_ARGS (avg.shot_to_save),
      TIME_ARGS (avg_wo_first.shot_to_save),
      TIME_ARGS (target_shot_to_save),
      TIMEDIFF_ARGS (TIME_DIFF (avg.shot_to_save, target_shot_to_save)));
  g_print ("Shot to snapshot      | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIME_FORMAT " | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIMEDIFF_FORMAT "\n",
      TIME_ARGS (min.shot_to_snapshot),
      TIME_ARGS (max.shot_to_snapshot),
      TIME_ARGS (avg.shot_to_snapshot),
      TIME_ARGS (avg_wo_first.shot_to_snapshot),
      TIME_ARGS (target_shot_to_snapshot),
      TIMEDIFF_ARGS (TIME_DIFF (avg.shot_to_snapshot,
              target_shot_to_snapshot)));
  g_print ("Preview to precapture | %" TIME_FORMAT " | %" TIME_FORMAT " | %"
      TIME_FORMAT " | %" TIME_FORMAT " | %" TIME_FORMAT " | %" TIMEDIFF_FORMAT
      "\n", TIME_ARGS (min.preview_to_precapture),
      TIME_ARGS (max.preview_to_precapture),
      TIME_ARGS (avg.preview_to_precapture),
      TIME_ARGS (avg_wo_first.preview_to_precapture),
      TIME_ARGS (target_preview_to_precapture),
      TIMEDIFF_ARGS (TIME_DIFF (avg.preview_to_precapture,
              target_preview_to_precapture)));
  g_print ("Shot to buffer        | %" TIME_FORMAT " | %" TIME_FORMAT " | %"
      TIME_FORMAT " | %" TIME_FORMAT " | %" TIME_FORMAT " | %" TIMEDIFF_FORMAT
      "\n", TIME_ARGS (min.shot_to_buffer), TIME_ARGS (max.shot_to_buffer),
      TIME_ARGS (avg.shot_to_buffer), TIME_ARGS (avg_wo_first.shot_to_buffer),
      TIME_ARGS (target_shot_to_buffer),
      TIMEDIFF_ARGS (TIME_DIFF (avg.shot_to_buffer, target_shot_to_buffer)));
}

bool cameraWidgets::isReady() const
{
    //it's possible to use QCamera without any viewfinder attached
    return TRUE;//!m_viewfinderInterface || m_viewfinderInterface->isReady();
}

void cameraWidgets::setupViewfinder(QObject * viewfinder) {
    /*if (m_viewfinderInterface)
        m_viewfinderInterface->stopRenderer();

    m_viewfinderInterface = qobject_cast<QGstreamerVideoRendererInterface*>(viewfinder);
    if (!m_viewfinderInterface)
        viewfinder = 0;

    qDebug() << "viewfinder isReady:" << isReady();

    if (m_viewfinder != viewfinder) {
        bool oldReady = isReady();

        if (m_viewfinder) {
            disconnect(m_viewfinder, SIGNAL(sinkChanged()),
                       this, SLOT(handleViewfinderChange()));
            disconnect(m_viewfinder, SIGNAL(readyChanged(bool)),
                       this, SIGNAL(readyChanged(bool)));

            m_busHelper->removeMessageFilter(m_viewfinder);
        }

        m_viewfinder = viewfinder;
        m_viewfinderHasChanged = true;

        if (m_viewfinder) {
            connect(m_viewfinder, SIGNAL(sinkChanged()),
                       this, SLOT(handleViewfinderChange()));
            connect(m_viewfinder, SIGNAL(readyChanged(bool)),
                    this, SIGNAL(readyChanged(bool)));

            m_busHelper->installMessageFilter(m_viewfinder);
        }

        emit viewfinderChanged();
        if (oldReady != isReady())
            emit readyChanged(isReady());
    }*/
}

void cameraWidgets::init()
{
    gchar *target_times = NULL;
    gchar *ev_option = NULL;
    gchar *fn_option = NULL;

    GOptionEntry options[] = {
    {"ev-compensation", '\0', 0, G_OPTION_ARG_STRING, &ev_option,
        "EV compensation for source element GstPhotography interface", NULL},
    {"aperture", '\0', 0, G_OPTION_ARG_INT, &aperture,
          "Aperture (size of lens opening) for source element GstPhotography interface",
        NULL},
    {"flash-mode", '\0', 0, G_OPTION_ARG_INT,
          &flash_mode,
        "Flash mode for source element GstPhotography interface", NULL},
    {"scene-mode", '\0', 0, G_OPTION_ARG_INT,
          &scene_mode,
        "Scene mode for source element GstPhotography interface", NULL},
    {"exposure", '\0', 0, G_OPTION_ARG_INT64,
          &exposure,
          "Exposure time (in ms) for source element GstPhotography interface",
        NULL},
    {"iso-speed", '\0', 0, G_OPTION_ARG_INT,
          &iso_speed,
        "ISO speed for source element GstPhotography interface", NULL},
    {"white-balance-mode", '\0', 0, G_OPTION_ARG_INT,
          &wb_mode,
        "White balance mode for source element GstPhotography interface", NULL},
    {"colour-tone-mode", '\0', 0, G_OPTION_ARG_INT,
          &color_mode,
        "Colour tone mode for source element GstPhotography interface", NULL},
    {"directory", '\0', 0, G_OPTION_ARG_STRING, &fn_option,
        "Directory for capture file(s) (default is current directory)", NULL},
    {"mode", '\0', 0, G_OPTION_ARG_INT, &mode,
        "Capture mode (default = 1 (image), 2 = video)", NULL},
    {"capture-time", '\0', 0, G_OPTION_ARG_INT,
          &capture_time,
        "Time to capture video in seconds (default = 10)", NULL},
    {"capture-total", '\0', 0, G_OPTION_ARG_INT, &capture_total,
        "Total number of captures to be done (default = 1)", NULL},
    {"zoom", '\0', 0, G_OPTION_ARG_INT, &zoom,
        "Zoom (100 = 1x (default), 200 = 2x etc.)", NULL},
    {"wrapper-source", '\0', 0, G_OPTION_ARG_STRING, &wrappersrc_name,
          "Camera source wrapper used for setting the video source (default is wrappercamerabinsrc)",
        NULL},
    {"video-source", '\0', 0, G_OPTION_ARG_STRING, &videosrc_name,
        "Video source used in still capture and video recording", NULL},
    {"video-device", '\0', 0, G_OPTION_ARG_STRING, &videodevice_name,
        "Video device to be set on the video source", NULL},
    {"audio-source", '\0', 0, G_OPTION_ARG_STRING, &audiosrc_name,
        "Audio source used in video recording", NULL},
    {"image-pp", '\0', 0, G_OPTION_ARG_STRING, &imagepp_name,
        "List of image post-processing elements separated with comma", NULL},
    {"viewfinder-sink", '\0', 0, G_OPTION_ARG_STRING, &vfsink_name,
        "Viewfinder sink (default = fakesink)", NULL},
    {"image-width", '\0', 0, G_OPTION_ARG_INT, &image_width,
        "Width for image capture", NULL},
    {"image-height", '\0', 0, G_OPTION_ARG_INT, &image_height,
        "Height for image capture", NULL},
    {"view-framerate-num", '\0', 0, G_OPTION_ARG_INT, &view_framerate_num,
        "Framerate numerator for viewfinder", NULL},
    {"view-framerate-den", '\0', 0, G_OPTION_ARG_INT, &view_framerate_den,
        "Framerate denominator for viewfinder", NULL},
    {"preview-caps", '\0', 0, G_OPTION_ARG_STRING, &preview_caps_name,
        "Preview caps (e.g. video/x-raw-rgb,width=320,height=240)", NULL},
    {"viewfinder-filter", '\0', 0, G_OPTION_ARG_STRING, &viewfinder_filter,
        "Filter to process all frames going to viewfinder sink", NULL},
    {"x-width", '\0', 0, G_OPTION_ARG_INT, &x_width,
        "X window width (default = 320)", NULL},
    {"x-height", '\0', 0, G_OPTION_ARG_INT, &x_height,
        "X window height (default = 240)", NULL},
    {"no-xwindow", '\0', 0, G_OPTION_ARG_NONE, &no_xwindow,
        "Do not create XWindow", NULL},
    {"encoding-target", '\0', 0, G_OPTION_ARG_STRING, &gep_targetname,
        "Video encoding target name", NULL},
    {"encoding-profile", '\0', 0, G_OPTION_ARG_STRING, &gep_profilename,
        "Video encoding profile name", NULL},
    {"encoding-profile-filename", '\0', 0, G_OPTION_ARG_STRING, &gep_filename,
        "Video encoding profile filename", NULL},
    {"image-capture-caps", '\0', 0,
          G_OPTION_ARG_STRING, &image_capture_caps_str,
        "Image capture caps (e.g. video/x-raw-rgb,width=640,height=480)", NULL},
    {"viewfinder-caps", '\0', 0, G_OPTION_ARG_STRING,
          &viewfinder_caps_str,
        "Viewfinder caps (e.g. video/x-raw-rgb,width=640,height=480)", NULL},
    {"video-capture-caps", '\0', 0,
          G_OPTION_ARG_STRING, &video_capture_caps_str,
        "Video capture caps (e.g. video/x-raw-rgb,width=640,height=480)", NULL},
    {"audio-capture-caps", '\0', 0,
          G_OPTION_ARG_STRING, &audio_capture_caps_str,
          "Audio capture caps (e.g. audio/x-raw-int,width=16,depth=16,rate=44100,channels=2)",
        NULL},
    {"performance-measure", '\0', 0,
          G_OPTION_ARG_NONE, &performance_measure,
          "If performance information should be printed at the end of execution",
        NULL},
    {"performance-targets", '\0', 0,
          G_OPTION_ARG_STRING, &performance_targets_str,
          "Comma separated list of doubles representing the target values in "
          "seconds. The order is: startup time, change mode time, shot to save"
          ", shot to snapshot, shot to shot, preview to shot, shot to buffer. "
          "e.g. 3.5,1.0,5.0,2.5,5.0,1.5,1.0",
        NULL},
    {"flags", '\0', 0, G_OPTION_ARG_STRING, &camerabin_flags,
        "camerabin element flags (default = 0)", NULL},
    {NULL}
  };

    GST_DEBUG_CATEGORY_INIT (camerabin_test, "camerabin-test", 0,
      "camerabin test");
    
    vfsink_name = g_strdup ("kmssink"); //default fakesink / waylandsink /autovideosink
    preview_caps_name = g_strdup ("video/x-raw,format=NV12,width=640,height=480");
    filename = g_string_new ("/mnt/sdcard");
    videosrc_name = g_strdup ("v4l2src");
    viewfinder_caps_str = g_strdup ("video/x-raw,format=RGBx,width=640,height=480");
    image_width = 640;
    image_height = 480;
    
//    m_capture = new QPushButton(mode == MODE_IMAGE ? "Take Picture" : "start Recorder", this);
    //m_capture->setFixedSize(300, 100);

//    m_mode = new QPushButton(mode == MODE_IMAGE ? "Capture Image Mode" : "Video Recorder Mode", this);
    //m_mode->setFixedSize(300, 100);

    m_previewWid = new cameraPreviewwidgets(this);
    //m_leftWid->setFixedSize(640, 480);
    

#ifndef CIF_CAMERA
    connect(m_capture, SIGNAL(released()),this,SLOT(slot_released()));
    connect(m_capture, SIGNAL(clicked()),this,SLOT(slot_capture()));

    connect(m_mode, SIGNAL(pressed()),this,SLOT(slot_pressed()));
    connect(m_mode, SIGNAL(clicked()),this,SLOT(slot_modeswitch()));
#endif
}

void cameraWidgets::initLayout()
{
    QVBoxLayout *vmainlyout = new QVBoxLayout;

    m_topWid = new cameraTopWidgets(this);

    // 下半部分布局
    QVBoxLayout *hmiddlelyout = new QVBoxLayout;

    hmiddlelyout->addWidget(m_previewWid,1);
    
    QHBoxLayout *lyout2 = new QHBoxLayout;
    lyout2->addWidget(m_capture,1);
    lyout2->addWidget(m_mode,1);
    lyout2->setContentsMargins(10,10,10,10);
    lyout2->setSpacing(10);
    hmiddlelyout->addLayout(lyout2,0);

    vmainlyout->addWidget(m_topWid,0);
    vmainlyout->addLayout(hmiddlelyout,1);
    vmainlyout->setContentsMargins(0,0,0,0);
    vmainlyout->setSpacing(0);

    setLayout(vmainlyout);
}
