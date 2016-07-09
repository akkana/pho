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
#include <errno.h>
#include <string.h>
#include <unistd.h>    /* for unlink() */
#include <fcntl.h>     /* for symbols like O_RDONLY */

/* ************* Definition of globals ************ */
PhoImage* gFirstImage = 0;
PhoImage* gCurImage = 0;

/* Monitor resolution */
int gMonitorWidth = 0;
int gMonitorHeight = 0;

/* Effective resolution, e.g. if we'll be sending to a projector */
int gPresentationWidth = 0;
int gPresentationHeight = 0;

/* We only have one image at a time, so make it global. */
GdkPixbuf* gImage = 0;

int gDebug = 0;    /* debugging messages */

int gScaleMode = PHO_SCALE_NORMAL;
double gScaleRatio = 1.0;

int gDisplayMode = PHO_DISPLAY_NORMAL;

/* Slideshow delay is zero by default -- no slideshow. */
int gDelaySeconds = 0;
int gPendingTimeout = 0;

/* Loop back to the first image after showing the last one */
int gRepeat = 0;

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
    /* Keywords dialog will be updated if necessary from DrawImage */

    if (gDelaySeconds > 0 && gPendingTimeout == 0
        && (gCurImage->next != 0 || gCurImage->next != gFirstImage)) {
        if (gDebug) printf("Adding timeout for %d msec\n", gDelaySeconds*1000);
        gPendingTimeout = g_timeout_add (gDelaySeconds * 1000, DelayTimer, 0);
    }

    return 0;
}
	
static int LoadImageFromFile(PhoImage* img)
{
    GError* err = NULL;
    int rot;

    if (img == 0)
        return -1;

    if (gDebug)
        printf("LoadImageFromFile(%s)\n", img->filename);

    /* Free the current image */
    if (gImage) {
        g_object_unref(gImage);
        gImage = 0;
    }

    gImage = gdk_pixbuf_new_from_file(img->filename, &err);
    if (!gImage)
    {
        gImage = 0;
        fprintf(stderr, "Can't open %s: %s\n", img->filename, err->message);
        return -1;
    }
    ReadCaption(img);

    img->curWidth = gdk_pixbuf_get_width(gImage);
    img->curHeight = gdk_pixbuf_get_height(gImage);

    /* The first time an image is loaded, it should be rotated
     * to its appropriate EXIF rotation. Subsequently, though,
     * it should be rotated to curRot.
     */
    if (img->trueWidth == 0 || img->trueHeight == 0) {
        /* Read the EXIF rotation if we haven't already rotated this image */
        ExifReadInfo(img->filename);
        if (HasExif() && (rot = ExifGetInt(ExifOrientation)) != 0)
            img->exifRot = rot;
        else
            img->exifRot = 0;
    }

    /* trueWidth and Height used to be set inside EXIF clause,
     * but that doesn't make sense -- we need it not just the first
     * time, but also ever time the image is reloaded.
     */
    img->trueWidth = img->curWidth;
    img->trueHeight = img->curHeight;

    return 0;
}

static int LoadImageAndRotate(PhoImage* img)
{
    int e;
    int rot = (img ? img->curRot : 0);
    int firsttime = (img && (img->trueWidth == 0));

    if (!img) return -1;

    img->trueWidth = img->trueHeight = img->curRot = 0;

    e = LoadImageFromFile(img);
    if (e) return e;

    /* If it's not the first time we've loaded this image,
     * default its rotation to the EXIF rotation if any.
     * Otherwise rotate to the saved img->curRot.
     */
    if (firsttime && img->exifRot != 0)
        ScaleAndRotate(gCurImage, img->exifRot);

    else
        ScaleAndRotate(gCurImage, rot);

    return 0;
}

/* ThisImage() is called when gCurImage has changed and needs to
 * be reloaded.
 */
int ThisImage()
{
    if (LoadImageAndRotate(gCurImage) != 0)
        return NextImage();
    ShowImage();
    return 0;
}

int NextImage()
{
    int retval = 0;
    int looping = 0;
    if (gDebug)
        printf("\n================= NextImage ====================\n");

    /* Loop, since images may fail to load
     * and may need to be deleted from the list
     */
    while (1)
    {
        if (gFirstImage == 0)
            /* There's no list! How can we go to the next item? */
            return -1;

        if (gCurImage == 0) {  /* no image loaded yet, first call */
            if (gDebug)
                printf("NextImage: going to first image\n");
            gCurImage = gFirstImage;
        }

        else if (looping && gCurImage->next == gFirstImage)
            /* We're to the end of the list, after deleting something bogus */
            return -1;

        else if (!gCurImage->next || (gCurImage->next == gFirstImage))
            /* We're at the end of the list, can't go farther.
             * However, we may have gotten here by trying to go to
             * the next image and failing, in which case we no longer
             * have a pixmap loaded. So we still need to LoadImage,
             * but we'll want to return -1 to indicate we didn't progress.
             */
            if (gRepeat)
                gCurImage = gFirstImage;
            else
                retval = -1;

        /* We only want to go to ->next the first time;
         * if we're looping because of an error, gCurImage is already set.
         */
        else if (!looping)
            gCurImage = gCurImage->next;

        if (LoadImageAndRotate(gCurImage) == 0) {   /* Success! */
            ShowImage();
            return retval;
        }

        /* The image didn't load. Remove it from the list.
         * That means we're going to loop around and try again,
         * so keep gCurImage where it is (but change gCurImage->next).
         */
        if (gDebug)
            printf("Skipping '%s' (didn't load)\n", gCurImage->filename);
        DeleteItem(gCurImage);
        looping = 1;
    }
    /* NOTREACHED */
    return 0;
}

int PrevImage()
{
    if (gDebug)
        printf("\n================= PrevImage ====================\n");
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
    } while (LoadImageAndRotate(gCurImage) != 0);
    ShowImage();
    return 0;
}

/* Limit new_width and new_height so that they're no bigger than
 * max_width and max_height. This doesn't actually scale, just
 * calculates dimensions and returns them in *width and *height.
 */
static void ScaleToFit(int *width, int *height,
                       int max_width, int max_height,
                       int scaleMode, double scaleRatio)
{
    int new_w, new_h;

    if (scaleMode == PHO_SCALE_SCREEN_RATIO) {
        /* Which dimension needs to be scaled down more? */
        double xratio = (double)(*width) / max_width;
        double yratio = (double)(*height) / max_height;

        if (xratio > 1. || yratio > 1.) {    /* need some scaling */
            if (xratio > yratio) {  /* X needs more scaling down */
                new_w = *width / xratio;
                new_h = *height / xratio;
            } else {                /* Y needs more scaling down */
                new_w = *width / yratio;
                new_h = *height / yratio;
            }
        }
    }
    else {
        double xratio, yratio;
        if (scaleMode == PHO_SCALE_FIXED) {
            /* Special case: scaleRatio isn't actually a ratio,
             * it's the max size we want the image's long dimension to be.
             */
            if (*width > *height) {
                new_w = scaleRatio;
                new_h = scaleRatio * *height / *width;
            }
            else {
                new_w = scaleRatio * *width / *height;
                new_h = scaleRatio;
            }
            /* Reset scaleRatio so it can now be used for (no) scaling */
            scaleRatio = 1.0;
        }
        else {
            new_w = *width;
            new_h = *height;
        }
        new_w *= scaleRatio;
        new_h *= scaleRatio;
        xratio = (double)new_w / (double)max_width;
        yratio = (double)new_h / (double)max_height;
        if (xratio > 1. || yratio > 1.) {
            if (xratio > yratio) {
                new_w /= xratio;
                new_h /= xratio;
            } else {
                new_w /= yratio;
                new_h /= yratio;
            }
        }
    }

    *width = new_w * scaleRatio;
    *height = new_h * scaleRatio;
}

#define SWAP(a, b) { int temp = a; a = b; b = temp; }
/*#define SWAP(a, b)  {a ^= b; b ^= a; a ^= b;}*/

/* Rotate the image according to the current scale mode, scaling as needed,
 * then redisplay.
 * 
 * This will read the image from disk if necessary,
 * and it will rotate the image at the appropriate time
 * (when the image is at its smallest).
 *
 * This is the routine that should be called by external callers:
 * callers should never need to call RotateImage.
 *
 * degrees is the increment from the current rotation (curRot).
 */
int ScaleAndRotate(PhoImage* img, int degrees)
{
#define true_width img->trueWidth
#define true_height img->trueHeight
    int new_width;
    int new_height;

    if (gDebug)
        printf("ScaleAndRotate(%d (cur = %d))\n", degrees, img->curRot);

    /* degrees should be between 0 and 360 */
    degrees = (degrees + 360) % 360;

    /* First, load the image if we haven't already, to get true w/h */
    if (true_width == 0 || true_height == 0) {
        if (gDebug) printf("Loading, first time, from ScaleAndRotate!\n");
        LoadImageFromFile(img);
    }

    /*
     * Calculate new_width and new_height, the size to which the image
     * should be scaled before or after rotation,
     * based on the current scale mode.
     * That means that if the aspect ratio is changing,
     * new_width will be the image's height after rotation.
     */

    /* Fullsize: display always at real resolution,
     * even if it's too big to fit on the screen.
     */
    if (gScaleMode == PHO_SCALE_FULLSIZE) {
        new_width = true_width * gScaleRatio;
        new_height = true_height * gScaleRatio;
        if (gDebug) printf("Now fullsize, %dx%d\n", new_width, new_height);
    }

    /* Normal: display at full size unless it won't fit the screen,
     * in which case scale it down.
     */
#define NORMAL_SCALE_SLOP 5
    else if (gScaleMode == PHO_SCALE_NORMAL
             || gScaleMode == PHO_SCALE_SCREEN_RATIO
             || gScaleMode == PHO_SCALE_FIXED)
    {
        int max_width, max_height;
        int aspect_changing;    /* Is the aspect ratio changing? */

        new_width = true_width;
        new_height = true_height;

        aspect_changing = ((degrees % 180) != 0);
        if (aspect_changing) {
            max_width = gMonitorHeight;
            max_height = gMonitorWidth;
            if (gDebug)
                printf("Aspect ratio is changing\n");
        } else {
            max_width = gMonitorWidth;
            max_height = gMonitorHeight;
        }

        /* If we're in fixed mode, make sure we've set the "scale ratio"
         * to the screen size:
         */
        if (gScaleMode == PHO_SCALE_FIXED && gScaleRatio == 0.0)
            gScaleRatio = FracOfScreenSize();

        if (new_width > max_width || new_height > max_height) {
            ScaleToFit(&new_width, &new_height, max_width, max_height,
                       gScaleMode, gScaleRatio);
        }

#if 0
        /* Special case: if in PHO_SCALE_SCREEN_RATIO, we don't need to
         * scale verticals (assuming a landscape screen) down -- it's
         * better to keep the max dimension of each image the same.
         */
        if (new_width <= max_height && new_height <= max_width) {
            double r = 
        } else {
            new_width *= gScaleRatio;
            new_height *= gScaleRatio;
        }
#endif

        /* See if new_width and new_height are close enough already
         * that it might not be worth doing the work of scaling:
         */
        if (abs(img->curWidth - new_width) + abs(img->curHeight - new_height)
            < NORMAL_SCALE_SLOP) {
            new_width = img->curWidth;
            new_height = img->curHeight;
        }
    }

    else if (gScaleMode == PHO_SCALE_IMG_RATIO) {
        new_width = true_width * gScaleRatio;
        new_height = true_height * gScaleRatio;

        /* See if we're close */
        if (abs(img->curWidth - new_width) + abs(img->curHeight - new_height)
            < NORMAL_SCALE_SLOP) {
            new_width = img->curWidth;
            new_height = img->curHeight;
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
        int diffx = abs(img->curWidth - gMonitorWidth);
        int diffy = abs(img->curHeight - gMonitorHeight);
        double xratio, yratio;
        gint screenwidth, screenheight;

        if (gDisplayMode == PHO_DISPLAY_PRESENTATION)
            gtk_window_get_size(GTK_WINDOW(gWin), &screenwidth, &screenheight);
        else {
            screenwidth = gMonitorWidth;
            screenheight = gMonitorHeight;
        }

        if (diffx < FULLSCREEN_SCALE_SLOP || diffy < FULLSCREEN_SCALE_SLOP) {
            new_width = img->curWidth;
            new_height = img->curHeight;
        }
        else {
            xratio = (double)screenwidth / true_width;
            yratio = (double)screenheight / true_height;

            /* Use xratio for the more extreme of the two */
            if (xratio > yratio) xratio = yratio;
            new_width = xratio * true_width;
            new_height = xratio * true_height;
        }
    }
    else {
        /* Shouldn't ever happen, means gScaleMode is bogus */
        printf("Internal error: Unknown scale mode %d\n", gScaleMode);
        new_width = img->curWidth;
        new_height = img->curHeight;
    }
    /*
     * Finally, we're done with the scaling modes.
     * Time to do the scaling and rotation,
     * reloading the image if needed.
     */

    /* First figure out if we're getting bigger and hence need to reload. */
    if ((new_width > img->curWidth || new_height > img->curHeight)
        && (img->curWidth < true_width && img->curHeight < true_height)) {
        if (gDebug)
            printf("Getting bigger, from %dx%d to %dx%d -- need to reload\n",
                   img->curWidth, img->curHeight, new_width, new_height);

        /* Because curRot is going back to zero, that means we
         * might need to swap new_width and new_height, in case
         * the aspect ratio is changing.
        if (degrees % 180 != 0) {
        if ((img->curRot + degrees) % 180 != 0) {
         * new_width and new_height have already taken into account
         * the desired rotation (degrees); the important thing is
         * whether the current rotation is 90 degrees off.
         */
        if (img->curRot % 180 != 0) {
            SWAP(new_width, new_height);
            if (gDebug)
                printf("Swapping new width/height: %dx%d\n",
                       new_width, new_height);
        }

        /* image->curRot is going to be set back to zero when we load
         * from file, so adjust current planned rotation accordingly.
         */
        degrees = (degrees + img->curRot + 360) % 360;
            /* Now it's the absolute end rot desired */

        img->curRot = 0;
        LoadImageFromFile(img);
    }
#if 0
    else if (degrees % 180 != 0) {
        SWAP(new_width, new_height);
        printf("Not reloading, but swapped new width/height, now %dx%d\n",
               new_width, new_height);
    }

    /* new_* are the sizes we want after rotation. But we need
     * to compare them now to the sizes before rotation. So we
     * need to compensate for rotation.
     */

    /* If we're going to scale up, then do the rotation first,
     * before scaling. Otherwise, scale down first then rotate.
     */
    if (degrees != 0 &&
        (new_width > img->curWidth || new_height > img->curHeight)) {
        if (gDebug) printf("Rotating before scaling up\n");
        RotateImage(img, degrees);
        degrees = 0;    /* finished with rotation */
    }
#endif

    /* Do the scaling (thought we'd never get there!) */
    if (new_width != img->curWidth || new_height != img->curHeight)
    {
        GdkPixbuf* newimage = gdk_pixbuf_scale_simple(gImage,
                                                      new_width,
                                                      new_height,
                                                      GDK_INTERP_BILINEAR);
        /* If that's too slow use GDK_INTERP_NEAREST */

        /* scale_simple apparently has no error return; if it fails,
         * it still returns a pixbuf but width and height are -1.
         * Hope this undocumented behavior doesn't change!
         * Later: now it's documented. Whew.
         */
        if (!newimage || gdk_pixbuf_get_width(newimage) < 1) {
            if (newimage)
                g_object_unref(newimage);
            printf("\007Error scaling from %d x %d to %d x %d: probably out of memory\n",
                   img->curWidth, img->curHeight, new_width, new_height);
            Prompt("Couldn't scale up: probably out of memory", "Bummer", 0,
                   "\n ", "");
            return -1;
        }
        if (gDebug)
            printf("Scale from %dx%d = %dx%d to %dx%d = %dx%d\n",
                   img->curWidth, img->curHeight,
                   gdk_pixbuf_get_width(gImage), gdk_pixbuf_get_height(gImage),
                   new_width, new_height,
                   gdk_pixbuf_get_width(newimage),
                   gdk_pixbuf_get_height(newimage));
        if (gImage)
            g_object_unref(gImage);
        gImage = newimage;

        img->curWidth = gdk_pixbuf_get_width(gImage);
        img->curHeight = gdk_pixbuf_get_height(gImage);
    }

    /* If we didn't rotate before, do it now. */
    if (degrees != 0)
        RotateImage(img, degrees);

    /* We've finished making our changes. Now we may need to make
     * changes in the window size or position.
     */
    PrepareWindow();
    return 0;
}

PhoImage* NewPhoImage(char* fnam)
{
    PhoImage* newimg = calloc(1, sizeof (PhoImage));
    if (newimg == 0) return 0;
    newimg->filename = fnam;  /* no copy, we don't own the memory */

    return newimg;
}

void ReallyDelete(PhoImage* delImg)
{
    /* Make sure the keywords dialog doesn't save a pointer to this image */
    NoCurrentKeywords();

    if (unlink(delImg->filename) < 0)
    {
        printf("OOPS!  Can't delete %s\n", delImg->filename);
        return;
    }

    DeleteItem(delImg);

    /* If we just deleted the last image, all we can do is quit */
    if (!gFirstImage)
        EndSession();

    ThisImage();
}

void DeleteImage(PhoImage* delImg)
{
    char buf[512];
    if (delImg->filename == 0)
        return;
    sprintf(buf, "Delete file %s?", delImg->filename);
    if (Prompt(buf, "Delete", 0, "dD\n", "nN") > 0)
        ReallyDelete(delImg);
}

/* RotateImage just rotates an existing image, no scaling or reloading.
 * It's typically called from ScaleAndRotate either just
 * before or just after scaling.
 * No one except ScaleAndRotate should call it.
 * Degrees is the amount of rotation relative to current.
 */
static int RotateImage(PhoImage* img, int degrees)
{
    guchar *oldpixels, *newpixels;
    int x, y;
    int oldrowstride, newrowstride, nchannels, bitsper, alpha;
    int newWidth, newHeight, newTrueWidth, newTrueHeight;
    GdkPixbuf* newImage;

    if (!gImage) return 1;     /* sanity check */

    if (gDebug)
        printf("RotateImage(%d), initially %d x %d, true %dx%d\n",
               degrees, img->curWidth, img->curHeight,
               img->trueWidth, img->trueHeight);

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
                newpixels[newy*newrowstride + newx*nchannels + i]
                    = oldpixels[y*oldrowstride + x*nchannels + i];
        }
    }

    img->curWidth = newWidth;
    img->curHeight = newHeight;
    img->trueWidth = newTrueWidth;
    img->trueHeight = newTrueHeight;

    img->curRot = (img->curRot + degrees + 360) % 360;

    g_object_unref(gImage);
    gImage = newImage;

    return 0;
}

void Usage()
{
    printf("pho version %s.  Copyright 2002-2009 Akkana Peck akkana@shallowsky.com.\n", VERSION);
    printf("Usage: pho [-dhnp] image [image ...]\n");
    printf("\t-p:  Presentation mode (full screen, centered)\n");
    printf("\t-p[resolution]: Projector mode:\n\tlike presentation mode but in upper left corner\n");
    printf("\t-P:  No presentation mode (separate window) -- default\n");
    printf("\t-k:  Keywords mode (show a Keywords dialog for each image)\n");
    printf("\t-n:  Replace each image window with a new window (helpful for some window managers)\n");
    printf("\t-sN: Slideshow mode, where N is the timeout in seconds\n");
    printf("\t-r:  Repeat: loop back to the first image after showing the last\n");
    printf("\t-cpattern: Caption/Comment file pattern, format string for reworking filename\n");
    printf("\t--:  Assume no more flags will follow\n");
    printf("\t-d:  Debug messages\n");
    printf("\t-h:  Help: Print this summary\n");
    printf("\t-v:  Verbose help: Print a summary of key bindings\n");
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
    printf("  (In keywords dialog, alt + 0-9 adds 10, e.g. alt-4 triggers flag 14.\n");
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
    printf("o\tChange the working file set (add files or make a new list)\n");
    printf("g\tRun gimp on the current image\n");
    printf("\t(or set PHO_REMOTE to an alternate command)\n");
    printf("q\tQuit\n");
    printf("<esc>\tQuit (or hide a dialog, if one is showing)\n");
    printf("\n");
    printf("Pho mouse bindings\n");
    printf("In presentation mode: drag with middlemouse to pan/move.\n");
    exit(1);
}







