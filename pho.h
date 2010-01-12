/*
 * gmain.c: gtk main routines for yass, an image viewer.
 *
 * Copyright 2002 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include <gdk-pixbuf/gdk-pixbuf.h>

extern GdkPixbuf* gImage;
extern int XSize, YSize;
extern int realXSize, realYSize;
extern int MonitorWidth, MonitorHeight;
extern int resized;
extern int Debug;
extern int ArgC, ArgP;
extern char** ArgV;

/* pho.c */
extern void DeleteImage();
extern void ReallyDelete();
extern int RotateImage(int degrees);
extern void Usage();
extern void ShowImage();

/* imagenote.c */
/* Keep the images as a doubly linked list, since many won't have any changes.
 */
struct ImgNotes_s
{
    int rotation;
    unsigned int noteFlags;  // flags for each numbered note made
    char* comment;           // comment added by user
};

//extern struct ImgNotes_s* NotesList;

extern void MakeNotesList(int numargs);
extern struct ImgNotes_s* FindImgNote(int index);
extern void MarkDeleted(int index);
extern int IsDeleted(int index);
extern void SetNoteFlag(int index, int notenum);
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

/* *main.c */
extern void EndSession();

/* dialogs.c */
extern void ToggleInfo();
extern int PromptDialog(char* question,
                        char* yesStr, char* noStr,     // displayed on the btns
                        char* yesChars, char* noChars);// to activate btns


