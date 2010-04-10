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

/* Delete an image, or gCurImage if item == 0.
 * Update gCurImage if need be.
 */
void DeleteItem(PhoImage* item)
{
    if (!item)
        item = gCurImage;

        /* Is this the only image? */
    if (item == gFirstImage && item->next == gFirstImage) {
        gFirstImage = gCurImage = 0;
    }
    else {
        PhoImage* newitem = item->next;

        newitem->prev = item->prev;
        newitem->prev->next = newitem;

        /* Only change gCurImage if item was the current image. */
        if (item == gCurImage) {
            /* If we just deleted the last item, newitem will be gFirstImage.
             * But we don't really want to loop around; we should stop
             * upon reaching the last image, and stay on the new last image.
             * Of course, this assumes that there's more than one item left.
             */
            if (newitem == gFirstImage && newitem->prev != gFirstImage)
                gCurImage = newitem->prev;
            else
                gCurImage = newitem;
        }

        if (item == gFirstImage)
            gFirstImage = newitem;
    }

    /* It's disconnected.  Free all the memory */
    FreePhoImage(item);
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

