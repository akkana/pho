/*
 * imagenote.c: save info about images, for pho, an image viewer.
 *
 * Copyright 2002 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include "pho.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>       /* for malloc() */
#include <glib.h>
#include <ctype.h>

static char *sFlagStrings[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

char* GetFlagString(PhoImage* img)
{
    static char buf[35];
    static char* spaces= "                                  ";
    int i, mask;
    int foundone = 0;

    if (img->noteFlags == 0) return spaces;
    mask = 1;
    for (i=0; i<10; ++i, mask <<= 1)
    {
        if (img->noteFlags & mask)
        {
            int len;
            if (!foundone)
                strcpy(buf, "Flags: ");
            len = strlen(buf);
            if (foundone)
                buf[len++] = ' ';
            buf[len++] = i + '0';
            buf[len] = '\0';
            foundone = 1;
        }
    }
    return buf;
}

void ToggleNoteFlag(PhoImage* img, int note)
{
    int bit = (1 << note);
    if (img->noteFlags & bit)
        img->noteFlags &= ~bit;
    else
        img->noteFlags |= bit;
}

char *QuoteString(char *str)
{
  int i;
  char *newstr;

  /*look for a space or quote in str */
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

    /* Should free up memory here, e.g. for sFlagStrings,
     * but since this is only called right before exit,
     * it's never been allocated so it doesn't matter.
     * If PrintNotes ever gets called except at exit,
     * this will leak.
     */

    img = gFirstImage;
    while (img)
    {
        if (img->comment)
            printf("%s: %s\n", img->filename, img->comment);
        if (img->noteFlags)
        {
            int flag, j;
            for (j=1, flag=1; j<=10; ++j)
            {
                flag <<= 1;
                if (img->noteFlags & flag)
                    AddImgToList(sFlagStrings+j, img->filename);
            }
        }

        switch (img->rotation)
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
    for (i=0; i<10; ++i)
        if (sFlagStrings[i])
            printf("\nNote %d: %s\n", i, sFlagStrings[i]);
}

