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
#include <ctype.h>
#include <unistd.h>

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
GtkWidget *gWin = 0;
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
    if (gDisplayMode == PHO_DISPLAY_PRESENTATION) {
        gMonitorWidth = sPhysMonitorWidth;
        gMonitorHeight = sPhysMonitorHeight;
    }
    else {
        gMonitorWidth = sPhysMonitorWidth - sFrameWidth;
        gMonitorHeight = sPhysMonitorHeight - sFrameHeight;
    }
}

void SetDisplayMode(int newmode)
{
    if (newmode == gDisplayMode)
        return;
    if (gWin && sDrawingArea) {
        if (newmode == PHO_DISPLAY_PRESENTATION) {
            hide_cursor(sDrawingArea);

#if GTK_MAJOR_VERSION == 2
            gtk_window_fullscreen(GTK_WINDOW(gWin));
            gtk_window_move(GTK_WINDOW(gWin), 0, 0);
#endif
        }
        else {
            show_cursor(sDrawingArea);

#if GTK_MAJOR_VERSION == 2
            gtk_window_unfullscreen(GTK_WINDOW(gWin));
#endif
        }
    }
    if (newmode == PHO_DISPLAY_KEYWORDS) {
        if (gWin)
            ShowKeywordsDialog();
        gScaleMode = PHO_SCALE_SCREEN_RATIO;
        gScaleRatio = .5;
    }
    else {
        if (gWin)
            HideKeywordsDialog();
    }
    gDisplayMode = newmode;
}

/* DrawImage is called from the expose callback.
 * It assumes we already have the image in gImage.
 */
void DrawImage()
{
    int dstX = 0, dstY = 0;
    char title[BUFSIZ];
#   define TITLELEN ((sizeof title) / (sizeof *title))

    if (gDebug) {
        printf("DrawImage %s, %dx%d\n", gCurImage->filename,
               gCurImage->curWidth, gCurImage->curHeight);
    }
    if (gImage == 0 || gWin == 0 || sDrawingArea == 0) return;

    if (gDisplayMode == PHO_DISPLAY_PRESENTATION) {
        gint width, height;
        gdk_window_clear(sDrawingArea->window);

        /* Center the image. This has to be done according to
         * the current window size, not the phys monitor size,
         * because in the xinerama case, gtk_window_fullscreen()
         * only fullscreens the current monitor, not all of them.
         */
        gtk_window_get_size(GTK_WINDOW(gWin), &width, &height);
        dstX = (width - gCurImage->curWidth) / 2;
        dstY = (height - gCurImage->curHeight) / 2;
    }
    else {
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
        else if (gScaleMode == PHO_SCALE_IMG_RATIO ||
                 gScaleMode == PHO_SCALE_SCREEN_RATIO) {
            char* titleEnd = title + strlen(title);
            if (gScaleRatio < 1)
                sprintf(titleEnd, " [%s/ %d]",
                        (gScaleMode == PHO_SCALE_IMG_RATIO ? "fullsize " : ""),
                        (int)(1. / gScaleRatio));
            else
                sprintf(titleEnd, " [%s* %d]",
                        (gScaleMode == PHO_SCALE_IMG_RATIO ? "fullsize " : ""),
                        (int)gScaleRatio);
        }
        gtk_window_set_title(GTK_WINDOW(gWin), title);

        if (gDisplayMode == PHO_DISPLAY_KEYWORDS)
            ShowKeywordsDialog(gCurImage);
    }

    gdk_pixbuf_render_to_drawable(gImage, sDrawingArea->window,
                   sDrawingArea->style->fg_gc[GTK_WIDGET_STATE(sDrawingArea)],
                                  0, 0, dstX, dstY,
                                  gCurImage->curWidth, gCurImage->curHeight,
                                  GDK_RGB_DITHER_NONE, 0, 0);

    UpdateInfoDialog(gCurImage);
}

static PhoImage* AddImage(char* filename)
{
    PhoImage* img = NewPhoImage(filename);
    if (gDebug)
        printf("Adding image %s\n", filename);
    if (!img) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }
    /* Make img the new last image in the list */
    if (gFirstImage == 0) {
        gFirstImage = img->next = img->prev = img;
    }
    else {
        PhoImage* lastImg = gFirstImage->prev;
        if (lastImg == gFirstImage || lastImg == 0) {  /* only 1 img in list */
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
    return img;
}

static void SetNewFiles(GtkWidget *dialog, gint res)
{
	GSList *files, *cur;
    gboolean overwrite;

    if (res == GTK_RESPONSE_ACCEPT)
        overwrite = FALSE;
    else if (res == GTK_RESPONSE_OK)
        overwrite = TRUE;
    else {
        gtk_widget_destroy (dialog);
        return;
    }

    files = cur = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));

    if (overwrite)
        ClearImageList();

    gCurImage = 0;

    while (cur)
    {
        PhoImage* img = AddImage((char*)(cur->data));
        if (!gCurImage)
            gCurImage = img;

        cur = cur->next;
    }
    if (files)
        g_slist_free (files);

    gtk_widget_destroy (dialog);

    ThisImage();
}

static void ChangeWorkingFileSet()
{
    GtkWidget* fsd = gtk_file_chooser_dialog_new("Change file set", NULL,
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_ADD, GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_NEW, GTK_RESPONSE_OK,
                                         NULL);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(fsd), TRUE);

    if (gCurImage && gCurImage->filename && gCurImage->filename[0] == '/')
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fsd),
                                            g_dirname(gCurImage->filename));
    else {
        char buf[BUFSIZ];
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fsd),
                                            getcwd(buf, BUFSIZ));
    }

	g_signal_connect(G_OBJECT(fsd), "response", G_CALLBACK(SetNewFiles), 0);
	gtk_widget_show(fsd);
}

/* An expose event has a  GdkRectangle area and a GdkRegion *region
 * as well as gint count of subsequent expose events.
 * Unfortunately count is always 0.
 */
static gint HandleExpose(GtkWidget* widget, GdkEventExpose* event)
{
    gint width, height;
    gdk_drawable_get_size(widget->window, &width, &height);
    if (gDebug) {
        printf("HandleExpose: area %dx%d +%d+%d in window %dx%d\n",
               event->area.width, event->area.height,
               event->area.x, event->area.y,
               width, height);
        if (event->area.width != gCurImage->curWidth ||
            event->area.height != gCurImage->curHeight)
            printf("*** Expose different from actual image size of %dx%d!\n",
                   gCurImage->curWidth, gCurImage->curHeight);
    }

    DrawImage();

    /* Get the frame offset if we don't already have it */
    if (sFrameWidth < 0 || sFrameHeight < 0) {
        gint win_x, win_y, contents_x, contents_y;
        gdk_window_get_root_origin(gWin->window, &win_x, &win_y);
        gdk_window_get_position(gWin->window, &contents_x, &contents_y);
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
        gtk_widget_set_size_request(GTK_WIDGET(gWin), 1, 1);
#endif

    return TRUE;
}

void EndSession()
{
    gCurImage = 0;
    UpdateInfoDialog();
    if (gDisplayMode == PHO_DISPLAY_KEYWORDS)
        ShowKeywordsDialog();

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

gint HandleGlobalKeys(GtkWidget* widget, GdkEventKey* event)
{
    switch (event->keyval)
    {
      case GDK_d:
          DeleteImage(gCurImage);
          break;
      case GDK_space:
          /* If we're in slideshow mode, cancel the slideshow */
          if (gDelaySeconds > 0) {
              gDelaySeconds = 0;
          }
          else if (NextImage() != 0) {
              if (Prompt("Quit pho?", "Quit", "Continue", "qx \n", "cn") != 0)
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
      case GDK_End:
          gCurImage = gFirstImage->prev;
          ThisImage();
          return TRUE;
      case GDK_n:   /* Get out of any weird display modes */
          gScaleMode = PHO_SCALE_NORMAL;
          gScaleRatio = 1.;
          AdjustScreenSize();
          ShowImage();
          return TRUE;
      case GDK_f:   /* Full size mode: show image bit-for-bit */
          if (gScaleMode != PHO_SCALE_FULLSIZE)
              gScaleMode = PHO_SCALE_FULLSIZE;
          else
              gScaleMode = PHO_SCALE_NORMAL;
          gScaleRatio = 1.;
          AdjustScreenSize();
          ShowImage();
          return TRUE;
      case GDK_F:   /* Full screen mode: as big as possible on screen */
          if (gScaleMode != PHO_SCALE_FULLSCREEN)
              gScaleMode = PHO_SCALE_FULLSCREEN;
          else
              gScaleMode = PHO_SCALE_NORMAL;
          gScaleRatio = 1.;
          AdjustScreenSize();
          ShowImage();
          return TRUE;
      case GDK_p:
          AdjustScreenSize();
          SetDisplayMode((gDisplayMode == PHO_DISPLAY_PRESENTATION)
                         ? PHO_DISPLAY_NORMAL
                         : PHO_DISPLAY_PRESENTATION);
          ShowImage();
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
          ScaleAndRotate(gCurImage, 90);
          PrepareWindow();
          DrawImage();
          return TRUE;
      case GDK_T:   /* make life easier for xv users */
      case GDK_R:
      case GDK_l:
      case GDK_L:
      case GDK_Left:
      case GDK_KP_Left:
          ScaleAndRotate(gCurImage, -90);
          PrepareWindow();
          DrawImage();
          return TRUE;
      case GDK_Up:
      case GDK_Down:
          ScaleAndRotate(gCurImage, 180);
          DrawImage();
          return TRUE;
      case GDK_plus:
      case GDK_KP_Add:
      case GDK_equal:
          if (gScaleMode == PHO_SCALE_FULLSIZE)
              gScaleMode = PHO_SCALE_IMG_RATIO;
          else if (gScaleMode != PHO_SCALE_IMG_RATIO)
              gScaleMode = PHO_SCALE_SCREEN_RATIO;
          gScaleRatio *= 2.;
          ScaleAndRotate(gCurImage, 0);
          PrepareWindow();
          DrawImage();
          return TRUE;
      case GDK_minus:
      case GDK_slash:
      case GDK_KP_Subtract:
          if (gScaleMode == PHO_SCALE_FULLSIZE)
              gScaleMode = PHO_SCALE_IMG_RATIO;
          else if (gScaleMode != PHO_SCALE_IMG_RATIO)
              gScaleMode = PHO_SCALE_SCREEN_RATIO;
          gScaleRatio /= 2.;
          ScaleAndRotate(gCurImage, 0);
          PrepareWindow();
          DrawImage();
          return TRUE;
      case GDK_g:  /* start gimp, or some other app */
          {
              char buf[BUFSIZ];
              char* cmd = getenv("PHO_REMOTE");
              if (cmd == 0) cmd = "gimp %s";
              else if (gDebug)
                  printf("Calling PHO_REMOTE %s\n", cmd);

              if (strlen(gCurImage->filename) + strlen(cmd) + 2 > BUFSIZ) {
                  printf("Filename '%s' or command '%s' too long!\n",
                         gCurImage->filename, cmd);
                  break;
              }
              sprintf(buf, cmd, gCurImage->filename);
              system(buf);
          }
          break;
      case GDK_i:
          ToggleInfo();
          return TRUE;
      case GDK_k:
          SetDisplayMode(PHO_DISPLAY_KEYWORDS);
          return TRUE;
      case GDK_o:
          ChangeWorkingFileSet();
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
 * Make sure we don't do that, assuming the image is still
 * small enough to fit. Then request focus from the window manager.
 * Try to move the window as little as possible.
 */
static void MaybeMove()
{
    gint x, y, w, h, nx, ny;

    if (gCurImage->curWidth > sPhysMonitorWidth
        || gCurImage->curHeight > sPhysMonitorHeight)
        return;

    /* If we don't have a window yet, don't move it */
    if (!gWin)
        return;

    /* If we're in presentation mode, never move the window */
    if (gDisplayMode == PHO_DISPLAY_PRESENTATION
        || gDisplayMode == PHO_DISPLAY_KEYWORDS)
        return;

    gtk_window_get_position(GTK_WINDOW(gWin), &x, &y);
    nx = x;  ny = y;
    gtk_window_get_size(GTK_WINDOW(gWin), &w, &h);
    /* printf("Currently (%d, %d) %d x %d\n", x, y, w, h); */

    /* See if it would overflow off the screen */
    if (x + gCurImage->curWidth > gMonitorWidth)
        nx = gMonitorWidth - gCurImage->curWidth;
    if (nx < 0)
        nx = 0;
    if (y + gCurImage->curHeight > gMonitorHeight)
        ny = gMonitorHeight - gCurImage->curHeight;
    if (ny < 0)
        ny = 0;

    if (x != nx || y != ny) {
        gtk_window_move(GTK_WINDOW(gWin), nx, ny);
    }

    /* Request focus from the window manager.
     * This is pretty much a no-op, but what the heck:
     */
    gtk_window_present(GTK_WINDOW(gWin));
}

/* Make a new window, destroying the old one. */
static void NewWindow()
{
    gint root_x = -1;
    gint root_y = -1;
    if (gWin) {
        gdk_window_get_position(gWin->window, &root_x, &root_y);
        gtk_object_destroy(GTK_OBJECT(gWin));
    }

    gWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_wmclass(GTK_WINDOW(gWin), "pho", "Pho");

    /* Window manager delete */
    gtk_signal_connect(GTK_OBJECT(gWin), "delete_event",
                       (GtkSignalFunc)HandleDelete, 0);

    /* This event occurs when we call gtk_widget_destroy() on the window,
     * or if we return FALSE in the "delete_event" callback.
    gtk_signal_connect(GTK_OBJECT(gWin), "destroy",
                       (GtkSignalFunc)HandleDestroy, 0);
     */

    /* KeyPress events on the drawing area don't come through --
     * they have to be on the window.
     */
    gtk_signal_connect(GTK_OBJECT(gWin), "key_press_event",
                       (GtkSignalFunc)HandleGlobalKeys, 0);

    sDrawingArea = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(gWin), sDrawingArea);
    gtk_widget_show(sDrawingArea);
#if GTK_MAJOR_VERSION == 2
    /* This can't be done in expose: it causes one of those extra
     * spurious expose events that gtk so loves.
     */
    gtk_widget_modify_bg(sDrawingArea, GTK_STATE_NORMAL, &sBlack);
#endif

    if (gDisplayMode == PHO_DISPLAY_PRESENTATION) {
        gtk_drawing_area_size(GTK_DRAWING_AREA(sDrawingArea),
                              sPhysMonitorWidth, sPhysMonitorHeight);
#if GTK_MAJOR_VERSION == 2
        gtk_window_fullscreen(GTK_WINDOW(gWin));
#endif
    }
    else {
        gtk_drawing_area_size(GTK_DRAWING_AREA(sDrawingArea),
                              gCurImage->curWidth, gCurImage->curHeight);
#if GTK_MAJOR_VERSION == 2
        gtk_window_unfullscreen(GTK_WINDOW(gWin));
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

    gtk_widget_show(gWin);

    if (gDisplayMode == PHO_DISPLAY_PRESENTATION)
        hide_cursor(sDrawingArea);
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
    if (gMakeNewWindows || gWin == 0) {
        NewWindow();
        return;
    }

    /* Otherwise, resize and reposition the current window. */

    if (gDisplayMode == PHO_DISPLAY_PRESENTATION) {
        /* XXX shouldn't have to do this every time */
        gtk_drawing_area_size(GTK_DRAWING_AREA(sDrawingArea),
                              sPhysMonitorWidth, sPhysMonitorHeight);
#if GTK_MAJOR_VERSION == 2
        //gtk_window_fullscreen(GTK_WINDOW(gWin));
#endif
    }
    else {
#if GTK_MAJOR_VERSION == 2
        gint winwidth, winheight;

        gdk_drawable_get_size(sDrawingArea->window, &winwidth, &winheight);

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
        gtk_window_unfullscreen(GTK_WINDOW(gWin));
        gtk_window_unmaximize(GTK_WINDOW(gWin));

        /* XXX Without the next line, we may get no expose events!
         * Likewise, if the next line doesn't actually resize anything
         * we may not get an expose event.
         */
        if (gCurImage->curWidth != winwidth
            || gCurImage->curHeight != winheight) {
            gtk_window_resize(GTK_WINDOW(gWin),
                              gCurImage->curWidth, gCurImage->curHeight);
            /* Unfortunately, on OS X this resize may not work,
             * if it puts part ofthe window off-screen; in which case
             * we won't get an Expose event. So if that happened,
             * force a redraw:
             */
            gdk_drawable_get_size(sDrawingArea->window, &winwidth, &winheight);
            if (gCurImage->curWidth != winwidth
                || gCurImage->curHeight != winheight) {
                if (gDebug)
                    printf("Resize didn't work! Forcing redraw\n");
                DrawImage();
            }
        }

        /* If we didn't resize the window, then we won't get an expose
         * event, and hence DrawImage won't be called. So call it explicitly:
         */
        else
            DrawImage();
#else
        /* In gtk1 we're happy, resizing the drawing area "just works",
         * even in metacity.  Ah, the good old days!
         * Wrongo -- doesn't work any more. :-(
         *
        gdk_window_resize(gWin->window,
                          gCurImage->curWidth, gCurImage->curHeight);
         */
        gtk_drawing_area_size(GTK_DRAWING_AREA(sDrawingArea),
                              gCurImage->curWidth, gCurImage->curHeight);
#endif

        /* Try to ensure that the window will be over the cursor
         * (so it will still have focus -- some window managers will
         * lose focus otherwise). But not in Keywords mode, where the
         * mouse should be over the Keywords dialog, not necessarily
         * the image window.
         */
        MaybeMove();
    }

    /* Request the focus.
     * Neither gtk_window_present nor gdk_window_focus seem to work.
     */
#if GTK_MAJOR_VERSION == 2
#ifdef TEST_FOCUS
    pho_window_focus(gWin->window, GDK_CURRENT_TIME);

    /* None of these actually work!  Is there any way to get keyboard
     * focus into a window?
     */
    gtk_window_activate_focus(GTK_WINDOW(gWin));
    gtk_window_present(GTK_WINDOW(gWin));

    GdkGrabStatus grabstat;
    printf("Trying desperately to grab focus!\n");
    gtk_window_present(GTK_WINDOW(gWin));
    gtk_widget_grab_focus(gWin);
    //gdk_x11_display_grab(gdk_drawable_get_display(gWin->window));
    grabstat = gdk_keyboard_grab(gWin->window, FALSE, GDK_CURRENT_TIME);
    printf("Grab status: %d\n", grabstat);
    gdk_window_lower(gWin->window);
    //gdk_window_raise(gWin->window);
    _gdk_x11_set_input_focus_safe(gdk_drawable_get_display(gWin->window),
                                  GDK_WINDOW_XID(gWin->window),
                                  TRUE,
                                  GDK_CURRENT_TIME);
#endif /* TEST_FOCUS */
#endif

    /* This will be redundant IF we get an expose event for the window;
     * but if we don't, then we need it here. How do we ensure that
     * we get exactly one expose event?
     * Though in practice, we always get at least one expose event,
     * and sometimes (if the window was resized) several.
     * So this call is probably never needed:
    DrawImage();
     */
}

/* CheckArg takes a string, like -Pvg, and sets all the relevant flags. */
static void CheckArg(char* arg)
{
    for ( ; *arg != 0; ++arg)
    {
        if (*arg == '-')
            ;
        else if (*arg == 'd')
            gDebug = 1;
        else if (*arg == 'h')
            Usage();
        else if (*arg == 'v')
            VerboseHelp();
        else if (*arg == 'n')
            gMakeNewWindows = 1;
        else if (*arg == 'p')
            SetDisplayMode(PHO_DISPLAY_PRESENTATION);
        else if (*arg == 'P')
            SetDisplayMode(PHO_DISPLAY_NORMAL);
        else if (*arg == 'k')
            SetDisplayMode(PHO_DISPLAY_KEYWORDS);
        else if (*arg == 's') {
            /* find the slideshow delay time, from e.g. pho -s2 */
            if (isdigit(arg[1]))
                gDelaySeconds = atoi(arg+1);
            else Usage();
            if (gDebug)
                printf("Slideshow delay %d seconds\n", gDelaySeconds);
        }
    }
}

int main(int argc, char** argv)
{
    /* Initialize some defaults from environment variables,
     * before reading cmdline args.
     */
    char* env = getenv("PHO_ARGS");
    if (env && *env)
        CheckArg(env);

    while (argc > 1)
    {
        if (argv[1][0] == '-')
            CheckArg(argv[1]);
        else {
            AddImage(argv[1]);
        }
        --argc;
        ++argv;
    }

    if (gFirstImage == 0)
        Usage();

    /* Initialize some variables associated with the notes flags */
    InitNotes();

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

