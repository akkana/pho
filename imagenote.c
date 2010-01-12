/*
 * imagenote.c: save info about images. For pho, an image viewer.
 *
 * Copyright 2002,2007 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include "pho.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>       /* for malloc() */
#include <glib.h>
#include <ctype.h>

static char *sFlagFileList[NUM_NOTES];

void InitNotes()
{
    int i;
    for (i=0; i<NUM_NOTES; ++i)
        sFlagFileList[i] = 0;
}

void ToggleNoteFlag(PhoImage* img, int note)
{
    int bit = (1 << note);
    if (img->noteFlags & bit)
        img->noteFlags &= ~bit;
    else
        img->noteFlags |= bit;

    /* Update any dialogs which might be showing toggles */
    SetInfoDialogToggle(note, (img->noteFlags & bit) != 0);
    SetKeywordsDialogToggle(note, (img->noteFlags & bit) != 0);
}

/* Guard against filenames which contain odd characters, like
 * spaces or quotes. Returns allocated memory.
 */
char *QuoteString(char *str)
{
    int i;
    char *newstr;

    /* look for a space or quote in str */
    for (i = 0; str[i] != '\0'; i++)
        if (isspace(str[i]) || (str[i] == '\"') || (str[i] == '\'')) {
            GString *gstr = g_string_new("\"");
            for (i = 0; str[i] != '\0'; i++) {
                if (str[i] == '\"')
                    g_string_append(gstr, (gchar *)"\\\"");
                /*else if (str[i] == '\'')
                  g_string_append(gstr, (gchar *)"\\\'");*/
                else
                    g_string_append_c(gstr, (gchar)str[i]);
            }

            g_string_append_c(gstr, '\"');

            newstr = gstr->str;
            g_string_free(gstr, FALSE);
            return newstr;
        }

    /*if there are no spaces or quotes in str, return a copy of str*/
    return (char *)g_strdup((gchar *)str);
}

void AddImgToList(char** strp, char* str)
{
    str = QuoteString(str);

    if (*strp)
    {
        char* newstr = malloc(strlen(*strp) + strlen(str) + 2);
        if (!newstr) return;
        strcpy(newstr, *strp);
        strcat(newstr, " ");
        strcat(newstr, str);
        free(*strp);
        *strp = newstr;
    }
    else
        *strp = strdup(str);

    free(str);
}

void PrintNotes()
{
    int i;
    char *rot90=0, *rot180=0, *rot270=0;
    PhoImage* img;

    /* Should free up memory here, e.g. for sFlagFileList,
     * but since this is only called right before exit,
     * it's never been allocated so it doesn't matter.
     * If PrintNotes ever gets called except at exit,
     * this will leak.
     */

    img = gFirstImage;
    while (img)
    {
        if (img->comment)
            printf("Comment %s: %s\n", img->filename, img->comment);
        if (img->noteFlags)
        {
            int flag, j;
            for (j=0, flag=1; j<NUM_NOTES; ++j, flag <<= 1)
                if (img->noteFlags & flag)
                    AddImgToList(sFlagFileList+j, img->filename);
        }

        switch (img->curRot)
        {
          case 90:
              AddImgToList(&rot90, img->filename);
              break;
          case 180:
              AddImgToList(&rot180, img->filename);
              break;
          case 270:
          case -90:
              AddImgToList(&rot270, img->filename);
              break;
          default:
              break;
        }

        img = img->next;
        /* Have we looped back to the beginning?*/
        if (img == gFirstImage) break;
    }

    /* Now we've looped over all the structs, so we can print out
     * the tables of rotation and notes.
     */
    if (rot90)
        printf("\nRotate 90 (CW): %s\n", rot90);
    if (rot270)
        printf("\nRotate -90 (CCW): %s\n", rot270);
    if (rot180)
        printf("\nRotate 180: %s\n", rot180);
    for (i=0; i < NUM_NOTES; ++i)
        if (sFlagFileList[i])
        {
            char* keyword = KeywordString(i);
            if (keyword && *keyword)
                printf("\n%s: ", keyword);
            else
                printf("\nNote %d: ", i);
            printf("%s\n", sFlagFileList[i]);
        }
}

