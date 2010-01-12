/*
 * pho.h: definitions for pho, an image viewer.
 *
 * Copyright 2004 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include <gdk-pixbuf/gdk-pixbuf.h>

/* Want this include to be the smallest possible include which
 * grabs GTK_MAJOR_VERSION.
 */
#include <gtk/gtk.h>

#define NUM_NOTES 10

/* Images are kept in a doubly linked list */
typedef struct PhoImage_s {
    char* filename;
    int trueWidth, trueHeight;
    int curWidth, curHeight;
    int rotation;
    int noteFlags;
    struct PhoImage_s* prev;
    struct PhoImage_s* next;
    char* comment;
} PhoImage;

extern PhoImage* NewPhoImage(char* filename);

/*************************************
 * Globals
 */

extern PhoImage* gFirstImage;
extern PhoImage* gCurImage;

/* Monitor resolution */
extern int gMonitorWidth, gMonitorHeight;

/* We only have one image at a time, so make it global. */
extern GdkPixbuf* gImage;

extern int gDebug;    /* debugging messages */

/* Scaling modes */
#define PHO_SCALE_NORMAL     0
#define PHO_SCALE_FULLSCREEN 1
#define PHO_SCALE_FULLSIZE   2
#define PHO_SCALE_ABSSIZE    3
extern int gScaleMode;

/* Some window managers don't deal well with windows that resize,
 * or don't retain focus if a resized window no longer contains
 * the mouse pointer. This allows making new windows instead.
 */
extern int gMakeNewWindows;

/* Run in "presentation mode".
 * This is different from PHO_SCALE_FULLSCREEN; fullscreen mode
 * tells the window manager to cover the whole screen with the
 * window, e.g. for presentations when you don't want your
 * desktop showing through.
 */
extern int gPresentationMode;

/*************************************
 * Forward declarations of functions
 */

extern void PrepareWindow();
extern void DeleteImage(PhoImage* img);
extern void Usage();
extern void VerboseHelp();
extern int NextImage();
extern int PrevImage();
extern void EndSession();
extern void ToggleNoteFlag(PhoImage* img, int note);
extern int RotateImage(PhoImage* img, int degrees);
extern void ScaleImage(PhoImage* img);
extern int ShowImage();
extern void PrintNotes();

