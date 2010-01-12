/*
 * pho.c: an image viewer.
 *
 * This file contains the image manipulation routines.
 *
 * Copyright 2002 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include <stdlib.h>       // for free()
#include <stdio.h>
#include <string.h>
#include <unistd.h>       // for unlink()

#include "pho.h"

GdkPixbuf* gImage = 0;

int resized = 0;
int Debug = 0;

int ArgC, ArgP;
char** ArgV;

int XSize = 500;
int YSize = 300;
int realXSize, realYSize;

int MonitorWidth = 1600;
int MonitorHeight = 1200;

static int LoadImageFromFile(int index)
{
    struct ImgNotes_s *curNote;

    if (IsDeleted(index))
        return -1;

    curNote = FindImgNote(index);

    if (gImage)
        gdk_pixbuf_unref(gImage);
    gImage = gdk_pixbuf_new_from_file(ArgV[index]);
    if (!gImage)
    {
        fprintf(stderr, "Can't open %s\n", ArgV[index]);
        return -1;
    }

    XSize = realXSize = gdk_pixbuf_get_width(gImage);
    YSize = realYSize = gdk_pixbuf_get_height(gImage);

    resized = 0;

    // Rotate if necessary:
    if (curNote && curNote->rotation != 0)
    {
        int rot = curNote->rotation;
        curNote->rotation = 0;
        RotateImage(rot);
    }

    return 0;
}

int NextImage()
{
    do {
        // Go to next image
        if (ArgP >= ArgC-1)
            return -1;
        ++ArgP;
    } while (LoadImageFromFile(ArgP) != 0);
    return 0;
}

int PrevImage()
{
    do {
        // Go to previous image
        if (ArgP <= 1)
            return -1;
        --ArgP;
    } while (LoadImageFromFile(ArgP) != 0);
    return 0;
}

void FreePixels(guchar* pixels, gpointer data)
{
    free(pixels);
}

void ReallyDelete()
{
    if (unlink(ArgV[ArgP]) < 0)
    {
        printf("OOPS!  Can't delete %s\n", ArgV[ArgP]);
        return;
    }
    MarkDeleted(ArgP);
    if (NextImage() != 0)
        if (PrevImage() != 0)
            EndSession();
    ShowImage();
}

void DeleteImage()
{
    struct ImgNotes_s* curNote = FindImgNote(ArgP);
    char buf[512];

    if (!curNote) return;
    sprintf(buf, "Delete file %s?", ArgV[ArgP]);
    if (PromptDialog(buf, "Delete", 0, "dD\n", "nN") > 0)
        ReallyDelete();
}

int RotateImage(int degrees)
{
    guchar *oldpixels, *newpixels;
    int x, y;
    int oldrowstride, newrowstride, nchannels, bitsper, alpha;

    // If we ever rotate it, we'll definitely need notes on this image:
    struct ImgNotes_s* curNote = FindImgNote(ArgP);

    // If we've resized it smaller than original, but after
    // rotating it we'd be able to show it bigger than current size,
    // then read in the original first before rotating.
    if (resized && degrees != 180
        && (YSize < MonitorWidth || XSize < MonitorHeight))
    {
        if (gImage)
            gdk_pixbuf_unref(gImage);
        gImage = gdk_pixbuf_new_from_file(ArgV[ArgP]);
        if (!gImage) return 1;
        XSize = realXSize = gdk_pixbuf_get_width(gImage);
        YSize = realYSize = gdk_pixbuf_get_height(gImage);
        degrees = (degrees + curNote->rotation) % 360;
        curNote->rotation = 0;
        resized = 0;
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
        newrowstride = oldrowstride * YSize / XSize;
    else
        newrowstride = oldrowstride;

    oldpixels = gdk_pixbuf_get_pixels(gImage);
    // XXX This should only need XSize*YSize*nchannels, but for some reason 
    // that crashed  after trashing the stack when I was assuming nchannels=3.
    newpixels = malloc(XSize * YSize * (nchannels+1));

    for (x = 0; x < XSize; ++x)
    {
        for (y = 0; y < YSize; ++y)
        {
            int newx, newy;
            int i;
            switch (degrees)
            {
              case 90:
                newx = YSize - y;
                newy = x;
                break;
              case -90:
              case 270:
                newx = y;
                newy = XSize - x;
                break;
              case 180:
                newx = XSize - x;
                newy = YSize - y;
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
        temp = XSize;
        XSize = YSize;
        YSize = temp;
        temp = realXSize;
        realXSize = realYSize;
        realYSize = temp;
    }

    curNote->rotation = (curNote->rotation + degrees + 360) % 360;

    if (gImage)
        gdk_pixbuf_unref(gImage);
    gImage = gdk_pixbuf_new_from_data(newpixels,
                                     GDK_COLORSPACE_RGB, alpha, bitsper,
                                     XSize, YSize, newrowstride,
                                     FreePixels, newpixels);
    if (!gImage) return 1;

    ShowImage();
    return 0;
}

void Usage()
{
    printf("pho version 0.7.  Copyright 2002 Akkana Peck.\n");
    printf("Usage: pho image [image ...]\n");
    exit(1);
}

