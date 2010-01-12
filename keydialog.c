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

static GtkWidget* KeywordsDialog = 0;
static GtkWidget* KeywordsDEntry[NUM_NOTES] = {0};
static GtkWidget* KeywordsDToggle[NUM_NOTES] = {0};
static GtkWidget* KeywordsDImgName = 0;
static GtkWidget* KeywordsContainer = 0;  /* where the Entries live */
static PhoImage* sLastImage = 0;

/* Make sure we remember any changes that have been made in the dialog */
static void RememberKeywords()
{
    int i, mask, flags;

    flags = 0;
    for (i=0, mask=1; i < NUM_NOTES; ++i, mask <<= 1)
    {
        if (KeywordsDToggle[i] &&
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(KeywordsDToggle[i])))
            flags |= mask;
    }
    if (sLastImage)
        sLastImage->noteFlags = flags;
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

    KeepOnTop(KeywordsDialog);

    sprintf(buffer, "pho keywords (%s)", gCurImage->filename);
    gtk_window_set_title(GTK_WINDOW(KeywordsDialog), buffer);

    s = gCurImage->comment;
    /*gtk_entry_set_text(GTK_ENTRY(KeywordsDEntry), s ? s : "");*/

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
          HideKeywordsDialog();
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
    int i;
    GtkWidget* label;
    char buf[BUFSIZ];

    for (i=0; i<NUM_NOTES; ++i)
    {
        if (KeywordsDEntry[i] == 0)
        {
            GtkWidget* hbox = gtk_hbox_new(FALSE, 3);
            gtk_box_pack_start(GTK_BOX(KeywordsContainer), hbox,
                               TRUE, TRUE, 4);

            sprintf(buf, "%-2d", i);
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
    sprintf(buf, "That's all: sorry, only %d keywords at once", NUM_NOTES);
    label = gtk_label_new(buf);
    gtk_box_pack_start(GTK_BOX(KeywordsContainer), label, TRUE, TRUE, 4);
    gtk_widget_show(label);
}

static void MakeNewKeywordsDialog()
{
    GtkWidget *ok, *label;
    GtkWidget *dlg_vbox, *sep, *btn_box;
    int i;

    /* Use a toplevel window, so it won't pop up centered on the image win */
    KeywordsDialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    /* Unfortunately that means we have to make everything inside */
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
                       (GtkSignalFunc)HideKeywordsDialog, 0);
    gtk_widget_show(ok);

    KeywordsDImgName = gtk_label_new("imgName");
    gtk_box_pack_start(GTK_BOX(KeywordsContainer), KeywordsDImgName,
                       TRUE, TRUE, 4);
    gtk_widget_show(KeywordsDImgName);

    label = gtk_label_new("To add a new keyword, enter it in the last box and hit Enter:");
    gtk_box_pack_start(GTK_BOX(KeywordsContainer), label, TRUE, TRUE, 4);
    gtk_widget_show(label);

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
    if (! KeywordsDialog)
        MakeNewKeywordsDialog();

    else if (!IsVisible(KeywordsDialog))
        gtk_widget_show(KeywordsDialog);
    /* else it's already showing */

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
    /*gDisplayMode = PHO_DISPLAY_NORMAL;*/
    sLastImage = 0;
}

