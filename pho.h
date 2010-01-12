/*
 * gmain.c: gtk main routines for yass, an image viewer.
 *
 * Copyright 2002 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include <gdk-pixbuf/gdk-pixbuf.h>

extern GdkPixbuf* image;
extern int XSize, YSize;
extern int MonitorWidth, MonitorHeight;
extern int resized;
extern int Debug;
extern int ArgC, ArgP;
extern char** ArgV;

/* pho.c */
extern void DeleteImage();
extern void ReallyDelete();
extern int LoadImageFromFile();
extern int RotateImage(int degrees);
extern void Usage();
extern void ShowImage();

/* imagenote.c */
/* Keep the images as a doubly linked list, since many won't have any changes.
 */
struct ImgNotes_s
{
    int index;               // position in argv
    int rotation;
    int deleted;
    unsigned int noteFlags;  // flags for each numbered note made
    char* comment;           // comment added by user
    struct ImgNotes_s* next;
    struct ImgNotes_s* prev;
};

extern struct ImgNotes_s* curNote;

extern void FindImgNote(int index);
extern void SetNoteFlag(int notenum, int index);
extern void AddComment(int index, char* note);
extern char* GetComment(int index);
extern int GetRotation(int index);
extern void GetTextNote();
extern int NextImage();
extern int PrevImage();
extern void PrintNotes();
extern char* GetFlagString(int index);
extern unsigned int GetFlags(int index);
extern void SetFlags(int index, unsigned int flags);
extern int IsDeleted(int index);
extern void SetDeleted(int index, unsigned int deleted);

/* dialogs.c */
extern void ToggleInfo();
extern void ShowDeleteDialog();

#define DELETED_INDEX 0
