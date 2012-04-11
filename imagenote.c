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
#include <fcntl.h>
#include <unistd.h>    /* for write() */

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

    /* If there are no spaces or quotes in str, return a copy of str */
    return (char *)g_strdup((gchar *)str);
}

void AddImgToList(char** strp, char* str)
{
    str = QuoteString(str);

    if (*strp)
    {
        int newsize = strlen(*strp) + strlen(str) + 2;
        char* newstr = malloc(newsize);
        if (!newstr) {
            free(str);
            *strp = 0;
            return;
        }
        snprintf(newstr, newsize, "%s %s", *strp, str);
        free(*strp);
        *strp = newstr;
    }
    else
        *strp = strdup(str);

    /* QuoteString allocated a copy, so free that now: */
    free(str);
}


/* Captions:
 * captions may be stored in one file per image, or they may
 * all be in one file in lines like
 * imgname: caption
 * This is controlled by gCapFileFormat: if it has a %s in it,
 * that will be replaced by the current image file name.
 */

static int GlobalCaptionFile()
{
    char* cp;

    /* For captions, are we going to put them all in one file,
     * or in individual files? That's controlled by whether
     * gCapFileFormat includes a '%s" in it, i.e., will it
     * substitute the name of the image file inside caption filenames.
     */
    for (cp = gCapFileFormat; *cp != 0; ++cp) {
        if (cp[0] == '%' && cp[1] == 's')
            return 0;
    }

    /* If we got throught he loop without seeing %s, then the
     * caption file is global.
     */
    return 1;
}

/* Return the appropriate caption file name for this image */
static char* CapFileName(PhoImage* img)
{
    /* How much ram do we need for the caption filename? */
    static char buf[BUFSIZ];
    int caplength = snprintf(buf, BUFSIZ, gCapFileFormat, img->filename);
    if (strncmp(img->filename, buf, caplength) == 0) {
        fprintf(stderr,
           "Caption filename expanded to same as image filename. Bailing!\n");
        return 0;
    }
    return buf;
}

/* Read any caption that might be in the caption file.
 * If the caption file is global, though, we read the file once
 * for the first image and cache them.
 */
void ReadCaption(PhoImage* img)
{
    int i;
    int capfile;
    char* capfilename;

    static int sFirstTime = 1;
    static int sGlobalCaptions = 0;
    static char** sCaptionFileList = 0;
    static char** sCaptionList = 0;
    static int sNumCaptions = 0;

    if (sFirstTime) {
        sFirstTime = 0;
        sGlobalCaptions = GlobalCaptionFile();
        if (sGlobalCaptions) {
            /* Read the global file */
            char line[10000];
            int numlines = 0;

            FILE* capfile = fopen(gCapFileFormat, "r");

            if (!capfile)    /* No captions to read, nothing to do */
                return;

            /* Make a first pass through the file just to count lines */
            while (fgets(line, sizeof line, capfile)) {
                char* colon = strchr(line, ':');
                if (colon)
                    ++numlines;
            }

            /* Now we can allocate space for the lists of files/captions */
            sCaptionFileList = malloc(numlines * (sizeof (char*)));
            sCaptionList = malloc(numlines * (sizeof (char*)));

            /* and loop through again to actually read the captions */
            rewind(capfile);
            while (fgets(line, sizeof line, capfile)) {
                char* cp;

                /* Line should look like: imagename: blah blah */
                char* colon = strchr(line, ':');
                if (!colon) continue;

                /* terminate the filename string */
                *colon = '\0';
                sCaptionFileList[sNumCaptions] = strdup(line);

                /* Skip the colon and any spaces immediately after it */
                ++colon;
                while (*colon == ' ')
                    ++colon;
                /* strip off the terminal newline */
                for (cp = colon; *cp != '\0'; ++cp)
                    if (*cp == '\n' || *cp == '\r') {
                        *cp = '\0';
                        break;
                    }

                /* Now colon points to the beginning of the caption */
                sCaptionList[sNumCaptions] = strdup(colon);

                ++sNumCaptions;
            }
            fclose(capfile);
        }
    }

    /* Now we've done the first-time reading of the file, if needed. */
    if (sGlobalCaptions) {
        for (i=0; i < sNumCaptions; ++i)
            if (!strcmp(sCaptionFileList[i], img->filename)) {
                img->caption = sCaptionList[i];
                return;
            }
        img->caption = 0;
        return;
    }

    /* If we get here, caption files are per-image.
     * So look for the appropriate caption file.
     */

    capfilename = CapFileName(img);
    if (!capfilename)
        return;

    capfile = open(capfilename, O_RDONLY);
    if (capfile < 0)
        return;
    printf("Reading caption from %s\n", capfilename);

#define MAX_CAPTION 1023
    img->caption = calloc(1, MAX_CAPTION);
    if (!(img->caption)) {
        perror("Couldn't allocate memory for caption");
        return;
    } 
    read(capfile, img->caption, MAX_CAPTION-1);
    for (i=0; i < MAX_CAPTION && img->caption[i] != 0; ++i) {
        if (img->caption[i] == '\n') {
            img->caption[i] = ' ';
        }
    }
    close(capfile);
    if (gDebug)
        fprintf(stderr, "Read caption file %s\n", capfilename);
}

/* Finally, the routine that prints a summary to a file or stdout */
void PrintNotes()
{
    int i;
    char *rot90=0, *rot180=0, *rot270=0, *rot0=0, *unmatchExif=0;
    PhoImage *img;
    FILE *capfile = 0;
    int useGlobalCaptionFile = GlobalCaptionFile();

    /* Should free up memory here, e.g. for sFlagFileList,
     * but since this is only called right before exit,
     * it's never been allocated so it doesn't matter.
     * If PrintNotes ever gets called except at exit,
     * this will leak.
     */

    img = gFirstImage;
    while (img)
    {
        if (img->caption && img->caption[0] && gCapFileFormat) {
            if (gDebug)
                printf("Caption %s: %s\n", img->filename, img->caption);

            /* If we have a global captions file and we haven't
             * opened it yet, do so now.
             */
            if (useGlobalCaptionFile && gCapFileFormat && !capfile) {
                capfile = fopen(gCapFileFormat, "w");
                if (!capfile) {
                    perror(gCapFileFormat);
                    gCapFileFormat = 0;  /* don't try again to write to it */
                }
            }

            if (capfile) {    /* One shared caption file */
                /* capfile is already open, so append to it: */
                fprintf(capfile, "%s: %s\n\n", img->filename, img->caption);

            } else if (gCapFileFormat) {   /* need individual caption files */
                char* capname = CapFileName(img);
                if (capname) {
                    capfile = fopen(capname, "w");
                    if (capfile) {
                        fputs(img->caption, capfile);
                        fclose(capfile);
                        capfile = 0;
                    }
                    else
                        perror(capname);
                }
            }
	}
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
          case 0:
              /* If the image is rotated at zero but the EXIF says otherwise,
               * we need to list that too:
               */
              if (img->exifRot != 0)
                  AddImgToList(&rot0, img->filename);
              break;
          default:
              break;
        }

        /* If the user-chosen rotation doesn't match the EXIF,
         * we need to know that too.
         */
        if (img->curRot != img->exifRot)
            AddImgToList(&unmatchExif, img->filename);

        img = img->next;
        /* Have we looped back to the beginning?*/
        if (img == gFirstImage) break;
    }

    if (capfile)
        fclose(capfile);

    /* Now we've looped over all the structs, so we can print out
     * the tables of rotation and notes.
     */
    if (rot90)
        printf("\nRotate 90 (CW): %s\n", rot90);
    if (rot270)
        printf("\nRotate -90 (CCW): %s\n", rot270);
    if (rot180)
        printf("\nRotate 180: %s\n", rot180);
    if (rot0)
        printf("\nRotate 0 (wrong EXIF): %s\n", rot0);
    if (unmatchExif)
        printf("\nWrong EXIF: %s\n", unmatchExif);
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
    printf("\n");
}
