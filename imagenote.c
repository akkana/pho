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

static struct ImgNotes_s* NotesList = 0;
struct ImgNotes_s* curNote = 0;

void
FindImgNote(int index)
{
    // Loop over the list and insert it at the right place
    struct ImgNotes_s* thisnote = NotesList;
    struct ImgNotes_s* lastnote = 0;
    struct ImgNotes_s* newnote;

    // Shortcut: often we'll already be pointing at the right item,
    // so check for that first:
    if (curNote && curNote->index == index)
        return;

    while (thisnote && thisnote->index <= index)
    {
        if (thisnote->index == index)
        {
            curNote = thisnote;
            return;
        }

        lastnote = thisnote;
        thisnote = thisnote->next;
    }

    // We didn't match anything exactly, so we have to make a new node.
    newnote = malloc(sizeof (struct ImgNotes_s));
    if (!newnote)
    {
        curNote = 0;
        return;
    }
    newnote->index = index;
    newnote->rotation = 0;
    newnote->deleted = 0;
    newnote->noteFlags = 0;
    newnote->comment = 0;

    // Now insert it into the list
    newnote->prev = lastnote;
    if (lastnote)
        lastnote->next = newnote;
    newnote->next = thisnote;
    if (thisnote)
        thisnote->prev = newnote;
    if (!NotesList)
        NotesList = newnote;
    curNote = newnote;
    return;
}

char* GetComment(int index)
{
    FindImgNote(index);
    if (!curNote) return 0;
    return curNote->comment;
}

int GetRotation(int index)
{
    FindImgNote(index);
    if (!curNote) return 0;
    return curNote->rotation;
}

void AddComment(int index, char* note)
{
    FindImgNote(index);
    if (!curNote) return;
    curNote->comment = strdup(note);
}

int IsDeleted(int index)
{
    FindImgNote(ArgP);
    if (!curNote) return 0;
    return (curNote->deleted);
}

void SetDeleted(int index, unsigned int deleted)
{
    FindImgNote(ArgP);
    if (curNote)
        curNote->deleted = deleted;
}

void SetFlags(int index, unsigned int flags)
{
    FindImgNote(ArgP);
    if (curNote)
        curNote->noteFlags = flags;
}

unsigned int GetFlags(int index)
{
    FindImgNote(ArgP);
    if (!curNote) return 0;
    return curNote->noteFlags;
}

char* GetFlagString(int index)
{
    static char buf[35];
    static char* spaces= "                                  ";
    int i, mask;
    int foundone = 0;

    FindImgNote(ArgP);
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

void SetNoteFlag(int note, int index)
{
    int mask = (1 << note);
    if (note > 15) return;
    FindImgNote(index);
    if (!curNote) return;
    if (curNote->noteFlags & mask)
        curNote->noteFlags &= ~mask;   // clear
    else
        curNote->noteFlags |= mask;    // set
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
    char* deleted=0;
    char *rot90=0, *rot180=0, *rot270=0;
    struct ImgNotes_s* thisnote = NotesList;

    while (thisnote)
    {
        if (thisnote->comment)
            printf("%s: %s\n", ArgV[thisnote->index], thisnote->comment);
        if (thisnote->noteFlags)
        {
            int flag;
            for (i=1, flag=1; i<=10; ++i)
            {
                flag <<= 1;
                if (thisnote->noteFlags & flag)
                    AddImgToList(flags+i, ArgV[thisnote->index]);
            }
        }
        if (thisnote->deleted)
            AddImgToList(&deleted, ArgV[thisnote->index]);

        switch (thisnote->rotation)
        {
          case 90:
              AddImgToList(&rot90, ArgV[thisnote->index]);
              break;
          case 180:
              AddImgToList(&rot180, ArgV[thisnote->index]);
              break;
          case 270:
          case -90:
              AddImgToList(&rot270, ArgV[thisnote->index]);
              break;
          default:
              break;
        }
        thisnote = thisnote->next;
    }

    // Now we've looped over all the structs, so we can print out
    // the tables of rotation and notes.
    if (deleted)
        printf("Deleted: %s\n", deleted);
    if (rot90)
        printf("Rotate 90 (CW): %s\n", rot90);
    if (rot270)
        printf("Rotate -90 (CCW): %s\n", rot270);
    if (rot180)
        printf("Rotate 180: %s\n", rot180);
    for (i=0; i<10; ++i)
        if (flags[i])
            printf("Note %d: %s\n", i, flags[i]);
}

