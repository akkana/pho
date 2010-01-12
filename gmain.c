/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * gmain.c: gtk main routines for pho, an image viewer.
 *
 * Copyright 2004 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include "pho.h"
#include "dialogs.h"
#include "exif/phoexif.h"

#include <stdlib.h>       /* for getenv() */
#include <stdio.h>
#include <string.h>

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include <gdk/gdkx.h>    /* for gdk_x11_display_grab */

/* Some window managers don't deal well with windows that resize,
 * or don't retain focus if a resized window no longer contains
 * the mouse pointer.
 * Offer an option to make new windows instead.
 */
int gMakeNewWindows = 0;

/* The size our window frame adds on the top left of the image */
gint sFrameWidth = -1;
gint sFrameHeight = -1;
gint sPhysMonitorWidth = 0;
gint sPhysMonitorHeight = 0;

static GdkColor sBlack;

/* gtk/X related window attributes */
static GtkWidget *sWin = 0;
static GtkWidget *sDrawingArea = 0;

static void NewWindow(); /* forward */

static void hide_cursor(GtkWidget* w)
{
#if GTK_MAJOR_VERSION == 2
    static char invisible_cursor_bits[] = { 0x0 };
    static GdkBitmap *empty_bitmap = 0;
    static GdkCursor* cursor = 0;

    if (empty_bitmap == 0 || cursor == 0) {
        empty_bitmap = gdk_bitmap_create_from_data(NULL,
                                                   invisible_cursor_bits,
                                                   1, 1);
        cursor = gdk_cursor_new_from_pixmap (empty_bitmap, empty_bitmap, &sBlack, &sBlack, 1, 1);
    }
    gdk_window_set_cursor(w->window, cursor);

    /* If we need to free this, do it this way:
    gdk_cursor_unref(cursor);
    g_object_unref(empty_bitmap);
     */
#endif
}

static void show_cursor(GtkWidget* w)
{
#if GTK_MAJOR_VERSION == 2
    gdk_window_set_cursor(w->window, NULL);
#endif
}    

static void AdjustScreenSize()
{
    if (gPresentationMode) {
        gMonitorWidth = sPhysMonitorWidth;
        gMonitorHeight = sPhysMonitorHeight;
    }
    else {
        gMonitorWidth = sPhysMonitorWidth - sFrameWidth;
        gMonitorHeight = sPhysMonitorHeight - sFrameHeight;
    }
}

/* DrawImage is called from the expose callback.
 * It assumes we already have the image in gImage.
 */
void DrawImage()
{
    int dstX = 0, dstY = 0;
    char title[BUFSIZ];
#   define TITLELEN ((sizeof title) / (sizeof *title))

    if (gImage == 0 || sWin == 0 || sDrawingArea == 0) return;

    if (gPresentationMode) {
        hide_cursor(sDrawingArea);

        /* Center the image */
        dstX = (sPhysMonitorWidth - gCurImage->curWidth) / 2;
        dstY = (sPhysMonitorHeight - gCurImage->curHeight) / 2;
    }
    else
        show_cursor(sDrawingArea);

    gdk_pixbuf_render_to_drawable(gImage, sDrawingArea->window,
                   sDrawingArea->style->fg_gc[GTK_WIDGET_STATE(sDrawingArea)],
                                  0, 0, dstX, dstY,
                                  gCurImage->curWidth, gCurImage->curHeight,
                                  GDK_RGB_DITHER_NONE, 0, 0);

    /* Update the titlebar */
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
    if (gScaleMode == PHO_SCALE_FULLSIZE)
        strcat(title, " (fullsize)");
    else if (gScaleMode == PHO_SCALE_FULLSCREEN)
        strcat(title, " (fullscreen)");
    gtk_window_set_title(GTK_WINDOW(sWin), title);

    UpdateInfoDialog(gCurImage);
}

static gint HandleExpose(GtkWidget* widget, GdkEventExpose* event)
{
    /* If we're in presentation mode, clear the screen first: */
    if (gPresentationMode) {
        gdk_window_clear(sDrawingArea->window);
    }

    DrawImage();

    /* Get the frame offset if we don't already have it */
    if (sFrameWidth < 0 || sFrameHeight < 0) {
        gint win_x, win_y, contents_x, contents_y;
        gdk_window_get_root_origin(sWin->window, &win_x, &win_y);
        gdk_window_get_position(sWin->window, &contents_x, &contents_y);
        sFrameWidth = contents_x - win_x;
        if (sFrameWidth < 0) sFrameWidth = 0;
        sFrameHeight = contents_y - win_y;
        if (sFrameHeight < 0) sFrameHeight = 0;
        AdjustScreenSize();
        /* Note that the first image will be slightly wrong,
         * since it didn't know about the frame size.  Oh, well!
         */
    }

    /* Make sure the window can resize smaller, later */
#if GTK_MAJOR_VERSION == 2
    if (!gMakeNewWindows)
        gtk_widget_set_size_request(GTK_WIDGET(sWin), 1, 1);
#endif

    return TRUE;
}

void EndSession()
{
    PrintNotes();
    gtk_main_quit();
    /* This doesn't always quit!  So make sure: */
    exit(0);
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

static gint HandleKeyPress(GtkWidget* widget, GdkEventKey* event)
{
    switch (event->keyval)
    {
      case GDK_d:
          DeleteImage(gCurImage);
          break;
      case GDK_space:
          if (NextImage() != 0) {
              if (Prompt("Quit pho?", "Quit", "Continue", "qx\n", "cn") != 0)
                  EndSession();
          }
          return TRUE;
      case GDK_BackSpace:
          PrevImage();
          return TRUE;
      case GDK_Home:
          gCurImage = 0;
          NextImage();
          return TRUE;
      case GDK_n:   /* Get out of any weird display modes */
          gScaleMode = PHO_SCALE_NORMAL;
          AdjustScreenSize();
          ScaleImage(gCurImage);
          ShowImage();
          return TRUE;
      case GDK_F:   /* Full size mode: show image bit-for-bit */
          if (gScaleMode != PHO_SCALE_FULLSIZE)
              gScaleMode = PHO_SCALE_FULLSIZE;
          else
              gScaleMode = PHO_SCALE_NORMAL;
          AdjustScreenSize();
          ScaleImage(gCurImage);
          ShowImage();
          return TRUE;
      case GDK_f:   /* Full screen mode: as big as possible on screen */
          if (gScaleMode != PHO_SCALE_FULLSCREEN)
              gScaleMode = PHO_SCALE_FULLSCREEN;
          else
              gScaleMode = PHO_SCALE_NORMAL;
          AdjustScreenSize();
          ScaleImage(gCurImage);
          ShowImage();
          return TRUE;
      case GDK_p:
          AdjustScreenSize();
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
      case GDK_t:   /* make life easier for xv users */
      case GDK_r:
      case GDK_Right:
      case GDK_KP_Right:
          RotateImage(gCurImage, 90);
          return TRUE;
      case GDK_T:   /* make life easier for xv users */
      case GDK_R:
      case GDK_l:
      case GDK_L:
      case GDK_Left:
      case GDK_KP_Left:
          RotateImage(gCurImage, -90);
          return TRUE;
      case GDK_Up:
      case GDK_Down:
          RotateImage(gCurImage, 180);
          return TRUE;
      case GDK_plus:
      case GDK_KP_Add:
      case GDK_equal:
          gScaleMode = PHO_SCALE_ABSSIZE;
          gCurImage->curWidth *= 2;
          gCurImage->curHeight *= 2;
          ScaleImage(gCurImage);
          ShowImage();
          return TRUE;
      case GDK_minus:
      case GDK_slash:
      case GDK_KP_Subtract:
          gScaleMode = PHO_SCALE_ABSSIZE;
          if (gCurImage->curWidth <= 2 || gCurImage->curHeight <= 2)
              return TRUE;
          gCurImage->curWidth /= 2;
          gCurImage->curHeight /= 2;
          ScaleImage(gCurImage);
          ShowImage();
          return TRUE;
      case GDK_g:  /* start gimp */
          system(strcat("gimp-remote ", gCurImage->filename));
#if 0
          if (CallExternal("gimp-remote -n", gCurImage->filename) != 0) {
              CallExternal("gimp", gCurImage->filename);
          }
#endif
          break;
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
    /* Keep gcc 2.95 happy: */
    return FALSE;
}

/* If we  resized, see if we might move off the screen.
 * If we're not over the mouse pointer, then we may lose focus
 * and so should move over the mouse pointer.
 * (It would be nice if window managers let us keep focus,
 * but many of them don't.)
 * But don't move if we're already bigger than the screen.
 */
static void MaybeMove()
{
/* Some functions, like gdk_get_default_root_window, don't exist in
 * gtk1, so this function would need to be backported.
 */
#if GTK_MAJOR_VERSION == 2
    gint x, y, nx, ny, w, h;
    gint mousex, mousey;
    GdkModifierType mask;

    if (gCurImage->curWidth > sPhysMonitorWidth
        || gCurImage->curHeight > sPhysMonitorHeight)
        return;

    /* If we don't have a window yet, don't move it */
    if (!sWin)
        return;

#if GTK_MAJOR_VERSION == 2
    gtk_window_get_position(GTK_WINDOW(sWin), &x, &y);
    gtk_window_get_size(GTK_WINDOW(sWin), &w, &h);
#else
    gdk_window_get_position(sWin->window, &x, &y);
    gdk_window_get_size(sWin->window, &w, &h);
#endif
    /* printf("Currently (%d, %d) %d x %d\n", x, y, w, h); */

    /* XXX If the window size hasn't changed, should probably return. */

    /* Try to center around the old center.
    nx = x + (w - gCurImage->curWidth) / 2;
    ny = y + (h - gCurImage->curHeight) / 2;
     */

    /* Get the mouse position */
    gdk_window_get_pointer(gdk_get_default_root_window(),
                           &mousex, &mousey, &mask);

    /* Start with the current position: don't move if we don't have to. */
    if (x > 0 && y > 0) {
        nx = x;
        ny = y;
    }
    /* If we don't have a current pos, start by centering over the mouse. */
    else {
        nx = mousex - w / 2;
        ny = mousey - h / 2;
    }

    /* Make sure it wonn't overflow off the screen */
    if (nx + gCurImage->curWidth > gMonitorWidth)
        nx = gMonitorWidth - gCurImage->curWidth;
    if (nx < 0)
        nx = 0;
    if (ny + gCurImage->curHeight > gMonitorHeight)
        ny = gMonitorHeight - gCurImage->curHeight;
    if (ny < 0)
        ny = 0;

    /* Make sure we still cover the mouse cursor (for focus). */
    /*
    printf("Mouse @ (%d, %d), our window will go from (%d, %d) - (%d, %d)\n",
           mousex, mousey, nx, ny,
           nx+gCurImage->curWidth, ny+gCurImage->curHeight);
     */
    if (mousex < nx)
        nx = mousex - 1;
    else if (mousex > nx+gCurImage->curWidth)
        nx = mousex - gCurImage->curWidth;
    if (mousey < ny)
        ny = mousey - 1;
    else if (mousey > ny+gCurImage->curHeight)
        ny = mousey - gCurImage->curHeight;
    /* printf("After mouse corrections, move to (%d, %d)\n", nx, ny); */
    if (x != nx || y != ny) {
        gtk_window_move(GTK_WINDOW(sWin), nx, ny);
    }
#endif /* GTK_MAJOR_VERSION == 2 */
}

/* Make a new window, destroying the old one. */
static void NewWindow()
{
    gint root_x = -1;
    gint root_y = -1;
    if (sWin) {
#if FOO_GTK_MAJOR_VERSION == 2
        gtk_window_get_position(GTK_WINDOW(sWin), &root_x, &root_y);
#else
        gdk_window_get_position(sWin->window, &root_x, &root_y);
#endif
        gtk_object_destroy(GTK_OBJECT(sWin));
    }

    sWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_wmclass(GTK_WINDOW(sWin), "pho", "Pho");

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
#if GTK_MAJOR_VERSION == 2
    /* This can't be done in expose: it causes one of those extra
     * spurious expose events that gtk so loves.
     */
    gtk_widget_modify_bg(sDrawingArea, GTK_STATE_NORMAL, &sBlack);
#endif

    if (gPresentationMode) {
        gtk_drawing_area_size(GTK_DRAWING_AREA(sDrawingArea),
                              sPhysMonitorWidth, sPhysMonitorHeight);
#if GTK_MAJOR_VERSION == 2
        gtk_window_fullscreen(GTK_WINDOW(sWin));
#endif
    }
    else {
        gtk_drawing_area_size(GTK_DRAWING_AREA(sDrawingArea),
                              gCurImage->curWidth, gCurImage->curHeight);
#if GTK_MAJOR_VERSION == 2
        gtk_window_unfullscreen(GTK_WINDOW(sWin));
#endif
    }

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
    MaybeMove();
#if 0
    if (root_x >= 0 && root_y >= 0) {
        if (root_x + gCurImage->curWidth > gMonitorWidth
            && gCurImage->curWidth <= gMonitorWidth)
            root_x = gMonitorWidth - gCurImage->curWidth;
        if (root_y + gCurImage->curHeight > gMonitorHeight
            && gCurImage->curHeight <= gMonitorHeight)
            root_y = gMonitorHeight - gCurImage->curHeight;

        gtk_window_move(GTK_WINDOW(sWin->window), root_x, root_y);
        //gtk_window_set_position(GTK_WINDOW(sWin), GTK_WIN_POS_MOUSE);
    }
#endif

    gtk_widget_show(sWin);
}

/**
 * gdk_window_focus:
 * @window: a #GdkWindow
 * @timestamp: timestamp of the event triggering the window focus
 *
 * Sets keyboard focus to @window. If @window is not onscreen this
 * will not work. In most cases, gtk_window_present() should be used on
 * a #GtkWindow, rather than calling this function.
 *
 * For Pho: this is a replacement for gdk_window_focus
 * due to the issue in http://bugzilla.gnome.org/show_bug.cgi?id=150668
 * 
 **/
#define GDK_WINDOW_DISPLAY(win)       gdk_drawable_get_display(win)
#define GDK_WINDOW_SCREEN(win)	      gdk_drawable_get_screen(win)
#define GDK_WINDOW_XROOTWIN(win)      GDK_ROOT_WINDOW()

#ifdef TEST_FOCUS
#if GTK_MAJOR_VERSION == 2
static void
pho_window_focus (GdkWindow *window,
                  guint32    timestamp)
{
  GdkDisplay *display;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  display = GDK_WINDOW_DISPLAY (window);

  if (gdk_x11_screen_supports_net_wm_hint (GDK_WINDOW_SCREEN (window),
                                           gdk_atom_intern ("_NET_ACTIVE_WINDOW", FALSE)))
  {
      XEvent xev;

      xev.xclient.type = ClientMessage;
      xev.xclient.serial = 0;
      xev.xclient.send_event = True;
      xev.xclient.window = GDK_WINDOW_XWINDOW (window);
      xev.xclient.message_type =
          gdk_x11_get_xatom_by_name_for_display (display,
                                                 "_NET_ACTIVE_WINDOW");
      xev.xclient.format = 32;
      xev.xclient.data.l[0] = 1; /* requestor type; we're an app */
      xev.xclient.data.l[1] = timestamp;
      xev.xclient.data.l[2] = GDK_WINDOW_XID (window);
      xev.xclient.data.l[3] = 0;
      xev.xclient.data.l[4] = 0;
      
      XSendEvent (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XROOTWIN (window), False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  &xev);
  }
  else
  {
      XRaiseWindow (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window));
      gdk_keyboard_grab();
      gtk_grab_add(my_window);
      //gtk_grab_remove(my_window);

      /* There is no way of knowing reliably whether we are viewable;
       * _gdk_x11_set_input_focus_safe() traps errors asynchronously.
      _gdk_x11_set_input_focus_safe (display, GDK_WINDOW_XID (window),
                                     RevertToParent,
                                     timestamp);
       */
  }
}
#endif /* GTK2 */
#endif /* TEST_FOCUS */

/* PrepareWindow is responsible for making the window the right
 * size and position, so the user doesn't see flickering.
 * It may actually make a new window, or it may just resize and/or
 * reposition the existing window.
 */
void PrepareWindow()
{
    if (gMakeNewWindows || sWin == 0) {
        NewWindow();
        return;
    }

    /* Otherwise, resize and reposition the current window. */

    if (gPresentationMode) {
        /* XXX shouldn't have to do this every time */
        gtk_drawing_area_size(GTK_DRAWING_AREA(sDrawingArea),
                              sPhysMonitorWidth, sPhysMonitorHeight);
#if GTK_MAJOR_VERSION == 2
        gtk_window_fullscreen(GTK_WINDOW(sWin));
#endif
    }
    else {
#if GTK_MAJOR_VERSION == 2
        /* We need to size the actual window, not just the drawing area.
         * Resizing the drawing area will resize the window for many
         * window managers, but not for metacity.
         *
         * Worse, metacity maximizes a window if the initial size is
         * bigger in either dimension than the screen size.
         * Since we can't be sure about the size of the wm decorations,
         * we will probably hit this and get unintentionally maximized,
         * after which metacity refuses to resize the window any smaller.
         * (Mac OS X apparently does this too.)
         * So force non-maximal mode.  (Users who want a maximized
         * window will probably prefer fullscreen mode anyway.)
         */
        gtk_window_unfullscreen(GTK_WINDOW(sWin));
        gtk_window_unmaximize(GTK_WINDOW(sWin));
        gtk_window_resize(GTK_WINDOW(sWin),
                          gCurImage->curWidth, gCurImage->curHeight);
#else
        /* In gtk1 we're happy, resizing the drawing area "just works",
         * even in metacity.  Ah, the good old days!
         * Wrongo -- doesn't work any more. :-(
         *
        gdk_window_resize(sWin->window,
                          gCurImage->curWidth, gCurImage->curHeight);
	 */
        gtk_drawing_area_size(GTK_DRAWING_AREA(sDrawingArea),
                              gCurImage->curWidth, gCurImage->curHeight);
#endif

        MaybeMove();
    }

    /* Request the focus.
     * Neither gtk_window_present nor gdk_window_focus seem to work.
     */
#if GTK_MAJOR_VERSION == 2
#ifdef TEST_FOCUS
    pho_window_focus(sWin->window, GDK_CURRENT_TIME);

    /* None of these actually work!  Is there any way to get keyboard
     * focus into a window?
     */
    gtk_window_activate_focus(GTK_WINDOW(sWin));
    gtk_window_present(GTK_WINDOW(sWin));

    GdkGrabStatus grabstat;
    printf("Trying desperately to grab focus!\n");
    gtk_window_present(GTK_WINDOW(sWin));
    gtk_widget_grab_focus(sWin);
    //gdk_x11_display_grab(gdk_drawable_get_display(sWin->window));
    grabstat = gdk_keyboard_grab(sWin->window, FALSE, GDK_CURRENT_TIME);
    printf("Grab status: %d\n", grabstat);
    gdk_window_lower(sWin->window);
    //gdk_window_raise(sWin->window);
    _gdk_x11_set_input_focus_safe(gdk_drawable_get_display(sWin->window),
                                  GDK_WINDOW_XID(sWin->window),
                                  TRUE,
                                  GDK_CURRENT_TIME);
#endif /* TEST_FOCUS */
#endif

    DrawImage();
}

void CheckArg(char arg)
{
    if (arg == 'd')
        gDebug = 1;
    else if (arg == 'h')
        Usage();
    else if (arg == 'v')
        VerboseHelp();
    else if (arg == 'n')
        gMakeNewWindows = 1;
    else if (arg == 'p')
        gPresentationMode = 1;
    else if (arg == 'P')
        gPresentationMode = 0;
    else Usage();
}

int main(int argc, char** argv)
{
    /* Initialize some defaults from environment variables,
     * before reading cmdline args.
     */
    char* env = getenv("PHO_ARGS");
    while (env && *env)
    {
        if (*env != '-')
            CheckArg(*env);
        ++env;
    }

    while (argc > 1)
    {
        if (argv[1][0] == '-') {
            CheckArg(argv[1][1]);
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
                img->next = 0;
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

    sPhysMonitorWidth = gMonitorWidth = gdk_screen_width();
    sPhysMonitorHeight = gMonitorHeight = gdk_screen_height();

    /* Initialize the "black" color */
    sBlack.red = sBlack.green = sBlack.blue = 0x0000;

    /* Load the first image */
    if (NextImage() != 0)
        exit(1);

    gtk_main();
    return 0;
}

