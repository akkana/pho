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

GdkPixbuf* image = 0;

int resized = 0;
int Debug = 0;

int ArgC, ArgP;
char** ArgV;

int XSize = 500;
int YSize = 300;

int MonitorWidth = 1600;
int MonitorHeight = 1200;

static int numImages = 0;

int LoadImageFromFile()
{
    if (image)
        gdk_pixbuf_unref(image);
    image = gdk_pixbuf_new_from_file(ArgV[ArgP]);
    if (!image)
    {
        fprintf(stderr, "Can't open %s\n", ArgV[ArgP]);
        return 1;
    }

    if (ArgP > numImages)
        ++numImages;

    // Rotate if necessary:
    FindImgNote(ArgP);
    if (curNote && curNote->rotation != 0)
        RotateImage(curNote->rotation);

    return 0;
}

int NextImage()
{
    do {
        // Go to next image
        if (ArgP >= ArgC-1)
            return -1;
        ++ArgP;
    } while (LoadImageFromFile() != 0);
    return 0;
}

int PrevImage()
{
    do {
        // Go to previous image
        if (ArgP <= 1)
            return -1;
        --ArgP;
    } while (LoadImageFromFile() != 0);
    return 0;
}

void FreePixels(guchar* pixels, gpointer data)
{
    free(pixels);
}

void DeleteImage()
{
    FindImgNote(ArgP);
    if (!curNote) return;
    curNote->deleted = 1;
    ShowDeleteDialog();
}

void ReallyDelete()
{
    unlink(ArgV[ArgP]);
    NextImage();
    ShowImage();
}

int RotateImage(int degrees)
{
    guchar *oldpixels, *newpixels;
    int x, y;
    int temp;
    int oldrowstride, newrowstride, nchannels, bitsper, alpha;
    struct ImgNotes_s* imgnote;

    // If we ever rotate it, we'll definitely need notes on this image:
    FindImgNote(ArgP);
    imgnote = curNote;

    // If we've resized it smaller than original, but after
    // rotating it we'd be able to show it bigger than current size,
    // then read in the original first before rotating.
    if (resized && degrees != 180
        && YSize < MonitorWidth && XSize < MonitorHeight)
    {
        if (image)
            gdk_pixbuf_unref(image);
        image = gdk_pixbuf_new_from_file(ArgV[ArgP]);
        if (!image) return 1;
        XSize = gdk_pixbuf_get_width(image);
        YSize = gdk_pixbuf_get_height(image);
        degrees = (degrees + imgnote->rotation) % 360;
        imgnote->rotation = 0;
    }

    // degrees might be zero now, since we might have just
    // read from disk and might now be rotating back to zero.
    if (degrees == 0)
    {
        ShowImage();
        return 0;
    }

    oldrowstride = gdk_pixbuf_get_rowstride(image);
    bitsper = gdk_pixbuf_get_bits_per_sample(image);
    nchannels = gdk_pixbuf_get_n_channels(image);
    alpha = gdk_pixbuf_get_has_alpha(image);
    if (degrees == 90 || degrees == -90 || degrees == 270)
        newrowstride = oldrowstride * YSize / XSize;
    else
        newrowstride = oldrowstride;

    oldpixels = gdk_pixbuf_get_pixels(image);
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
                newpixels[newy*newrowstride + newx*nchannels + i]
                    = oldpixels[y*oldrowstride + x*nchannels + i];
        }
    }

    if (degrees == 90 || degrees == -90 || degrees == 270)
    {
          temp = XSize;
          XSize = YSize;
          YSize = temp;
    }

    imgnote->rotation = (imgnote->rotation + degrees + 360) % 360;

    if (image)
        gdk_pixbuf_unref(image);
    image = gdk_pixbuf_new_from_data(newpixels,
                                     GDK_COLORSPACE_RGB, alpha, bitsper,
                                     XSize, YSize, newrowstride,
                                     FreePixels, newpixels);
    if (!image) return 1;

    ShowImage();
    return 0;
}

void Usage()
{
    printf("pho version 0.5.  Copyright 2002 Akkana Peck.\n");
    printf("Usage: pho [-d] image [image ...]\n");
    exit(1);
}

