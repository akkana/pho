/*
 * gmain.c: gtk main routines for pho, an image viewer.
 *
 * Copyright 2004 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include "pho.h"
#include "dialogs.h"
#include "exif/phoexif.h"

#include <stdlib.h>       // for getenv()
#include <stdio.h>
#include <string.h>

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

/* Some window managers don't deal well with windows that resize,
 * or don't retain focus if a resized window no longer contains
 * the mouse pointer.
 * Make new windows instead, by default.
 */
int gMakeNewWindows = 0;

/* gtk/X related window attributes */
static GtkWidget *sWin = 0;
static GtkWidget *sDrawingArea = 0;

/* DrawImage is called from the expose callback.
 * It assumes we already have the image in gImage.
 */
void DrawImage()
{
    int dstX = 0, dstY = 0;
    char title[BUFSIZ];
#   define TITLELEN ((sizeof title) / (sizeof *title))

    if (gImage == 0 || sWin == 0 || sDrawingArea == 0) return;

    /* If we're in presentation mode, clear the screen first: */
    if (gPresentationMode) {
        gdk_window_clear(sDrawingArea->window);

        /* Center the image */
        dstX = (gMonitorWidth - gCurImage->curWidth) / 2;
        dstY = (gMonitorHeight - gCurImage->curHeight) / 2;
    }

    gdk_pixbuf_render_to_drawable(gImage, sDrawingArea->window,
                   sDrawingArea->style->fg_gc[GTK_WIDGET_STATE(sDrawingArea)],
                                  0, 0, dstX, dstY,
                                  gCurImage->curWidth, gCurImage->curHeight,
                                  GDK_RGB_DITHER_NONE, 0, 0);

    // Update the titlebar
    sprintf(title, "pho: %s (%d x %d)", gCurImage->filename,
            gCurImage->trueWidth, gCurImage->trueHeight);
    if (HasExif())
    {
        const char* date = ExifGetString(ExifDate);
        if (date && date[0]) {
            /* Make sure there's room */
            if (strlen(title) + strlen(date) + 3 < TITLELEN)
            strcat(title, " (");
            strcat(title, date);
            strcat(title, ")");
        }
    }
    gtk_window_set_title(GTK_WINDOW(sWin), title);

    UpdateInfoDialog(gCurImage);
}

static gint HandleExpose(GtkWidget* widget, GdkEventExpose* event)
{
    DrawImage();

#if GTK_MAJOR_VERSION == 2
    /* Make sure the window can resize smaller, later */
    if (!gMakeNewWindows)
        gtk_widget_set_size_request(GTK_WIDGET(sWin), 1, 1);
#endif

    return TRUE;
}

void EndSession()
{
    PrintNotes();
    gtk_main_quit();
}

static gint HandleDelete(GtkWidget* widget, GdkEventKey* event, gpointer data)
{
    /* If you return FALSE in the "delete_event" signal handler,
     * GTK will emit the "destroy" signal. Returning TRUE means
     * you don't want the window to be destroyed.
     * This is useful for popping up 'are you sure you want to quit?'
     * type dialogs. */
    EndSession();
    return TRUE;
}

#if 0
/* Called each time a window goes away. */
static gint HandleDestroy(GtkWidget* widget, GdkEventKey* event, gpointer data)
{
    return TRUE;
}
#endif

static gint HandleKeyPress(GtkWidget* widget, GdkEventKey* event)
{
    static double scale = 1.;
    switch (event->keyval)
    {
      case GDK_d:
          DeleteImage(gCurImage);
          break;
      case GDK_space:
          if (NextImage() != 0) {
              if (Prompt("Quit pho?", "Quit", "Continue", "qx \n", "c ") != 0)
                  EndSession();
          }
          return TRUE;
      case GDK_BackSpace:
      case GDK_minus:
          PrevImage();
          return TRUE;
      case GDK_Home:
          gCurImage = gFirstImage;
          NextImage();
          return TRUE;
      case GDK_F:
          if (gScaleMode != PHO_SCALE_FULLSIZE)
              gScaleMode = PHO_SCALE_FULLSIZE;
          else
              gScaleMode = PHO_SCALE_NORMAL;
          ShowImage();
          return TRUE;
      case GDK_f:
          if (gScaleMode != PHO_SCALE_FULLSCREEN)
              gScaleMode = PHO_SCALE_FULLSCREEN;
          else
              gScaleMode = PHO_SCALE_NORMAL;
          ShowImage();
          return TRUE;
      case GDK_p:
          gPresentationMode = !gPresentationMode;
          PrepareWindow();
          return TRUE;
      case GDK_0:
      case GDK_1:
      case GDK_2:
      case GDK_3:
      case GDK_4:
      case GDK_5:
      case GDK_6:
      case GDK_7:
      case GDK_8:
      case GDK_9:
          ToggleNoteFlag(gCurImage, event->keyval - GDK_0);
          return TRUE;
      case GDK_t:   // make life easier for xv users
      case GDK_r:
      case GDK_Right:
      case GDK_KP_Right:
          RotateImage(90);
          return TRUE;
      case GDK_T:   // make life easier for xv users
      case GDK_R:
      case GDK_l:
      case GDK_L:
      case GDK_Left:
      case GDK_KP_Left:
          RotateImage(-90);
          return TRUE;
      case GDK_Up:
          RotateImage(180);
          return TRUE;
      case GDK_plus:
      case GDK_KP_Add:
      case GDK_equal:
          if (gScaleMode != PHO_SCALE_ABSSIZE) scale = 1.;
          gScaleMode = PHO_SCALE_ABSSIZE;
          scale *= 2;
          gCurImage->curWidth = gCurImage->trueWidth * scale;
          gCurImage->curHeight = gCurImage->trueHeight * scale;
          ShowImage();
          return TRUE;
      case GDK_slash:
      case GDK_KP_Subtract:
          if (gScaleMode != PHO_SCALE_ABSSIZE) scale = 1.;
          gScaleMode = PHO_SCALE_ABSSIZE;
          scale /= 2;
          gCurImage->curWidth = gCurImage->trueWidth * scale;
          gCurImage->curHeight = gCurImage->trueHeight * scale;
          if (gCurImage->curWidth < 1) gCurImage->curWidth = 1;
          if (gCurImage->curHeight < 1) gCurImage->curHeight = 1;
          ShowImage();
          return TRUE;
#if 0
      case GDK_g:  // start gimp
          if ((i = CallExternal("gimp-remotte -n", ArgV[ArgP])) != 0) {
              i = CallExternal("gimp", ArgV[ArgP]);
              printf("Called gimp, returned %d\n", i);
          }
          else printf("Called gimp-remote, returned %d\n", i);
          break;
#endif
      case GDK_i:
          ToggleInfo();
          return TRUE;
      case GDK_Escape:
      case GDK_q:
          EndSession();
          return TRUE;
      default:
          if (gDebug)
              printf("Don't know key 0x%lu\n", (unsigned long)(event->keyval));
          return FALSE;
    }
    // Keep gcc 2.95 happy:
    return FALSE;
}

/* Make a new window, destroying the old one. */
static void NewWindow()
{
    gint root_x = -1;
    gint root_y = -1;
    if (sWin) {
#if GTK_MAJOR_VERSION == 2
        gtk_window_get_position(GTK_WINDOW(sWin), &root_x, &root_y);
#else
        gdk_window_get_position(sWin->window, &root_x, &root_y);
#endif
        gtk_object_destroy(GTK_OBJECT(sWin));
    }

    sWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    
    /* Window manager delete */
    gtk_signal_connect(GTK_OBJECT(sWin), "delete_event",
                       (GtkSignalFunc)HandleDelete, 0);

    /* This event occurs when we call gtk_widget_destroy() on the window,
     * or if we return FALSE in the "delete_event" callback.
    gtk_signal_connect(GTK_OBJECT(sWin), "destroy",
                       (GtkSignalFunc)HandleDestroy, 0);
     */

    /* KeyPress events on the drawing area don't come through --
     * they have to be on the window.
     */
    gtk_signal_connect(GTK_OBJECT(sWin), "key_press_event",
                       (GtkSignalFunc)HandleKeyPress, 0);

    sDrawingArea = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(sWin), sDrawingArea);
    gtk_widget_show(sDrawingArea);

    if (gPresentationMode) {
        gtk_drawing_area_size(GTK_DRAWING_AREA(sDrawingArea),
                              gMonitorWidth, gMonitorHeight);
#if GTK_MAJOR_VERSION == 2
        gtk_window_fullscreen(GTK_WINDOW(sWin));
        /* XXX Would like to set the background color to black here.  How?
         * gtk_style documentation shows how to set a window's background
         * to a style, but it doesn't say how to make a style be "black".
         */
#endif
    }
    else
        gtk_drawing_area_size(GTK_DRAWING_AREA(sDrawingArea),
                              gCurImage->curWidth, gCurImage->curHeight);

#if 0
    /* This doesn't seem to make any difference. */
    gtk_widget_set_events(sDrawingArea,
                          GDK_EXPOSURE_MASK
                          | GDK_KEY_PRESS_MASK
                          | GDK_STRUCTURE_MASK );
#endif

    gtk_signal_connect(GTK_OBJECT(sDrawingArea), "expose_event",
                       (GtkSignalFunc)HandleExpose, 0);

    /* Request a window position, based on the image size
     * and the position of the previous window.
     */
    if (root_x >= 0 && root_y >= 0) {
        if (root_x + gCurImage->curWidth > gMonitorWidth
            && gCurImage->curWidth <= gMonitorWidth)
            root_x = gMonitorWidth - gCurImage->curWidth;
        if (root_y + gCurImage->curHeight > gMonitorHeight
            && gCurImage->curHeight <= gMonitorHeight)
            root_y = gMonitorHeight - gCurImage->curHeight;

        gdk_window_move(sWin->window, root_x, root_y);
    }

    gtk_widget_show(sWin);
}

/* PrepareWindow is responsible for making the window the right
 * size and position, so the user doesn't see flickering.
 * It may actually make a new window, or it may just resize and/or
 * reposition the existing window.
 */
void PrepareWindow()
{
    gint x, y, nx = -1, ny = -1;

    if (gMakeNewWindows || sWin == 0) {
        NewWindow();
        return;
    }

    /* Otherwise, resize and reposition the current window. */

    if (gPresentationMode) {
        gtk_drawing_area_size(GTK_DRAWING_AREA(sDrawingArea),
                              gMonitorWidth, gMonitorHeight);
#if GTK_MAJOR_VERSION == 2
        gtk_window_fullscreen(GTK_WINDOW(sWin));
#endif
    }
    else {
#if GTK_MAJOR_VERSION == 2
        gtk_window_unfullscreen(GTK_WINDOW(sWin));
        /* We need to size the actual window, not just the drawing area.
         * Resizing the drawing area will resize the window for some
         * window managers, like openbook, but not for metacity.
         */
        gtk_window_resize(GTK_WINDOW(sWin),
                          gCurImage->curWidth, gCurImage->curHeight);
#else
        gdk_window_resize(sWin->window,
                          gCurImage->curWidth, gCurImage->curHeight);
#endif

        /* If we  resized, see if we need to move: */
        gdk_window_get_position(sWin->window, &x, &y);

        if (x + gCurImage->curWidth >= gMonitorWidth)
            nx = gMonitorWidth - gCurImage->curWidth;
        if (y + gCurImage->curHeight >= gMonitorHeight)
            ny = gMonitorHeight - gCurImage->curHeight;
        if (x >= 0 || y >= 0) {
            gdk_window_move(sWin->window, (nx >= 0 ? nx : x),
                            (ny >= 0 ? ny : y));
            //gtk_widget_set_uposition(GTK_WIDGET(sWin), (nx >= 0 ? nx : x), (ny >= 0 ? ny : y));
        }
    }

    /* Request the focus.
     * Neither gtk_window_present nor gdk_window_focus seem to work.
     */
#if GTK_MAJOR_VERSION == 2
    gtk_window_present(GTK_WINDOW(sWin));
    gdk_window_focus(sWin->window, 0);
#endif

    DrawImage();
}

int main(int argc, char** argv)
{
    while (argc > 1)
    {
        if (argv[1][0] == '-') {
            if (argv[1][1] == 'd')
                gDebug = 1;
            else if (argv[1][1] == 'h')
                Usage();
            else if (argv[1][1] == 'v')
                VerboseHelp();
            else if (argv[1][1] == 'n')
                gMakeNewWindows = 1;
            else if (argv[1][1] == 'p')
                gPresentationMode = 1;
            else Usage();
        }
        else {
            PhoImage* img = NewPhoImage(argv[1]);
            PhoImage* lastImg;
            if (!img) {
                fprintf(stderr, "Out of memory!\n");
                exit(1);
            }
            /* Make img the new last image in the list */
            if (gFirstImage == 0)
                gFirstImage = img;
            else {
                lastImg = gFirstImage->prev;
                if (lastImg == 0) {
                    gFirstImage->next = img;
                    img->prev = gFirstImage;
                }
                else {
                    lastImg->next = img;
                    img->prev = lastImg;
                }
                gFirstImage->prev = img;
                img->next = gFirstImage;
            }
        }
        --argc;
        ++argv;
    }

    if (gFirstImage == 0)
        Usage();

    /* See http://www.gtk.org/tutorial */
    gtk_init(&argc, &argv);

    /* Must init rgb system explicitly, else we'll crash
     * in the first gdk_pixbuf_render_to_drawable(),
     * calling gdk_draw_rgb_image_dithalign():
     * (Is this still true in gtk2?)
     */
    gdk_rgb_init();

    gMonitorWidth = gdk_screen_width();
    gMonitorHeight = gdk_screen_height();

    /* Load the first image */
    if (NextImage() != 0)
        exit(1);

    gtk_main();
    return 0;
}

