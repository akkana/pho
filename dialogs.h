/*
 * dialogs.h: dialogs/user interactions used in pho, an image viewer.
 *
 * Copyright 2004 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

/* Prompt for an answer. */
extern int Prompt(char* msg, char* yesStr, char* noStr,
                  char* yesChars, char* noChars);

/* Show or hide the info dialog. */
extern void ToggleInfo();

/* Update the info dialog, e.g. when the image changes */
void UpdateInfoDialog(PhoImage* img);
