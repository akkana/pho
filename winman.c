/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * xstuff.c: assorted X11-specific routines for pho.
 *
 * Most of this comes from 
 * http://www.opensource.apple.com/source/gcc/gcc-5483/libjava/jni/gtk-peer/gnu_java_awt_peer_gtk_GtkWindowPeer.c
 *
 * To build as a standalone demo:
gcc `pkg-config --cflags --libs gtk+-2.0` -o winman -DSTANDALONE=1 winman.c
 */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

static Atom extents_atom = 0;

static Bool
property_notify_predicate (Display *xdisplay __attribute__((unused)),
                           XEvent  *event,
                           XPointer window_id)
{
  unsigned long *window = (unsigned long *) window_id;

  if (event->xany.type == PropertyNotify
      && event->xany.window == *window
      && event->xproperty.atom == extents_atom)
    return True;
  else
    return False;
}

/* Requests that the window manager set window's
   _NET_FRAME_EXTENTS property. */
void
request_frame_extents (GtkWidget *window)
{
  const char *request_str = "_NET_REQUEST_FRAME_EXTENTS";
  GdkAtom request_extents = gdk_atom_intern (request_str, FALSE);

  /* Check if the current window manager supports
     _NET_REQUEST_FRAME_EXTENTS. */
  if (gdk_net_wm_supports (request_extents))
    {
      GdkDisplay *display = gtk_widget_get_display (window);
      Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);

      GdkWindow *root_window = gdk_get_default_root_window ();
      Window xroot_window = GDK_WINDOW_XID (root_window);

      Atom extents_request_atom =
          gdk_x11_get_xatom_by_name_for_display (display, request_str);

      XEvent xevent;
      XEvent notify_xevent;

      /* This doesn't work if window==GWin */
      unsigned long window_id = GDK_WINDOW_XID (GDK_DRAWABLE(window->window));

      if (!extents_atom)
      {
          const char *extents_str = "_NET_FRAME_EXTENTS";
          extents_atom =
              gdk_x11_get_xatom_by_name_for_display (display, extents_str);
      }

      xevent.xclient.type = ClientMessage;
      xevent.xclient.message_type = extents_request_atom;
      xevent.xclient.display = xdisplay;
      xevent.xclient.window = window_id;
      xevent.xclient.format = 32;
      xevent.xclient.data.l[0] = 0;
      xevent.xclient.data.l[1] = 0;
      xevent.xclient.data.l[2] = 0;
      xevent.xclient.data.l[3] = 0;
      xevent.xclient.data.l[4] = 0;

      XSendEvent (xdisplay, xroot_window, False,
		  (SubstructureRedirectMask | SubstructureNotifyMask),
                  &xevent);

      XIfEvent(xdisplay, &notify_xevent,
	       property_notify_predicate, (XPointer) &window_id);
    }
}

#ifdef STANDALONE
void
window_get_frame_extents (GtkWidget *window, int *width, int *height);

int gDebug = 1;

static gint HandleExpose(GtkWidget* widget, GdkEventExpose* event)
{
    gint width, height;
    gdk_drawable_get_size(widget->window, &width, &height);

    printf("HandleExpose: area %dx%d +%d+%d in window %dx%d\n",
           event->area.width, event->area.height,
           event->area.x, event->area.y,
           width, height);
    if (event->area.x != 0 || event->area.y != 0) {
        if (event->area.width != width || event->area.height != height)
            printf("*** Expose different from window size!\n");
    }
}

int main(int argc, char** argv)
{
    int gMonitorWidth, gMonitorHeight;

    gint sFrameWidth = -1;
    gint sFrameHeight = -1;
    static GdkColor sBlack;

    GtkWidget *gWin = 0;
    static GtkWidget *sDrawingArea = 0;

    gtk_init(&argc, &argv);
    gMonitorWidth = gdk_screen_width();
    gMonitorHeight = gdk_screen_height();

    gWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);

//    gtk_signal_connect(GTK_OBJECT(gWin), "key_press_event",
//                       (GtkSignalFunc)HandleGlobalKeys, 0);
    sDrawingArea = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(gWin), sDrawingArea);
    gtk_widget_show(sDrawingArea);

    gtk_drawing_area_size(GTK_DRAWING_AREA(sDrawingArea),
                          gMonitorWidth, gMonitorHeight);
    gtk_window_unfullscreen(GTK_WINDOW(gWin));

    gtk_signal_connect(GTK_OBJECT(sDrawingArea), "expose_event",
                       (GtkSignalFunc)HandleExpose, 0);
    gtk_widget_show(gWin);

    int width, height;
    window_get_frame_extents (gWin, &sFrameWidth, &sFrameHeight);
    printf("Frame size: %d, %d\n", sFrameWidth, sFrameHeight);

    gtk_window_resize(GTK_WINDOW(gWin),
                      gMonitorWidth-sFrameWidth, gMonitorHeight-sFrameHeight);
    printf("Resizing after frame extents to %dx%d\n",
           gMonitorWidth-sFrameWidth, gMonitorHeight-sFrameHeight);
}
#endif

/* Try to find the size of the window decorations,
 * using _NET_REQUEST_FRAME_EXTENTS
 * gdk_window_get_frame_extents doesn't work until after the
 * window is realized, by which time it's too late.
 * This method works once the window is mapped but not realized.
 */
void
window_get_frame_extents (GtkWidget *window, int *width, int *height)
{
  unsigned long *extents = NULL;
  extern int gDebug;

  /* I'm not sure whether is_visible or is_viewable is better here.
   * is_viewable requires that the window and its ancestors be mapped;
   * is_visible, just the window. Unfortunately, there's no documentation
   * on what's needed to make gdk_window_get_frame_extents() work.
   */
  if (gdk_window_is_viewable(window->window)) {
      GdkRectangle rect;
      gint winwidth, winheight;

      gdk_drawable_get_size(window->window, &winwidth, &winheight);
      gdk_window_get_frame_extents(window->window, &rect);
      *width = rect.width - winwidth;
      *height = rect.height - winheight;

      if (gDebug)
          printf("get_frame_extents from visible window: %dx%d\n",
                 *width, *height);
      return;
  }

  /* Otherwise, the window isn't visible, so maybe we'll be able to
   * get the X property (which just hangs if the window already exists).
   */
  if (gDebug)
      printf("Window isn't visible; requestiong NET_FRAME_EXTENTS\n");

  /* Guess frame extents in case _NET_FRAME_EXTENTS is not
     supported. */
  *height = 29;
  *width = 12;

  /* Request that the window manager set window's
     _NET_FRAME_EXTENTS property.
  */
  request_frame_extents (window);

  /* Attempt to retrieve window's frame extents. */
  if (gdk_property_get (window->window,
                        gdk_atom_intern ("_NET_FRAME_EXTENTS", FALSE),
                        gdk_atom_intern ("CARDINAL", FALSE),
                        0,
                        sizeof (unsigned long) * 4,
                        FALSE,
                        NULL,
                        NULL,
                        NULL,
                        (guchar**)&extents))
    {
      *width = extents[0] + extents[1];
      *height = extents[2] + extents[3];
    }
}
