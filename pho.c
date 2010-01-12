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

int gPresentationMode = 0;

static void ScaleImage(PhoImage* img);  /* forward */

int ShowImage()
{
    ScaleImage(gCurImage);
    PrepareWindow();
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

    img->trueWidth = gdk_pixbuf_get_width(gImage);
    img->trueHeight = gdk_pixbuf_get_height(gImage);

    /* do any necessary rotation */
    ExifReadInfo(img->filename);
    if (img->rotation) {
        rot = img->rotation;
        img->rotation = 0;
        RotateImage(rot);
    }
    else if (HasExif() && (rot = ExifGetInt(ExifOrientation)) != 0) {
        RotateImage(rot);
    }

    img->curWidth = img->trueWidth;
    img->curHeight = img->trueHeight;
    return 0;
}

int ThisImage()
{
    if (gScaleMode == PHO_SCALE_ABSSIZE)
        gScaleMode = PHO_SCALE_NORMAL;
    if (LoadImageFromFile(gCurImage) != 0)
        return NextImage();
    ShowImage();
    return 0;
}

int NextImage()
{
    if (gScaleMode == PHO_SCALE_ABSSIZE)
        gScaleMode = PHO_SCALE_NORMAL;
    do {
        if (gCurImage == 0)   /* no image loaded yet, first call */
            gCurImage = gFirstImage;
        else if (gCurImage->next == 0 || gCurImage->next == gFirstImage)
            return -1;  /* end of list, can't go farther */
        else
            gCurImage = gCurImage->next;
    } while (LoadImageFromFile(gCurImage) != 0);
    ShowImage();
    return 0;
}

int PrevImage()
{
    if (gScaleMode == PHO_SCALE_ABSSIZE)
        gScaleMode = PHO_SCALE_NORMAL;
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

/* Scale the image according to the current scale mode.
 * Any rotation has already happened before calling Scale.
 */
static void ScaleImage(PhoImage* img)
{
    /* Absolute Size: size has already been set, just follow it.
     */
    if (gScaleMode == PHO_SCALE_ABSSIZE) {
#define ABS_SCALE_SLOP 5
        int curW = gdk_pixbuf_get_width(gImage);
        int curH = gdk_pixbuf_get_height(gImage);
        GdkPixbuf* newimage;

        if (img->curWidth > curW + ABS_SCALE_SLOP
            || img->curHeight > curH + ABS_SCALE_SLOP) {
            /* It's getting bigger, so re-read from the file before scaling  */
            int w = img->curWidth;
            int h = img->curHeight;
            LoadImageFromFile(img);
            img->curWidth = w;
            img->curHeight = h;
        }
        newimage = gdk_pixbuf_scale_simple(gImage,
                                           img->curWidth, img->curHeight,
                                           GDK_INTERP_NEAREST);
        if (gImage)
            gdk_pixbuf_unref(gImage);
        gImage = newimage;
        return;
    }

    /* Fullsize: display always at real resolution,
     * even if it's too big to fit on the screen.
     */
    if (gScaleMode == PHO_SCALE_FULLSIZE) {
        if (img->curWidth == img->trueWidth &&
            img->curHeight == img->trueHeight)
            return;
        LoadImageFromFile(img);
        return;
    }

    /* Normal: display at full size unless it won't fit the screen,
     * in which case scale it down.
     */
#define NORMAL_SCALE_SLOP 5
    if (gScaleMode == PHO_SCALE_NORMAL) {
        int w = img->trueWidth;
        int h = img->trueHeight;
        int diff;
        GdkPixbuf* newimage;
        if (w > gMonitorWidth || h > gMonitorHeight) {
            double xratio = (double)gMonitorWidth / img->trueWidth;
            double yratio = (double)gMonitorHeight / img->trueHeight;
            /* Use xratio for the most extreme */
            if (xratio > yratio) xratio = yratio;
            w = xratio * img->trueWidth;
            h = xratio * img->trueHeight;
        }
        /* Now w and h hold the desired sizes.  See if we're close */
        diff = abs(img->curWidth - w) + abs(img->curHeight - h);
        if (diff < NORMAL_SCALE_SLOP)
            return;

        /* We have to rescale */
        newimage = gdk_pixbuf_scale_simple(gImage, w, h, GDK_INTERP_NEAREST);
        if (gImage)
            gdk_pixbuf_unref(gImage);
        gImage = newimage;
        img->curWidth = w;
        img->curHeight = h;
        return;
    }
        
    /* Fullscreen: Scale either up or down if necessary to make
     * the largest dimension match the screen size.
     */
#define FULLSCREEN_SCALE_SLOP 20
    if (gScaleMode == PHO_SCALE_FULLSCREEN) {
        int diffx = abs(gCurImage->curWidth - gMonitorWidth);
        int diffy = abs(gCurImage->curHeight - gMonitorHeight);
        double xratio, yratio;
        GdkPixbuf* newimage;

        if (diffx < FULLSCREEN_SCALE_SLOP || diffy < FULLSCREEN_SCALE_SLOP)
            return;
        xratio = (double)gMonitorWidth / img->trueWidth;
        yratio = (double)gMonitorHeight / img->trueHeight;

        /* Use xratio for the most extreme */
        if (xratio > yratio) xratio = yratio;
        gCurImage->curWidth = xratio * img->trueWidth;
        gCurImage->curHeight = yratio * img->trueHeight;
        newimage = gdk_pixbuf_scale_simple(gImage,
                                           gCurImage->curWidth,
                                           gCurImage->curHeight,
                                           GDK_INTERP_NEAREST);
        if (gImage)
            gdk_pixbuf_unref(gImage);
        gImage = newimage;
    }
}

PhoImage* NewPhoImage(char* fnam)
{
    PhoImage* newimg = calloc(1, sizeof (PhoImage));
    if (newimg == 0) return 0;
    newimg->filename = fnam;  /* no copy, we don't own the memory */

    return newimg;
}

/* XXX ReallyDelete no workee, doesn't go to the right shot afterward */
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
    if (img->comment) free(img->comment);
    free(img);

    ThisImage();
}

static void FreePixels(guchar* pixels, gpointer data)
{
    free(pixels);
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

#define IsResized(img)  (((img)->curWidth != (img)->trueWidth) || ((img)->curHeight != (img)->trueHeight))

#define IsSmaller(img)  (((img)->curWidth < (img)->trueWidth) || ((img)->curHeight < (img)->trueHeight))

int RotateImage(int degrees)
{
    guchar *oldpixels, *newpixels;
    int x, y;
    int oldrowstride, newrowstride, nchannels, bitsper, alpha;
#if GTK_MAJOR_VERSION == 2
    GError* err = NULL;
#endif

    // If we've resized it smaller than original, but after
    // rotating it we'd be able to show it bigger than current size,
    // then read in the original first before rotating.
    if (IsSmaller(gCurImage) && degrees != 180
        && (gCurImage->curWidth < gMonitorWidth
            || gCurImage->curHeight < gMonitorHeight))
    {
        if (gImage) {
            gdk_pixbuf_unref(gImage);
	    gImage = 0;
	}
#if GTK_MAJOR_VERSION == 2
        gImage = gdk_pixbuf_new_from_file(gCurImage->filename, &err);
#else
        gImage = gdk_pixbuf_new_from_file(gCurImage->filename);
#endif
        if (gImage == 0) return 1;
        gCurImage->trueWidth = gdk_pixbuf_get_width(gImage);
        gCurImage->trueHeight = gdk_pixbuf_get_height(gImage);
        gCurImage->curWidth = gCurImage->trueWidth;
        gCurImage->curHeight = gCurImage->trueHeight;

        degrees = (degrees + gCurImage->rotation) % 360;
        gCurImage->rotation = 0;
    }

    // degrees might be zero now, since we might have just
    // read from disk and might now be rotating back to zero.
    if (degrees == 0)
    {
        ShowImage();
        return 0;
    }

    oldrowstride = gdk_pixbuf_get_rowstride(gImage);
    bitsper = gdk_pixbuf_get_bits_per_sample(gImage);
    nchannels = gdk_pixbuf_get_n_channels(gImage);
    alpha = gdk_pixbuf_get_has_alpha(gImage);
    if (degrees == 90 || degrees == -90 || degrees == 270)
        newrowstride = oldrowstride
            * gCurImage->curHeight / gCurImage->curWidth;
    else
        newrowstride = oldrowstride;

    oldpixels = gdk_pixbuf_get_pixels(gImage);
    newpixels = malloc(gCurImage->curWidth * gCurImage->curHeight * nchannels);

    for (x = 0; x < gCurImage->curWidth; ++x)
    {
        for (y = 0; y < gCurImage->curHeight; ++y)
        {
            int newx, newy;
            int i;
            switch (degrees)
            {
              case 90:
                newx = gCurImage->curHeight - y - 1;
                newy = x;
                break;
              case -90:
              case 270:
                newx = y;
                newy = gCurImage->curWidth - x - 1;
                break;
              case 180:
                newx = gCurImage->curWidth - x - 1;
                newy = gCurImage->curHeight - y - 1;
                break;
              default:
                printf("Illegal rotation value!\n");
                return 1;
            }
            for (i=0; i<nchannels; ++i)
            {
                //printf("(%d, %d) -> (%d, %d) # %d\n", x, y, newx, newy, i);
                newpixels[newy*newrowstride + newx*nchannels + i]
                    = oldpixels[y*oldrowstride + x*nchannels + i];
            }
        }
    }

    // Swap X and Y if appropriate.
    if (degrees == 90 || degrees == -90 || degrees == 270)
    {
        int temp;
        temp = gCurImage->curWidth;
        gCurImage->curWidth = gCurImage->curHeight;
        gCurImage->curHeight = temp;
        temp = gCurImage->trueWidth;
        gCurImage->trueWidth = gCurImage->trueHeight;
        gCurImage->trueHeight = temp;
    }

    gCurImage->rotation = (gCurImage->rotation + degrees + 360) % 360;

    if (gImage)
        gdk_pixbuf_unref(gImage);
    gImage = gdk_pixbuf_new_from_data(newpixels,
                                     GDK_COLORSPACE_RGB, alpha, bitsper,
                                     gCurImage->curWidth, gCurImage->curHeight,
                                      newrowstride,
                                     FreePixels, newpixels);
    if (!gImage) return 1;

    ShowImage();
    return 0;
}

void Usage()
{
    printf("pho version %s.  Copyright 2002,2003,2004 Akkana Peck akkana@shallowsky.com.\n", VERSION);
    printf("Usage: pho [-dhnp] image [image ...]\n");
    printf("\t-p: Presentation mode (full screen)\n");
    printf("\t-n: Replace each image window with a new window (helpful for some window managers)\n");
    printf("\t-d: Debug messages\n");
    printf("\t-h: Help: Print this summary\n");
    printf("\t-v: Verbose help: Print a summary of key bindings\n");
    exit(1);
}

void VerboseHelp()
{
    printf("pho version %s.  Copyright 2002,2003,2004 Akkana Peck akkana@shallowsky.com.\n", VERSION);
    printf("Type pho -h for commandline arguments.\n");
    printf("\npho Key Bindings:\n\n");
    printf("<space>\tNext image\n");
    printf("-\tPrevious image\n");
    printf("<backspace>\tPrevious image\n");
    printf("<home>\tFirst image\n");
    printf("F\tFull-size mode (even if bigger than screen)\n");
    printf("f\tFullscreen mode (scale even small images up to fullscreen)\n");
    printf("p\tPresentation mode (take up the whole screen, centering the image)\n");
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







