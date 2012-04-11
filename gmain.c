/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * gmain.c: gtk main routines for pho, an image viewer.
 *
 * Copyright 2004 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include "pho.h"
#include "dialogs.h"
#include "exif/phoexif.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include <stdlib.h>       /* for getenv() */
#include <unistd.h>
#include <string.h>
#include <ctype.h>

char * gCapFileFormat = "Captions";

/* Toggle a variable between two modes, preferring the first.
 * If it's anything but mode1 it will end up as mode1.
 */
#define ToggleBetween(val,mode1,mode2) (val != mode1 ? mode1 : mode2)

/* If we're switching into scaling mode because the user pressed - or +/=,
 * which scaling mode should we switch into?
 */
static int ModeForScaling(int oldmode)
{
    switch (oldmode)
    {
      case PHO_SCALE_IMG_RATIO:
      case PHO_SCALE_FULLSIZE:
        return PHO_SCALE_IMG_RATIO;

      case PHO_SCALE_FIXED:
          return PHO_SCALE_FIXED;

      case PHO_SCALE_NORMAL:
      case PHO_SCALE_FULLSCREEN:
      default:
        return PHO_SCALE_SCREEN_RATIO;
    }
}

static void RunPhoCommand()
{
    char* pos;
    char* cmd = getenv("PHO_CMD");
    if (cmd == 0) cmd = "gimp";
    else if (gDebug)
        printf("Calling PHO_CMD %s\n", cmd);

    /* PHO_CMD command can have a %s in it but doesn't need to.
     * If it does, we'll use sprintf and system(),
     * otherwise we'll properly use vfork() and execlp().
     */
    if ((pos = strchr(cmd, '%')) == 0 || pos[1] != 's') {
        if (fork() == 0) {      /* child process */
            if (gDebug)
                printf("Child: about to exec %s, %s\n",
                       cmd, gCurImage->filename);
            execlp(cmd, cmd, gCurImage->filename, (char*)0);
        }
    }
    else {
        /* If there's a %s in the filename, then we'll pass
         * the command to sh -c. That means we should put
         * single quotes around the filename to guard against
         * evil filenames like "`rm -rf ~`".
         * But that also means we have to escape any single
         * quotes in that filename.
         */
        int len;
        char *buf, *bufp, *cmdp;
        int numquotes = 0;
        int did_subst = 0;

        /* Count any single quotes in the filename */
        for (len = 0, buf = gCurImage->filename; buf[len]; ++len)
            if (buf[len] == '\'')
                ++numquotes;
        /* len is now the # chars in filename, not including null */

        buf = malloc(strlen(cmd) + len + numquotes + 1);
        /* -2 because we lose the %s, +2 for the two quotes we add,
         * and another for each \ we need to add to escape a quote.
         */

        /* copy cmd into buf, substituting the filename for %s */
        for (bufp = buf, cmdp = cmd; *cmdp; )
        {
            if (!did_subst && cmdp[0] == '%' && cmdp[1] == 's') {
                *(bufp++) = '\'';
                strncpy(bufp, gCurImage->filename, len);
                bufp += len;
                *(bufp++) = '\'';
                cmdp += 2;
                did_subst = 1;
            }
            else if (cmd[0] == '\'') {
                *(bufp++) = '\\';
                *(bufp++) = '\'';
            }
            else
                *(bufp++) = *(cmdp++);
        }
        *bufp = 0;
        if (fork() == 0) {      /* child process */
            if (gDebug)
                printf("Child: about to exec sh -c \"%s\"\n", buf);
            execl("/bin/sh", "sh", "-c", buf, (char*)0);
        }
    }
}

void TryScale(float times)
{
    /* Save the view modes, in case this fails */
    int saveScaleMode = gScaleMode;
    double saveScaleRatio = gScaleRatio;
    int saveDisplayMode = gDisplayMode;

    if (SetViewModes(gDisplayMode, ModeForScaling(gScaleMode),
                     gScaleRatio * times) == 0)
        return;

    /* Oops! It didn't work. Try to reset back to previous settings */
    SetViewModes(saveDisplayMode, saveScaleMode, saveScaleRatio);
}

gint HandleGlobalKeys(GtkWidget* widget, GdkEventKey* event)
{
    if (gDebug) printf("\nKey event\n");
    switch (event->keyval)
    {
      case GDK_d:
          DeleteImage(gCurImage);
          break;
      case GDK_space:
          /* If we're in slideshow mode, cancel the slideshow */
          if (gDelaySeconds > 0) {
              gDelaySeconds = 0;
          }
          else if (NextImage() != 0) {
              if (Prompt("Quit pho?", "Quit", "Continue", "qx \n", "cn") != 0)
                  EndSession();
          }
          return TRUE;
      case GDK_BackSpace:
          PrevImage();
          return TRUE;
      case GDK_Home:
          gCurImage = 0;
          NextImage();
          return TRUE;
      case GDK_End:
          gCurImage = gFirstImage->prev;
          ThisImage();
          return TRUE;
      case GDK_n:   /* Get out of any weird display modes */
          SetViewModes(PHO_DISPLAY_NORMAL, PHO_SCALE_NORMAL, 1.);
          ShowImage();
          return TRUE;
      case GDK_f:   /* Full size mode: show image bit-for-bit */
          /* Don't respond to ctrl-F -- that might be an attempt
           * to edit in a text field in the keywords dialog
           */
          if (event->state & GDK_CONTROL_MASK)
              return FALSE;
          SetViewModes(gDisplayMode,
                       ToggleBetween(gScaleMode,
                                     PHO_SCALE_FULLSIZE, PHO_SCALE_NORMAL),
                       1.);
          return TRUE;
      case GDK_F:   /* Full screen mode: as big as possible on screen */
          SetViewModes(gDisplayMode,
                       ToggleBetween(gScaleMode,
                                     PHO_SCALE_FULLSCREEN, PHO_SCALE_NORMAL),
                       1.);
          return TRUE;
      case GDK_p:
          SetViewModes((gDisplayMode == PHO_DISPLAY_PRESENTATION)
                       ? PHO_DISPLAY_NORMAL
                       : PHO_DISPLAY_PRESENTATION,
                       gScaleMode, gScaleRatio);
          return TRUE;
      case GDK_0:
      case GDK_1:
      case GDK_2:
      case GDK_3:
      case GDK_4:
      case GDK_5:
      case GDK_6:
      case GDK_7:
      case GDK_8:
      case GDK_9:
          if (event->state & GDK_MOD1_MASK) /* alt-num: add 10 to num */
              ToggleNoteFlag(gCurImage, event->keyval - GDK_0 + 10);
          else
              ToggleNoteFlag(gCurImage, event->keyval - GDK_0);
          return TRUE;
      case GDK_t:   /* make life easier for xv switchers */
      case GDK_r:
      case GDK_Right:
      case GDK_KP_Right:
          ScaleAndRotate(gCurImage, 90);
          return TRUE;
      case GDK_T:   /* make life easier for xv users */
      case GDK_R:
      case GDK_l:
      case GDK_L:
      case GDK_Left:
      case GDK_KP_Left:
          ScaleAndRotate(gCurImage, 270);
          return TRUE;
      case GDK_Up:
      case GDK_Down:
          ScaleAndRotate(gCurImage, 180);
          return TRUE;
      case GDK_plus:
      case GDK_KP_Add:
      case GDK_equal:
          TryScale(2.);
          return TRUE;
      case GDK_minus:
      case GDK_slash:
      case GDK_KP_Subtract:
          TryScale(.5);
          return TRUE;
      case GDK_g:  /* start gimp, or some other app */
          RunPhoCommand();
          break;
      case GDK_i:
          ToggleInfo();
          return TRUE;
      case GDK_k:
          ToggleKeywordsMode();
          return TRUE;
      case GDK_o:
          ChangeWorkingFileSet();
          return TRUE;
      case GDK_Escape:
      case GDK_q:
          EndSession();
          return TRUE;
      default:
          if (gDebug)
              printf("Don't know key 0x%lu\n", (unsigned long)(event->keyval));
          return FALSE;
    }
    /* Keep gcc 2.95 happy: */
    return FALSE;
}

PhoImage* AddImage(char* filename)
{
    PhoImage* img = NewPhoImage(filename);
    if (gDebug)
        printf("Adding image %s\n", filename);
    if (!img) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }
    /* Make img the new last image in the list */
    AppendItem(img);
    return img;
}

/* CheckArg takes a string, like -Pvg, and sets all the relevant flags. */
static void CheckArg(char* arg)
{
    for ( ; *arg != 0; ++arg)
    {
        if (*arg == '-')
            ;
        else if (*arg == 'd')
            gDebug = 1;
        else if (*arg == 'h')
            Usage();
        else if (*arg == 'v')
            VerboseHelp();
        else if (*arg == 'n')
            gMakeNewWindows = 1;
        else if (*arg == 'p')
            gDisplayMode = PHO_DISPLAY_PRESENTATION;
        else if (*arg == 'P')
            gDisplayMode = PHO_DISPLAY_NORMAL;
        else if (*arg == 'k') {
            gDisplayMode = PHO_DISPLAY_KEYWORDS;
            gScaleMode = PHO_SCALE_FIXED;
            gScaleRatio = 0.0;
        } else if (*arg == 's') {
            /* find the slideshow delay time, from e.g. pho -s2 */
            if (isdigit(arg[1]))
                gDelaySeconds = atoi(arg+1);
            else Usage();
            if (gDebug)
                printf("Slideshow delay %d seconds\n", gDelaySeconds);
        } else if (*arg == 'c') {
            gCapFileFormat = strdup(arg+1);
            if (gDebug)
                printf("Format set to '%s'\n", gCapFileFormat);

            /* Can't follow this with any other letters -- they're all
             * part of the filename -- so return.
             */
            return;
	    }
    }
}

int main(int argc, char** argv)
{
    /* Initialize some defaults from environment variables,
     * before reading cmdline args.
     */
    char* env = getenv("PHO_ARGS");
    if (env && *env)
        CheckArg(env);

    while (argc > 1)
    {
        if (argv[1][0] == '-')
            CheckArg(argv[1]);
        else {
            AddImage(argv[1]);
        }
        --argc;
        ++argv;
    }

    if (gFirstImage == 0)
        Usage();

    /* Initialize some variables associated with the notes flags */
    InitNotes();

    /* See http://www.gtk.org/tutorial */
    gtk_init(&argc, &argv);

    /* Must init rgb system explicitly, else we'll crash
     * in the first gdk_pixbuf_render_to_drawable(),
     * calling gdk_draw_rgb_image_dithalign():
     * (Is this still true in gtk2?)
     */
    gdk_rgb_init();

    gPhysMonitorWidth = gMonitorWidth = gdk_screen_width();
    gPhysMonitorHeight = gMonitorHeight = gdk_screen_height();

    /* Load the first image */
    if (NextImage() != 0)
        exit(1);

    gtk_main();
    return 0;
}

