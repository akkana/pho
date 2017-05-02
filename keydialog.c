/*
 * keydialog.c: the pho keywords dialog.
 *
 * Copyright 2007 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include "pho.h"
#include "dialogs.h"

#include <gdk/gdkkeysyms.h>
#include <stdio.h>      /* needed on Mac, not on Linux, for sprintf */
#include <ctype.h>
#include <string.h>
#include <stdlib.h>     /* for malloc() and free() */

static GtkWidget* KeywordsDialog = 0;
static GtkWidget* KeywordsCaption = 0;
static GtkWidget* KeywordsDEntry[NUM_NOTES] = {0};
static GtkWidget* KeywordsDToggle[NUM_NOTES] = {0};
static GtkWidget* KeywordsDImgName = 0;
static GtkWidget* KeywordsContainer = 0;  /* where the Entries live */
static PhoImage* sLastImage = 0;

static void LeaveKeywordsMode()
{
    SetViewModes(PHO_DISPLAY_NORMAL, PHO_SCALE_NORMAL, 1.0);
}

void ToggleKeywordsMode()
{
    if (gDisplayMode == PHO_DISPLAY_KEYWORDS)
        LeaveKeywordsMode();
    else {
        SetViewModes(PHO_DISPLAY_KEYWORDS, PHO_SCALE_FIXED, 0.0);
        ThisImage();
    }
}

/* Make sure we remember any changes that have been made in the dialog */
void RememberKeywords()
{
    int i, mask, flags;

    if (!sLastImage)
        return;

    flags = 0;
    for (i=0, mask=1; i < NUM_NOTES; ++i, mask <<= 1)
    {
        if (KeywordsDToggle[i] &&
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(KeywordsDToggle[i])))
            flags |= mask;
    }

    sLastImage->noteFlags = flags;

    /* and save a caption, if any */
    if (sLastImage->caption)
        free(sLastImage->caption);
    sLastImage->caption = strdup((char*)gtk_entry_get_text(
                                     (GtkEntry*)KeywordsCaption));
}

/* When deleting an image, we need to clear any notion of sLastImage
 * or else we'll crash trying to access it.
 */
void NoCurrentKeywords()
{
    sLastImage = 0;
}

void SetKeywordsDialogToggle(int which, int newval)
{
    if (KeywordsDToggle[which])
        gtk_toggle_button_set_active((GtkToggleButton*)KeywordsDToggle[which],
                                     newval ? TRUE : FALSE);
}

void UpdateKeywordsDialog()
{
    char buffer[256];
    char* s;
    int i, mask, flags;

    if (!gCurImage || !KeywordsDialog || gDisplayMode != PHO_DISPLAY_KEYWORDS)
        return;
    if (!IsVisible(KeywordsDialog)) return;
    if (gCurImage != sLastImage)
        sLastImage = gCurImage;

    sprintf(buffer, "pho keywords (%s)", gCurImage->filename);
    gtk_window_set_title(GTK_WINDOW(KeywordsDialog), buffer);

    s = gCurImage->caption;
    gtk_entry_set_text(GTK_ENTRY(KeywordsCaption), s ? s : "");

    gtk_label_set_text(GTK_LABEL(KeywordsDImgName), gCurImage->filename);

    /* Update the flags fields */
    flags = gCurImage->noteFlags;
    for (i=0, mask=1; i < NUM_NOTES; ++i, mask <<= 1)
    {
        if (KeywordsDToggle[i])
            SetKeywordsDialogToggle(i, flags & mask);
    }
}

char* KeywordString(int notenum)
{
    if (! KeywordsDEntry[notenum])
        return 0;
    return (char*)gtk_entry_get_text((GtkEntry*)KeywordsDEntry[notenum]);
}

static void AddNewKeywordField();

/* When the user hits return in the last keyword field,
 * add a new one, assuming we don't already have too many.
 */
static void activate(GtkEntry *entry, int which)
{
    if (which < NUM_NOTES-1 && KeywordsDEntry[which+1] == 0)
        AddNewKeywordField();
}

static gint handleKeywordsKeyPress(GtkWidget* widget, GdkEventKey* event)
{
    /* We only handle a few key events that aren't shifted: */
    switch (event->keyval)
    {
      case GDK_Escape:
          LeaveKeywordsMode();
          return TRUE;
    }

    /* But we handle certain other events if a modifier key is down */
    if (! (event->state & GDK_MODIFIER_MASK))
        return FALSE;

    /* But don't just pass all modifier events -- many of them are
     * meaningful when editing a keyword in a text field!
     */
    /* Shifted printable keys should probably stay here */
    if (event->state & GDK_SHIFT_MASK && (event->length > 0)
        && isprint(event->string[0]))
        return FALSE;
    /* Emacs/readline editing keys are also useful */
    if (event->state & GDK_CONTROL_MASK) {
        switch (event->keyval)
        {
        case GDK_a:
        case GDK_e:
        case GDK_u:
        case GDK_h:
        case GDK_w:
        case GDK_k:
        case GDK_d:
            return FALSE;
        }
    }

    /* Otherwise, it's probably a pho key binding, so pass it on: */
    return HandleGlobalKeys(widget, event);
}

/* Add a new keyword field to the dialog */
static void AddNewKeywordField()
{
    long i;
    GtkWidget* label;
    char buf[BUFSIZ];

    for (i=0; i<NUM_NOTES; ++i)
    {
        if (KeywordsDEntry[i] == 0)
        {
            GtkWidget* hbox = gtk_hbox_new(FALSE, 3);
            gtk_box_pack_start(GTK_BOX(KeywordsContainer), hbox,
                               TRUE, TRUE, 4);

            sprintf(buf, "%-2ld", i);
            KeywordsDToggle[i] = gtk_toggle_button_new_with_label(buf);
            gtk_box_pack_start(GTK_BOX(hbox), KeywordsDToggle[i],
                               FALSE, FALSE, 4);
            gtk_toggle_button_set_active((GtkToggleButton*)KeywordsDToggle[i],
                                         TRUE);
            gtk_widget_show(KeywordsDToggle[i]);

            KeywordsDEntry[i] = gtk_entry_new();
            gtk_box_pack_start(GTK_BOX(hbox), KeywordsDEntry[i],
                               TRUE, TRUE, 4);
            gtk_signal_connect(GTK_OBJECT(KeywordsDEntry[i]), "activate",
                               (GtkSignalFunc)activate, (gpointer)i);
            gtk_widget_show(KeywordsDEntry[i]);
            gtk_widget_show(hbox);

            gtk_widget_grab_focus(KeywordsDEntry[i]);
            return;
        }
    }

    /* If we got here, we've overflowed */
    sprintf(buf, "That's all: sorry, only %ld keywords at once", NUM_NOTES);
    label = gtk_label_new(buf);
    gtk_box_pack_start(GTK_BOX(KeywordsContainer), label, TRUE, TRUE, 4);
    gtk_widget_show(label);
}

static void MakeNewKeywordsDialog()
{
    GtkWidget *ok, *label;
    GtkWidget *dlg_vbox, *sep, *btn_box, *hbox;
    int i;

    /* Use a toplevel window, so it won't pop up centered on the image win */
    KeywordsDialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    /* Unfortunately that means we have to make everything inside. */

    /* Tried making it a utility win for focus issues, but it didn't help:
    gtk_window_set_type_hint (GTK_WINDOW(KeywordsDialog),
                              GDK_WINDOW_TYPE_HINT_UTILITY);
     */

    dlg_vbox = gtk_vbox_new(FALSE, 3);
    gtk_container_add(GTK_CONTAINER(KeywordsDialog), dlg_vbox);
    gtk_widget_show(dlg_vbox);

    gtk_signal_connect(GTK_OBJECT(KeywordsDialog), "key_press_event",
                       (GtkSignalFunc)handleKeywordsKeyPress, 0);

    KeywordsContainer = gtk_vbox_new(FALSE, 3);
    gtk_container_add(GTK_CONTAINER(dlg_vbox), KeywordsContainer);
    gtk_widget_show(KeywordsContainer);
    sep = gtk_hseparator_new();
    gtk_container_add(GTK_CONTAINER(dlg_vbox), sep);
    gtk_widget_show(sep);
    btn_box = gtk_hbox_new(FALSE, 3);
    gtk_container_add(GTK_CONTAINER(dlg_vbox), btn_box);
    gtk_container_set_border_width(GTK_CONTAINER(btn_box), 20);
    gtk_widget_show(btn_box);

    gtk_container_set_border_width(GTK_CONTAINER(KeywordsContainer), 8);

    /* Make the button */
    ok = gtk_button_new_with_label("Leave Keywords Mode");
    gtk_box_pack_start(GTK_BOX(btn_box), ok, TRUE, TRUE, 0);

    gtk_signal_connect(GTK_OBJECT(ok), "clicked",
                       (GtkSignalFunc)LeaveKeywordsMode, 0);
    gtk_widget_show(ok);

    KeywordsDImgName = gtk_label_new("imgName");
    gtk_box_pack_start(GTK_BOX(KeywordsContainer), KeywordsDImgName,
                       TRUE, TRUE, 4);
    gtk_widget_show(KeywordsDImgName);

    label = gtk_label_new("To add a new keyword, hit Enter in the last box:");
    gtk_box_pack_start(GTK_BOX(KeywordsContainer), label, TRUE, TRUE, 4);
    gtk_widget_show(label);

    /* Add the caption field */
    hbox = gtk_hbox_new(FALSE, 3);
    gtk_box_pack_start(GTK_BOX(KeywordsContainer), hbox,
                       TRUE, TRUE, 4);
    label = gtk_label_new("Caption:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
    gtk_widget_show(label);

    KeywordsCaption = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), KeywordsCaption, TRUE, TRUE, 4);
    gtk_widget_show(KeywordsCaption);

    gtk_widget_show(hbox);

    /* Make sure all the entries are initialized */
    for (i=0; i < NUM_NOTES; ++i)
        KeywordsDEntry[i] = 0;

    /* Add the first keywords field. Others will be added as needed */
    AddNewKeywordField();

    gtk_widget_show(KeywordsDialog);
}

/* Make sure the dialog is showing */
void ShowKeywordsDialog()
{
    if (!gWin)
        return;

    if (!KeywordsDialog)
        MakeNewKeywordsDialog();

    else if (!IsVisible(KeywordsDialog))
        gtk_widget_show(KeywordsDialog);
    /* else it's already showing */

    /* Calling this from UpdateKeywordsDialog somehow sends focus
     * back to the image window.
    KeepOnTop(KeywordsDialog);
     */
    /*
    gdk_window_raise(GTK_WIDGET(KeywordsDialog)->window);
     * XXX Why does raise raise forever? Isn't there some way
     * to raise once but still let the user control the stacking??
     */

    /* Save any state we have from the previous image */
    RememberKeywords();

    /* update to show the state of the new image */
    UpdateKeywordsDialog();
}

void HideKeywordsDialog()
{
    RememberKeywords();
    if (IsVisible(KeywordsDialog))
        gtk_widget_hide(KeywordsDialog);
    sLastImage = 0;
}

