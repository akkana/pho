/*
 * imagenote.c: save info about images, for pho, an image viewer.
 *
 * Copyright 2002 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include "pho.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>       // for malloc()

struct ImgNotes_s **NotesList = 0;

static int numImages = 0;

void MakeNotesList(int num)
{
    NotesList = calloc(num, sizeof (struct ImgNotes_s *));
}

struct ImgNotes_s*
FindImgNote(int index)
{
    if (NotesList[index] != 0)
        return NotesList[index];

    // We didn't match anything exactly, so we have to make a new node.
    NotesList[index] = malloc(sizeof (struct ImgNotes_s));
    if (!NotesList[index])
        return 0;

    NotesList[index]->rotation = 0;
    NotesList[index]->noteFlags = 0;
    NotesList[index]->comment = 0;

    if (index > numImages)
        ++numImages;

    return NotesList[index];
}

void MarkDeleted(int index)
{
    ArgV[index][0] = 0;
}

int IsDeleted(int index)
{
    return (ArgV[index][0] == 0);
}

char* GetComment(int index)
{
    struct ImgNotes_s *curNote = FindImgNote(index);
    if (!curNote) return 0;
    return curNote->comment;
}

int GetRotation(int index)
{
    struct ImgNotes_s *curNote = FindImgNote(index);
    if (!curNote) return 0;
    return curNote->rotation;
}

void AddComment(int index, char* note)
{
    struct ImgNotes_s *curNote = FindImgNote(index);
    if (!curNote) return;
    curNote->comment = strdup(note);
}

void SetFlags(int index, unsigned int flags)
{
    struct ImgNotes_s *curNote = FindImgNote(index);
    if (curNote)
        curNote->noteFlags = flags;
}

unsigned int GetFlags(int index)
{
    struct ImgNotes_s *curNote = FindImgNote(index);
    if (!curNote) return 0;
    return curNote->noteFlags;
}

char* GetFlagString(int index)
{
    static char buf[35];
    static char* spaces= "                                  ";
    int i, mask;
    int foundone = 0;

    struct ImgNotes_s *curNote = FindImgNote(index);

    if (!curNote) return spaces;
    if (curNote->noteFlags == 0) return spaces;
    mask = 1;
    for (i=0; i<10; ++i, mask <<= 1)
    {
        if (curNote->noteFlags & mask)
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

void SetNoteFlag(int index, int note)
{
    int mask = (1 << note);
    struct ImgNotes_s *curNote = FindImgNote(index);

    if (note > 15) return;
    if (!curNote) return;
    if (curNote->noteFlags & mask)
        curNote->noteFlags &= ~mask;   // clear
    else
        curNote->noteFlags |= mask;    // set

    printf("Set note %d for %d to 0x%x\n", note, index, curNote->noteFlags);
}

static char *flags[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

void AddImgToList(char** strp, char* str)
{
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
}

void PrintNotes()
{
    int i;
    char *rot90=0, *rot180=0, *rot270=0;

    for (i=1; i<numImages; ++i)
    {
        if (!NotesList[i] || IsDeleted(i))
            continue;

        if (NotesList[i]->comment)
            printf("%s: %s\n", ArgV[i], NotesList[i]->comment);
        if (NotesList[i]->noteFlags)
        {
            int flag;
            int j;
            for (j=1, flag=1; j<=10; ++j)
            {
                flag <<= 1;
                if (NotesList[i]->noteFlags & flag)
                    AddImgToList(flags+j, ArgV[i]);
            }
        }

        switch (NotesList[i]->rotation)
        {
          case 90:
              AddImgToList(&rot90, ArgV[i]);
              break;
          case 180:
              AddImgToList(&rot180, ArgV[i]);
              break;
          case 270:
          case -90:
              AddImgToList(&rot270, ArgV[i]);
              break;
          default:
              break;
        }
    }

    // Now we've looped over all the structs, so we can print out
    // the tables of rotation and notes.
    if (rot90)
        printf("\nRotate 90 (CW): %s\n", rot90);
    if (rot270)
        printf("\nRotate -90 (CCW): %s\n", rot270);
    if (rot180)
        printf("\nRotate 180: %s\n", rot180);
    for (i=0; i<10; ++i)
        if (flags[i])
            printf("\nNote %d: %s\n", i, flags[i]);
}

