/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * phoimglist.c: list manipulation for PhoImage type.
 *
 * Copyright 2010 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

/* A PhoImage is a doubly-linked list, with ->next and ->prev.
 * The global gFirstImage marks the head of the list;
 * the list is circular, so the last item has next = gFirstImage.
 *
 * gCurImage points to the current list item.
 *
 * List items are freed with FreePhoImage()
 */

#include "pho.h"
#include <stdlib.h>

/* This routine exists to keep track of any allocated memory
 * existing in the PhoImage structure.
 */
static void FreePhoImage(PhoImage* img)
{
    if (img->comment) free(img->comment);
    free(img);
}

static void printImageList()
{
    if (! gFirstImage) {
        printf("  No images\n");
        return;
    }
    PhoImage* im = gFirstImage;
    PhoImage* lastIm = gFirstImage->prev;
    while (im) {
        if (im == gCurImage)
            printf("> %s\n", im->filename);
        else
            printf("  %s\n", im->filename);
        if (im == lastIm)
            break;
        im = im->next;
    }
    printf("\n");
}

/* Delete an image from the image list (not from disk).
 * Will use gCurImage if item == 0.
 * Update gCurImage if needed.
 */
void DeleteItem(PhoImage* item)
{
    if (gDebug) {
        printf("Removing image %s from image list\n", item->filename);
        printf("Image list before removal:\n");
        printImageList();
    }

    if (!item)
        item = gCurImage;

    /* Is this the only image? */
    if (item == gFirstImage && item->next == gFirstImage) {
        gFirstImage = gCurImage = 0;
    }
    else {
        /* Is it the first image? */
        if (item == gFirstImage) {
            if (gCurImage)
                gCurImage = item->next;
            gFirstImage = item->next;
        }

        /* Is it the last image? */
        else if (item == gFirstImage->prev) {
            gFirstImage->prev = item->prev;   // New last image
            item->prev->next = gFirstImage;
            if (gCurImage)
                gCurImage = item->prev;
        }
        else if (gCurImage)
            gCurImage = item->next;

        item->next->prev = item->prev;
        item->prev->next = item->next;
    }

    /* It's disconnected.  Free all the memory */
    FreePhoImage(item);

    /* What does the list look like now? */
    if (gDebug) {
        printf("Image list after removal:\n");
        printImageList();
    }
}

/* Append an item to the end of the list */
void AppendItem(PhoImage* item)
{
    PhoImage* last;

    if (!item)
        return;

    /* Is the list empty? */
    if (gFirstImage == 0) {
        gFirstImage = item;
        item->next = item->prev = item;
        return;
    }

    /* Is there only one item in the list? */
    if (gFirstImage->next == gFirstImage) {
        gFirstImage->next = gFirstImage->prev = item;
        item->next = item->prev = gFirstImage;
        return;
    }

    last = gFirstImage->prev;
    last->next = item;
    item->prev = last;
    item->next = gFirstImage;
    gFirstImage->prev = item;
}

/* Remove all images from the image list, to start fresh. */
void ClearImageList()
{
    PhoImage* img = gFirstImage;
    do {
        PhoImage* next = img->next;
        FreePhoImage(img);
        img = next;
    } while (img && img != gFirstImage);

    gCurImage = gFirstImage = 0;
}

