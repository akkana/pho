/*
 * dialogs.h: dialogs/user interactions used in pho, an image viewer.
 *
 * Copyright 2004 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

/* Prompt for an answer. */
extern int Prompt(char* msg, char* yesStr, char* noStr,
                  char* yesChars, char* noChars);

/* Show or hide the Info dialog. */
extern void ToggleInfo();
extern void UpdateInfoDialog();

/* Show or hide the Keywords dialog. Show will also update it;
 * Hide will update the underlying flags.
 */
extern void InitKeywords();
extern void ShowKeywordsDialog();
extern void HideKeywordsDialog();
extern void UpdateKeywordsDialog();

/* A function dialogs must call to stay on top of the image window */
extern void KeepOnTop(GtkWidget* dialog);

/* Dialogs need to know the ID of the app's current image window,
 * so they can be transient to it.
 */
extern GtkWidget *gWin;

#define IsVisible(dlg)  (dlg && dlg->window && (GTK_WIDGET_FLAGS(dlg) & GTK_VISIBLE))

