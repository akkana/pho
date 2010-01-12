/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * pho.c: core routines for pho, an image viewer.
 *
 * Copyright 2004 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include "pho.h"
#include "dialogs.h"
#include "exif/phoexif.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>    /* for unlink() */

/* ************* Definition of globals ************ */
PhoImage* gFirstImage = 0;
PhoImage* gCurImage = 0;

/* Monitor resolution */
int gMonitorWidth = 0;
int gMonitorHeight = 0;

/* We only have one image at a time, so make it global. */
GdkPixbuf* gImage = 0;

int gDebug = 0;    /* debugging messages */

int gScaleMode = PHO_SCALE_NORMAL;
double gScaleRatio = 1.0;

int gDisplayMode = PHO_DISPLAY_NORMAL;

/* Slideshow delay is zero by default -- no slideshow. */
int gDelaySeconds = 0;
int gPendingTimeout = 0;

static int RotateImage(PhoImage* img, int degrees);    /* forward */

static gint DelayTimer(gpointer data)
{
    if (gDelaySeconds == 0)    /* slideshow mode was cancelled */
        return FALSE;

    if (gDebug) printf("-- Timer fired\n");
    gPendingTimeout = 0;

    NextImage();
    return FALSE;       /* cancel the timer */
}

int ShowImage()
{
    ScaleAndRotate(gCurImage, 0);
    PrepareWindow();
    /* Keywords dialog will be updated if necessary from DrawImage */
    //UpdateKeywordsDialog();

    if (gDelaySeconds > 0 && gPendingTimeout == 0
        && (gCurImage->next != 0 || gCurImage->next != gFirstImage)) {
        if (gDebug) printf("Adding timeout for %d msec\n", gDelaySeconds*1000);
        gPendingTimeout = g_timeout_add (gDelaySeconds * 1000, DelayTimer, 0);
    }

    return 0;
}

static int LoadImageFromFile(PhoImage* img)
{
#if GTK_MAJOR_VERSION == 2
    GError* err = NULL;
#endif
    int rot;

    if (img == 0)
        return -1;

    if (gDebug)
        printf("LoadImageFromFile(%s)\n", img->filename);

    /* Free the current image */
    if (gImage) {
        gdk_pixbuf_unref(gImage);
        gImage = 0;
    }

#if GTK_MAJOR_VERSION==2
    gImage = gdk_pixbuf_new_from_file(img->filename, &err);
#else
    gImage = gdk_pixbuf_new_from_file(img->filename);
#endif
    if (!gImage)
    {
        gImage = 0;
#if GTK_MAJOR_VERSION == 2
        fprintf(stderr, "Can't open %s: %s\n", img->filename, err->message);
#else
        fprintf(stderr, "Can't open %s\n", img->filename);
#endif
        return -1;
    }

    img->curWidth = img->trueWidth = gdk_pixbuf_get_width(gImage);
    img->curHeight = img->trueHeight = gdk_pixbuf_get_height(gImage);
    img->curRot = 0;

    /* Read the EXIF rotation if we haven't already rotated this image */
    ExifReadInfo(img->filename);
    if (HasExif() && (rot = ExifGetInt(ExifOrientation)) != 0) {
        img->desiredRot = rot;
    }

    return 0;
}

int ThisImage()
{
    if (LoadImageFromFile(gCurImage) != 0)
        return NextImage();
    ShowImage();
    return 0;
}

int NextImage()
{
    if (gDebug)
        printf("\n================= NextImage ====================\n");
    do {
        if (gCurImage == 0)   /* no image loaded yet, first call */
            gCurImage = gFirstImage;
        else if ((gCurImage->next == 0) || (gCurImage->next == gFirstImage))
            return -1;  /* end of list, can't go farther */
        else
            gCurImage = gCurImage->next;
    } while (LoadImageFromFile(gCurImage) != 0);
    ShowImage();
    return 0;
}

int PrevImage()
{
    do {
        if (gCurImage == 0) {  /* no image loaded yet, first call */
            gCurImage = gFirstImage;
            if (gCurImage->prev)
                gCurImage = gCurImage->prev;
        }
        else {
            if (gCurImage == gFirstImage)
                return -1;  /* end of list */
            gCurImage = gCurImage->prev;
        }
    } while (LoadImageFromFile(gCurImage) != 0);
    ShowImage();
    return 0;
}

/* Limit new_width and new_height so that they're no bigger than
 * max_width and max_height.
 */
static void ScaleToFit(int *new_width, int *new_height,
                       int max_width, int max_height)
{
    double ratio = 1.;
    /* scale so that the biggest ratio just barely fits on screen. */
    double xratio = (double)max_width / *new_width;
    double yratio = (double)max_height / *new_height;
    if (xratio > yratio) ratio = yratio;
    else ratio = xratio;

    *new_width = ratio * *new_width;
    *new_height = ratio * *new_height;
}

#define SWAP(a, b) { int temp = a; a = b; b = temp; }
//#define SWAP(a, b)  {a ^= b; b ^= a; a ^= b;}

/* Scale the image according to the current scale mode.
 * 
 * This will read the image from disk if necessary,
 * and it will rotate the image at the appropriate time
 * (when the image is at its smallest).
 *
 * This is the routine that should be called by external callers:
 * callers should never need to call RotateImage or LoadImageFromFile.
 *
 * degrees is the amount of rotation *in addition to* img->desiredRot.
 */
void ScaleAndRotate(PhoImage* img, int degrees)
{
    int true_width = img->trueWidth;
    int true_height = img->trueHeight;
    int cur_width = img->curWidth;
    int cur_height = img->curHeight;
    int new_width;
    int new_height;
    int aspect_changing;

    if (gDebug)
        printf("ScaleAndRotate(%d)\n", degrees);

    degrees += img->desiredRot - img->curRot;

    /* Make sure degrees is between 0 and 360 */
    degrees = (degrees + 360) % 360;
    aspect_changing = ((degrees % 180) != 0);

    /* First, load the image if we haven't already */
    if (true_width == 0 || true_height == 0) {
        LoadImageFromFile(img);
        degrees = degrees + img->desiredRot;
    }

    /* Adjust aspect ratio of true width and height for rotation */
    if (aspect_changing) {
        SWAP(true_width, true_height);
        SWAP(cur_width, cur_height);
    }

    /* Fullsize: display always at real resolution,
     * even if it's too big to fit on the screen.
     */
    if (gScaleMode == PHO_SCALE_FULLSIZE) {
        new_width = true_width;
        new_height = true_height;
    }

    /* Normal: display at full size unless it won't fit the screen,
     * in which case scale it down.
     */
#define NORMAL_SCALE_SLOP 5
    else if (gScaleMode == PHO_SCALE_NORMAL
             || gScaleMode == PHO_SCALE_SCREEN_RATIO) {
        int max_width = gMonitorWidth;
        int max_height = gMonitorHeight;
        int diff;
        if (gScaleMode == PHO_SCALE_SCREEN_RATIO) {
            max_width *= gScaleRatio;
            max_height *= gScaleRatio;
            /* If we're scaled up or down, then keep the image's maximum
             * size the same whether we're horizontal or vertical; no need
             * to limit the vertical's size.
             */
            if (gScaleRatio != 1.)
                max_width = max_height = MAX(max_width, max_height);
        }

        new_width = true_width;
        new_height = true_height;

        /* In screen scale mode, if scale ratio is greater than 1,
         * scale the image up by that ratio if we can do that without
         * exceeding the maximum size.
         */
        if (gScaleMode == PHO_SCALE_SCREEN_RATIO && gScaleRatio > 1) {
            new_width *= gScaleRatio;
            new_height *= gScaleRatio;
        }

        if (new_width > max_width || new_height > max_height) {
            ScaleToFit(&new_width, &new_height, max_width, max_height);
        }

        /* Now w and h hold the desired sizes.  See if we're close already. */
        diff = abs(cur_width - new_width)
            + abs(cur_height - new_height);
        if (diff < NORMAL_SCALE_SLOP) {
            new_width = cur_width;
            new_height = cur_height;
        }
    }

    else if (gScaleMode == PHO_SCALE_IMG_RATIO) {
        new_width = true_width * gScaleRatio;
        new_height = true_height * gScaleRatio;

        /* See if we're close */
        int diff = abs(cur_width - new_width)
            + abs(cur_height - new_height);
        if (diff < NORMAL_SCALE_SLOP) {
            new_width = cur_width;
            new_height = cur_height;
        }
    }
        
    /* Fullscreen: Scale either up or down if necessary to make
     * the largest dimension match the screen size.
     */
#define FULLSCREEN_SCALE_SLOP 20
    else if (gScaleMode == PHO_SCALE_FULLSCREEN) {
        /* If we're in presentation mode, then we need to scale
         * to the current size of the window, not the monitor,
         * because in xinerama gdk_window_fullscreen() will fullscreen
         * onto only one monitor, but gdk_screen_width() gives the
         * width of the full xinerama setup.
         */
        int diffx = abs(cur_width - gMonitorWidth);
        int diffy = abs(cur_height - gMonitorHeight);
        double xratio, yratio;
        gint screenwidth, screenheight;

        if (gDisplayMode == PHO_DISPLAY_PRESENTATION)
            gtk_window_get_size(GTK_WINDOW(gWin), &screenwidth, &screenheight);
        else {
            screenwidth = gMonitorWidth;
            screenheight = gMonitorHeight;
        }

        if (diffx < FULLSCREEN_SCALE_SLOP || diffy < FULLSCREEN_SCALE_SLOP) {
            /* XXX these get overwritten in a few lines! */
            new_width = cur_width;
            new_height = cur_height;
        }

        xratio = (double)screenwidth / true_width;
        yratio = (double)screenheight / true_height;

        /* Use xratio for the more extreme of the two */
        if (xratio > yratio) xratio = yratio;
        new_width = xratio * true_width;
        new_height = xratio * true_height;
    }
    else {
        /* Shouldn't ever happen, means gScaleMode is bogus */
        printf("Internal error: Unknown scale mode %d\n", gScaleMode);
        new_width = cur_width;
        new_height = cur_height;
    }
    /* Finally, we're done with the scaling modes.
     * Time to do the scaling and rotation,
     * reloading the image if needed.
     */

    /* First figure out if we're getting bigger and hence need to reload. */
    if ((new_width > cur_width || new_height > cur_height)
        && (cur_width < true_width && cur_height < true_height)) {
        /* image->curRot is going to be set back to zero
         * (or to the exif rotation setting)
         * so adjust current planned rotation accordingly.
         */
        degrees = (degrees + img->curRot + 360) % 360;
            /* Now it's the absolute end rot desired */
        aspect_changing = (((degrees + 360) % 180) != 0);

        LoadImageFromFile(img);
        //degrees += img->desiredRot;
        cur_width = true_width = gdk_pixbuf_get_width(gImage);
        cur_height = true_height = gdk_pixbuf_get_height(gImage);

        if (aspect_changing) {
            SWAP(true_width, true_height);
            SWAP(cur_width, cur_height);
        }
    }

    /* new_* are the sizes we want after rotation. But we need
     * to compare them now to the sizes before rotation. So we
     * need to compensate for rotation:
     */
    int unrot_new_height, unrot_new_width;

    /* If we're going to scale up, then do the rotation first,
     * before scaling. Otherwise, scale down first then rotate.
     */
    if (degrees != 0 && (new_width > cur_width || new_height > cur_height)) {
        RotateImage(img, degrees);
        degrees = 0;    /* finished with rotation */
        unrot_new_width = new_width;
        unrot_new_height = new_height;
    }
    else if (aspect_changing) {
        unrot_new_height = new_width;
        unrot_new_width = new_height;
    } else {
        unrot_new_width = new_width;
        unrot_new_height = new_height;
    }

    /* Do the scaling (thought we'd never get there!) */
    if (unrot_new_width != img->curWidth || unrot_new_height != img->curHeight)
    {
        GdkPixbuf* newimage = gdk_pixbuf_scale_simple(gImage,
                                                      unrot_new_width,
                                                      unrot_new_height,
                                                      GDK_INTERP_NEAREST);
        /* scale_simple apparently has no error return; if it fails,
         * it still returns a pixbuf but width and height are -1.
         * Hope this undocumented behavior doesn't change!
         */
        if (!newimage || gdk_pixbuf_get_width(newimage) < 1) {
            printf("\007Error scaling up to %d x %d: probably out of memory\n",
                   unrot_new_width, unrot_new_height);
            if (newimage)
                gdk_pixbuf_unref(newimage);
            Prompt("Couldn't scale up: probably out of memory", "Bummer", 0,
                   "\n ", "");
            return;
        }
        if (gImage)
            gdk_pixbuf_unref(gImage);
        gImage = newimage;
        img->curWidth = gdk_pixbuf_get_width(gImage);
        img->curHeight = gdk_pixbuf_get_height(gImage);
    }

    /* If we didn't rotate before, do it now. */
    if (degrees != 0)
        RotateImage(img, degrees);

    /* We'd better have current=desired rotation by now */
    img->desiredRot = img->curRot;
}

PhoImage* NewPhoImage(char* fnam)
{
    PhoImage* newimg = calloc(1, sizeof (PhoImage));
    if (newimg == 0) return 0;
    newimg->filename = fnam;  /* no copy, we don't own the memory */

    return newimg;
}

/* This routine exists to keep track of any allocated memory
 * existing in the PhoImage structure.
 */
static void FreePhoImage(PhoImage* img)
{
    if (img->comment) free(img->comment);
    free(img);
}

/* Remove all images from the image list, to start fresh. */
void ClearImageList()
{
    PhoImage* img = gFirstImage;
    while (img) {
        PhoImage* del = img;
        img = img->next;
        FreePhoImage(del);
    }
    gCurImage = gFirstImage = 0;
}

void ReallyDelete(PhoImage* img)
{
    if (unlink(img->filename) < 0)
    {
        printf("OOPS!  Can't delete %s\n", img->filename);
        return;
    }
    /* Disconnect it from the linked list, and reset gCurImage. */
    if (img == gFirstImage && img->next == 0) {   /* This is the only image! */
        EndSession();
    }
    if (img->prev == img->next) {       /* One image left after this */
        gCurImage = img->prev;
        gCurImage->prev = gCurImage->next = 0;
    }
    else if (img->next == 0) {          /* last image in the list. Go back! */
        gCurImage = img->prev;
        gCurImage->next = 0;
        gFirstImage->prev = gCurImage;
    }
    else {
        gCurImage = img->next;
        gCurImage->prev = img->prev;
        if (img->prev)
            img->prev->next = gCurImage;
        if (gCurImage->next)
            gCurImage->next->prev = gCurImage;
    }
    /* If we deleted the first image, make sure we reset the list */
    if (img == gFirstImage) {
        gFirstImage = img->next;
    }

    /* It's disconnected.  Free all the memory */
    FreePhoImage(img);

    ThisImage();
}

void DeleteImage(PhoImage* img)
{
    char buf[512];
    if (img->filename == 0)
        return;
    sprintf(buf, "Delete file %s?", img->filename);
    if (Prompt(buf, "Delete", 0, "dD\n", "nN") > 0)
        ReallyDelete(img);
}

/* RotateImage just rotates; it no longer calls ScaleImage.
 * It's typically called from ScaleAndRotate either just
 * before or just after scaling.
 */
static int RotateImage(PhoImage* img, int degrees)
{
    guchar *oldpixels, *newpixels;
    int x, y;
    int oldrowstride, newrowstride, nchannels, bitsper, alpha;
    int newWidth, newHeight, newTrueWidth, newTrueHeight;
    GdkPixbuf* newImage;

    if (!gImage) return 1;     /* sanity check */

    /* Make sure degrees is between 0 and 360 even if it's -90 */
    degrees = (degrees + 360) % 360;

    /* degrees might be zero now, since we might be rotating back to zero. */
    if (degrees == 0) {
        return 0;
    }

    /* Swap X and Y if appropriate */
    if (degrees == 90 || degrees == 270)
    {
        newWidth = img->curHeight;
        newHeight = img->curWidth;
        newTrueWidth = img->trueHeight;
        newTrueHeight = img->trueWidth;
    }
    else
    {
        newWidth = img->curWidth;
        newHeight = img->curHeight;
        newTrueWidth = img->trueWidth;
        newTrueHeight = img->trueHeight;
    }

    oldrowstride = gdk_pixbuf_get_rowstride(gImage);
    /* Sometimes rowstride is slightly different from width*nchannels:
     * gdk_pixbuf optimizes by aligning to 32-bit boundaries.
     * But apparently it works even if rowstride is not aligned,
     * just might not be as fast.
     * XXX check newrowstride alignment
     */
    bitsper = gdk_pixbuf_get_bits_per_sample(gImage);
    nchannels = gdk_pixbuf_get_n_channels(gImage);
    alpha = gdk_pixbuf_get_has_alpha(gImage);

    oldpixels = gdk_pixbuf_get_pixels(gImage);

    newImage = gdk_pixbuf_new(GDK_COLORSPACE_RGB, alpha, bitsper,
                              newWidth, newHeight);
    if (!newImage) return 1;
    newpixels = gdk_pixbuf_get_pixels(newImage);
    newrowstride = gdk_pixbuf_get_rowstride(newImage);

    for (x = 0; x < img->curWidth; ++x)
    {
        for (y = 0; y < img->curHeight; ++y)
        {
            int newx, newy;
            int i;
            switch (degrees)
            {
              case 90:
                newx = img->curHeight - y - 1;
                newy = x;
                break;
              case 270:
                newx = y;
                newy = img->curWidth - x - 1;
                break;
              case 180:
                newx = img->curWidth - x - 1;
                newy = img->curHeight - y - 1;
                break;
              default:
                printf("Illegal rotation value!\n");
                return 1;
            }
            for (i=0; i<nchannels; ++i)
            {
#if 0
                if (x > img->curWidth-3 && y > img->curHeight-3)
                {
                    printf("(%d, %d) -> (%d, %d) # %d\n", x, y, newx, newy, i);
                    printf("%d -> %d\n",
                           newy*newrowstride + newx*nchannels + i,
                           y*oldrowstride + x*nchannels + i);
                }
#endif
                newpixels[newy*newrowstride + newx*nchannels + i]
                    = oldpixels[y*oldrowstride + x*nchannels + i];
            }
        }
    }

    img->curWidth = newWidth;
    img->curHeight = newHeight;
    img->trueWidth = newTrueWidth;
    img->trueHeight = newTrueHeight;

    img->curRot = img->desiredRot = (img->curRot + degrees + 360) % 360;

    gdk_pixbuf_unref(gImage);
    gImage = newImage;

    return 0;
}

void Usage()
{
    printf("pho version %s.  Copyright 2002,2003,2004,2007 Akkana Peck akkana@shallowsky.com.\n", VERSION);
    printf("Usage: pho [-dhnp] image [image ...]\n");
    printf("\t-p: Presentation mode (full screen)\n");
    printf("\t-k: Keywords mode (show a Keywords dialog for each image)\n");
    printf("\t-n: Replace each image window with a new window (helpful for some window managers)\n");
    printf("\t-sN: Slideshow mode, where N is the timeout in seconds\n");
    printf("\t-d: Debug messages\n");
    printf("\t-h: Help: Print this summary\n");
    printf("\t-v: Verbose help: Print a summary of key bindings\n");
    exit(1);
}

void VerboseHelp()
{
    printf("pho version %s.  Copyright 2002,2003,2004,2007 Akkana Peck akkana@shallowsky.com.\n", VERSION);
    printf("Type pho -h for commandline arguments.\n");
    printf("\npho Key Bindings:\n\n");
    printf("<space>\tNext image (or cancel slideshow mode)\n");
    printf("-\tPrevious image\n");
    printf("<backspace>\tPrevious image\n");
    printf("<home>\tFirst image\n");
    printf("f\tToggle fullscreen mode (scale even small images up to fullscreen)\n");
    printf("F\tToggle full-size mode (even if bigger than screen)\n");
    printf("k\tTurn on keywords mode: show the keywords dialog\n");
    printf("p\tToggle presentation mode (take up the whole screen, centering the image)\n");
    printf("d\tDelete current image (from disk, after confirming with another d)\n");
    printf("0-9\tRemember image in note list 0 through 9 (to be printed at exit)\n");
    printf("t\tRotate right 90 degrees\n");
    printf("r\tRotate right 90 degrees\n");
    printf("<Right>\tRotate right 90 degrees\n");
    printf("T\tRotate left 90 degrees\n");
    printf("R\tRotate left 90 degrees\n");
    printf("l\tRotate left 90 degrees\n");
    printf("L\tRotate left 90 degrees\n");
    printf("<left>\tRotate left 90 degrees\n");
    printf("<up>\tRotate 180 degrees\n");
    printf("+\tDouble size\n");
    printf("=\tDouble size\n");
    printf("/\tHalf size\n");
    printf("<kp>-\tHalf size\n");
    printf("i\tShow/hide info dialog\n");
    printf("q\tQuit\n");
    printf("<esc>\tQuit (or hide a dialog, if one is showing)\n");
    exit(1);
}







