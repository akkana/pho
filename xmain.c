/*
 * xmain.c: Xlib routines for pho.
 *
 * Copyright 2002 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-xlib.h>

#include <X11/keysym.h>

#include <stdlib.h>       // for getenv()
#include <stdio.h>

extern GdkPixbuf* image;

static Display *dpy;
static int screen;
static Window win;
static GC gc;

#include "pho.h"

void InitWin();

void ShowImage()
{
    if (win == 0)
        InitWin();

    XSize = gdk_pixbuf_get_width(image);
    YSize = gdk_pixbuf_get_height(image);

    // Now scale it if needed.
    if (XSize > MonitorWidth || YSize > MonitorHeight)
    {
        double xratio = (double)XSize / MonitorWidth;
        double yratio = (double)YSize / MonitorHeight;
        double ratio = (xratio > yratio ? xratio : yratio);
        GdkPixbuf* newimage;

        XSize = (double)XSize / ratio;
        YSize = (double)YSize / ratio;

        newimage = gdk_pixbuf_scale_simple(image, XSize, YSize,
                                           GDK_INTERP_NEAREST);
        gdk_pixbuf_unref(image);
        image = newimage;
        resized = 1;
    }

    XResizeWindow(dpy, win, XSize, YSize);
    gdk_pixbuf_xlib_render_to_drawable(image, win, gc,
                                       0, 0, 0, 0, XSize, YSize,
                                       XLIB_RGB_DITHER_NONE, 0, 0);
    XSync(dpy, 1);
}

// stub, unused
void ShowDeleteDialog() { }

void EndSession()
{
    PrintNotes();
    exit(0);
}

static long eventMask = ( ExposureMask
                          /* | ButtonPressMask| ButtonReleaseMask */
                          | KeyPressMask
                          | StructureNotifyMask );

int HandleEvent()
{
    XEvent event;
    char buffer[20];
    KeySym keysym;
    XComposeStatus compose;

    XNextEvent(dpy, &event);
    switch (event.type)
    {
      case Expose:
          //printf("Expose\n");
          ShowImage();
          break;
      case ConfigureNotify:
          XSize = event.xconfigure.width;
          YSize = event.xconfigure.height;
          //printf("ConfigureNotify: now (%d, %d)\n", XSize, YSize);
          //XSelectInput(dpy, win, eventMask);
          break;
      case KeyPress:
          //printf("KeyPress\n");
          XLookupString(&(event.xkey), buffer, sizeof buffer,
                        &keysym, &compose);
          switch (keysym)
          {
            case XK_d:
                //ReallyDelete();
                // Fall through: go to next image.
            case XK_space:
                if (NextImage() != 0)
                    EndSession();
                ShowImage();
                break;
            case XK_BackSpace:
            case XK_minus:
                if (PrevImage() == 0)
                    ShowImage();
                break;
            case XK_0:
            case XK_1:
            case XK_2:
            case XK_3:
            case XK_4:
            case XK_5:
            case XK_6:
            case XK_7:
            case XK_8:
            case XK_9:
                SetNoteFlag(ArgP, keysym - XK_0);
                break;
            case XK_t:   // make life easier for xv users
            case XK_r:
            case XK_Right:
                RotateImage(90);
                break;
            case XK_T:   // make life easier for xv users
            case XK_R:
            case XK_Left:
                RotateImage(-90);
                break;
            case XK_Up:
                RotateImage(180);
                break;
            case XK_q:
                EndSession();
                return -1;
            default:
                if (Debug)
                    printf("Don't know key 0x%lx\n", keysym);
                break;
          }
          break;

      case MapNotify:
      case ReparentNotify:
          break;
      default:
          if (Debug)
              printf("Unknown event: %d\n", event.type);
    }
    return 0;
}

int main(int argc, char** argv)
{
    while (argc > 1 && argv[1][0] == '-')
    {
        if (argv[1][1] == 'd')
            Debug = 1;
        else if (argv[1][1] == 'v')
            Usage();
        else Usage();
        --argc;
        ++argv;
    }

    if (argc <= 1)
        Usage();

    ArgV = argv;
    ArgC = argc;
    ArgP = 0;

    // Allocate space to store notes about each image.
    MakeNotesList(ArgC);

    // Load the first image (make sure we have at least one):
    if (NextImage() != 0)
        exit(1);

    if ((dpy = XOpenDisplay(getenv("DISPLAY"))) == 0)
    {
        fprintf(stderr, "Can't open display: %s\n", getenv("DISPLAY"));
        exit(1);
    }
    screen = DefaultScreen(dpy);

    MonitorWidth = DisplayWidth(dpy, screen);
    MonitorHeight = DisplayHeight(dpy, screen);

    // Don't actually create a window now -- wait 'til we open the first image.
    InitWin();

    XMapWindow(dpy, win);

    while (HandleEvent() >= 0)
        ;

    PrintNotes();
    EndSession();
    return 0;
}

void InitWin()
{
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen),
                              0, 0, XSize, YSize, 3,
                              WhitePixel(dpy, screen),
                              BlackPixel(dpy, screen));
    if (!win)
    {
        fprintf(stderr, "Can't create window\n");
        exit(1);
    }

    gc = XCreateGC(dpy, win,  0, 0);
    if (!gc) {
        fprintf(stderr, "Couldn't create gc!\n");
        exit(1);
    }

    gdk_pixbuf_xlib_init(dpy, screen);

    XSelectInput(dpy, win, eventMask);
}

