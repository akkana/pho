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

/* Images are kept in a doubly linked list */
typedef struct PhoImage_s {
    char* filename;
    int trueWidth, trueHeight;
    int curWidth, curHeight;
    int curRot;       /* rotation of the current image bits */
    int desiredRot;   /* exif or user-specified rotation */
    unsigned long noteFlags;
    unsigned int deleted;
    struct PhoImage_s* prev;
    struct PhoImage_s* next;
    char* comment;
} PhoImage;

/* Number of bits in unsigned long noteFlags */
#define NUM_NOTES ((sizeof (unsigned long) * 8) - 1)

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
/* Normal: show at full size if it fits on screen, otherwise size to screen. */
#define PHO_SCALE_NORMAL       0
/* Full Screen: make it as big as necessary to fill the screen,
 * even if that means scaling up.
 */
#define PHO_SCALE_FULLSCREEN   1
/* Full Size: show at full size (1:1 pixel) even if it's bigger
 * than the screen.
 */
#define PHO_SCALE_FULLSIZE     2
 /* IMG_RATIO: pho will scale the image to an absolute multiple of
  * the true image size.
  */
#define PHO_SCALE_IMG_RATIO    4
 /* SCREEN_RATIO: pho will show the image no larger than the screen
  * size times gScaleRatio (i.e. when ratio==1.0, same as normal mode).
  */
#define PHO_SCALE_SCREEN_RATIO 5
extern int gScaleMode;
extern double gScaleRatio;     /* only used for PHO_SCALE_*_RATIO */

/* Display modes */
#define PHO_DISPLAY_NORMAL       0
#define PHO_DISPLAY_PRESENTATION 1
#define PHO_DISPLAY_KEYWORDS     2
extern int gDisplayMode;

/* Some window managers don't deal well with windows that resize,
 * or don't retain focus if a resized window no longer contains
 * the mouse pointer. This allows making new windows instead.
 */
extern int gMakeNewWindows;

/* Seconds delay between images in slideshow mode.
 * Normally 0, no slideshow.
 */
extern int gDelaySeconds;

/* Get the keyword string associated with a note number */
extern char* KeywordString(int notenum);

/* Set the display mode, which may involve some modifications to
 * window size or type
 */
extern void SetDisplayMode(int newmode);

/* Update toggles for the flags */
extern void SetInfoDialogToggle(int which, int newval);
extern void SetKeywordsDialogToggle(int which, int newval);

/* Other routines that need to be public */
extern void PrepareWindow();
extern void DeleteImage(PhoImage* img);
extern void Usage();
extern void VerboseHelp();
extern int NextImage();
extern int PrevImage();
extern int ThisImage();
extern void EndSession();
extern void ToggleNoteFlag(PhoImage* img, int note);
extern void ScaleAndRotate(PhoImage* img, int degrees);
extern int ShowImage();
extern void InitNotes();
extern void PrintNotes();
extern void ClearImageList();

/* event handler. Ugh, this introduces gtk stuff */
extern gint HandleGlobalKeys();
