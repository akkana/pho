/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Test of focus.
 *
 * This program tests whether focus reverts to another window when
 * the focused window resizes out from under the cursor.
 * Some window managers have this bug, while others don't.
 * It only matters when you're using pointer focus, not click-to-type.
 *
 * It also tests whether gtk_window_present() can take the focus
 * for window managers that have this bug. See the call to
 * gtk_window_present() in line 62.
 *
 * To test:
 *   1. Make sure you have pointer focus set.
 *   2. Compile, then run focustest.
 *   3. Spacebar will toggle between portrait and landscape shaped windows.
 *      Toggle a few times to see.
 *   4. Put your mouse in a place where it's in the window, but once
 *      the window resizes the mouse will be outside the window and
 *      in another window underneath.
 *   5. Hit spacebar to toggle, and see if the window loses focus.
 *
 *   If it does lose focus, then uncomment the call to gtk_window_present()
 *   (line 62) and try again. See if it's any better.
 *
 * Copyright 2007 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 *
 * Sample compile line:
 * cc -g -Wall -I/usr/include/gtk-2.0 -I/usr/lib/gtk-2.0/include -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -o focustest focustest.c -lgtk-x11-2.0 -lgdk-x11-2.0 -lX11
 */

#include <stdlib.h>       /* for getenv() */
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include <gdk/gdkx.h>    /* for gdk_x11_display_grab */

GtkWidget *gWin = 0;
static GtkWidget *sDrawingArea = 0;
static gint gMonitorWidth, gMonitorHeight;

gint HandleKeys(GtkWidget* widget, GdkEventKey* event)
{
    gint width, height;
    switch (event->keyval)
    {
      case GDK_space:  /* swap dimensions */
          gdk_drawable_get_size(sDrawingArea->window, &width, &height);
          gtk_window_resize(GTK_WINDOW(gWin), height, width);

          /* The next line tries to set focus explicitly.
           * Comment it out if you're just trying to find out
           * whether your windowmanager has the bug.
           */
          //gtk_window_present(GTK_WINDOW(gWin));
          return TRUE;
      case GDK_q:
        exit(0);
    }
    return FALSE;
}

int main(int argc, char** argv)
{
    gtk_init(&argc, &argv);

    gMonitorWidth = gdk_screen_width();
    gMonitorHeight = gdk_screen_height();

    /* Make it possible to resize smaller as well as larger: */
    gWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_widget_set_size_request(GTK_WIDGET(gWin), 1, 1);

    /* Window manager delete */
    gtk_signal_connect(GTK_OBJECT(gWin), "delete_event",
                       (GtkSignalFunc)exit, 0);

    /* This event occurs when we call gtk_widget_destroy() on the window,
     * or if we return FALSE in the "delete_event" callback.
    gtk_signal_connect(GTK_OBJECT(gWin), "destroy",
                       (GtkSignalFunc)HandleDestroy, 0);
     */

    /* KeyPress events on the drawing area don't come through --
     * they have to be on the window.
     */
    gtk_signal_connect(GTK_OBJECT(gWin), "key_press_event",
                       (GtkSignalFunc)HandleKeys, 0);

    sDrawingArea = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(gWin), sDrawingArea);
    gtk_widget_show(sDrawingArea);

    gtk_drawing_area_size(GTK_DRAWING_AREA(sDrawingArea), 800, 600);
    gtk_window_resize(GTK_WINDOW(gWin), 800, 600);

    gtk_widget_show(gWin);

    gtk_main();
    return 0;
}

