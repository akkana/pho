/*
 * gdialogs.c: gtk dialogs used in pho, an image viewer.
 *
 * Copyright 2004 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include "pho.h"
#include "dialogs.h"
#include "exif/phoexif.h"

#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>    /* for free() */
#include <unistd.h>

static GtkWidget* InfoDialog = 0;
static GtkWidget* InfoDEntry = 0;
static GtkWidget* InfoDImgName = 0;
static GtkWidget* InfoDImgSize = 0;
static GtkWidget* InfoDOrigSize = 0;
static GtkWidget* InfoDImgRotation = 0;
static GtkWidget* InfoFlag[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static GtkWidget* InfoExifContainer;
static GtkWidget* InfoExifEntries[NUM_EXIF_FIELDS];
static PhoImage* sCurInfoImage = 0;

/* It turns out that trying to have two dialogs both transient
 * to the same main window causes bad things to happen.
 * So guard against that.
 */
void KeepOnTop(GtkWidget* dialog)
{
    static GtkWidget* sCurTransientDlg = 0;
    static GtkWidget* sCurTransientOwner = 0;

    if (sCurTransientDlg != dialog && sCurTransientOwner != gWin)
    {
        gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gWin));
        sCurTransientDlg = dialog;
        sCurTransientOwner = gWin;
    }
}

static void AddComment(PhoImage* img, char* txt)
{
    if (img->comment != 0)
        free(img->comment);
    img->comment = strdup(txt);
}

/* Update the image according to whatever has changed in the dialog.
 * Call this before popping down or quitting,
 * and before going to next or prev image.
 */
static void UpdateImage()
{
    int i;
    unsigned mask, flags;
    char* text;

    if (!InfoDialog || !InfoDialog->window || !IsVisible(InfoDialog)
        || !sCurInfoImage)
        return;

    text = (char*)gtk_entry_get_text((GtkEntry*)InfoDEntry);
    if (text && *text)
        AddComment(sCurInfoImage, text);
            
    flags = 0;
    for (i=0, mask=1; i<10; ++i, mask <<= 1)
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(InfoFlag[i])))
            flags |= mask;
    }
    sCurInfoImage->noteFlags = flags;
}

static void PopdownInfoDialog()
{
    UpdateImage();
    if (IsVisible(InfoDialog))
        gtk_widget_hide(InfoDialog);
}

void SetInfoDialogToggle(int which, int newval)
{
    if (InfoFlag[which])
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(InfoFlag[which]),
                                     newval ? TRUE : FALSE);
}

void UpdateInfoDialog()
{
    char buffer[256];
    char* s;
    int i, mask, flags;

    if (!gCurImage || !InfoDialog || !InfoDialog->window)
        /* Don't need to check whether it's visible -- if we're not
         * about to show the dialog we shouldn't be calling this anyway.
         */
        return;
    if (gDebug)
        printf("UpdateInfoDialog() for %s\n", gCurImage->filename);

    /* Update the last image according to the current dialog */
    if (gCurImage == sCurInfoImage) {
        if (gDebug) printf("Already up-to-date\n");
        return;
    }
    if (sCurInfoImage) {
        if (gDebug) printf("Calling UpdateImage\n");
        UpdateImage();
    }

    if (gCurImage == 0)
        return;

    /* Now update the dialog for the new image */
    sCurInfoImage = gCurImage;

    /* In case the image's window has changed, make sure we're
     * staying on top of the right window:
     */
    KeepOnTop(InfoDialog);

    sprintf(buffer, "pho: %s info", gCurImage->filename);
    gtk_window_set_title(GTK_WINDOW(InfoDialog), buffer);

    s = gCurImage->comment;
    gtk_entry_set_text(GTK_ENTRY(InfoDEntry), s ? s : "");

    gtk_label_set_text(GTK_LABEL(InfoDImgName), gCurImage->filename);
    sprintf(buffer, "%d x %d", gCurImage->trueWidth, gCurImage->trueHeight);
    gtk_label_set_text(GTK_LABEL(InfoDOrigSize), buffer);
    sprintf(buffer, "%d x %d", gCurImage->curWidth, gCurImage->curHeight);
    gtk_label_set_text(GTK_LABEL(InfoDImgSize), buffer);
    switch (gCurImage->curRot)
    {
      case 90:
          gtk_label_set_text(GTK_LABEL(InfoDImgRotation), " 90 ");
          break;
      case 180:
          gtk_label_set_text(GTK_LABEL(InfoDImgRotation), "180");
          break;
      case -90:
      case 270:
          gtk_label_set_text(GTK_LABEL(InfoDImgRotation), "-90");
          break;
      default:
          gtk_label_set_text(GTK_LABEL(InfoDImgRotation), "  0");
          break;
    }

    /* Update the flags buttons */
    flags = gCurImage->noteFlags;
    for (i=0, mask=1; i<10; ++i, mask <<= 1)
        SetInfoDialogToggle(i, (flags & mask) != 0);

    /* Loop over the various EXIF elements.
     * Expect we already called ExifReadInfo, back in LoadImageFromFile.
     */
    if (HasExif(gCurImage))
        gtk_widget_set_sensitive(InfoExifContainer, TRUE);
    else
        gtk_widget_set_sensitive(InfoExifContainer, FALSE);
    for (i=0; i<NUM_EXIF_FIELDS; ++i)
    {
        if (HasExif()) {
            gtk_entry_set_text(GTK_ENTRY(InfoExifEntries[i]),
                               ExifGetString(i));
            gtk_entry_set_editable(GTK_ENTRY(InfoExifEntries[i]), FALSE);
        }
        else {
            gtk_entry_set_text(GTK_ENTRY(InfoExifEntries[i]), " ");
            gtk_entry_set_editable(GTK_ENTRY(InfoExifEntries[i]), FALSE);
        }
    }
}

static gint InfoDialogExpose(GtkWidget* widget, GdkEventKey* event)
{
    gtk_signal_handler_block_by_func(GTK_OBJECT(InfoDialog),
                                     (GtkSignalFunc)InfoDialogExpose, 0);
    gtk_widget_grab_focus(InfoDEntry);
    if (gDebug)
        printf("InfoDialogExpose\n");
    UpdateInfoDialog(gCurImage);
    /*
    gtk_signal_handler_unblock_by_func(GTK_OBJECT(InfoDialog),
                                       (GtkSignalFunc)InfoDialogExpose, 0);
     */
    /* Return FALSE so that the regular dialog expose handler will
     * draw the dialog properly the first time.
     */
    return FALSE;
}

static gint HandleInfoKeyPress(GtkWidget* widget, GdkEventKey* event)
{
    switch (event->keyval)
    {
      case GDK_Escape:
      case GDK_Return:
      case GDK_KP_Enter:
          PopdownInfoDialog();
          return TRUE;
    }

    /* handle other events iff Shift is pressed */
    if (! event->state & GDK_SHIFT_MASK)
        return FALSE;

    return HandleGlobalKeys(widget, event);
}

/* Show a dialog with info about the current image.
 */
void ToggleInfo()
{
    GtkWidget *ok;
    GtkWidget *label, *vbox, *box, *scroller;
    int i;

    if (gDebug) printf("ToggleInfo\n");

    if (InfoDialog && InfoDialog->window)
    {
        if (GTK_WIDGET_FLAGS(InfoDialog) & GTK_VISIBLE)
            gtk_widget_hide(InfoDialog);
        else {
            UpdateInfoDialog(gCurImage);
            gtk_widget_show(InfoDialog);
        }
        return;
    }

    /* Else it's the first time, and we need to create the dialog */

    InfoDialog = gtk_dialog_new();
    gtk_signal_connect(GTK_OBJECT(InfoDialog), "key_press_event",
                       (GtkSignalFunc)HandleInfoKeyPress, 0);

#ifdef SCROLLER
    /* With the scroller, the dialog comes up tiny.
     * Until I solve this, turn it off by default.
     */
    scroller = gtk_scrolled_window_new(0, 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_box_pack_start(GTK_BOX (GTK_DIALOG(InfoDialog)->vbox), scroller,
                       TRUE, TRUE, 0);
    gtk_widget_show (scroller);

    vbox = gtk_vbox_new(FALSE, 3);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW (scroller),
                                          vbox);
    gtk_widget_show(vbox);
#else /* SCROLLER */
    scroller = 0;  /* warning fix */
    vbox = GTK_DIALOG(InfoDialog)->vbox;
#endif /* SCROLLER */

    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

    /* Make the button */
    ok = gtk_button_new_with_label("Ok");
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(InfoDialog)->action_area),
                       ok, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(ok), "clicked",
                       (GtkSignalFunc)PopdownInfoDialog, 0);
    gtk_widget_show(ok);

    /* Add the info items */
    InfoDImgName = gtk_label_new("imgName");
    gtk_box_pack_start(GTK_BOX(vbox), InfoDImgName, TRUE, TRUE, 4);
    gtk_widget_show(InfoDImgName);

    box = gtk_table_new(3, 2, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(box), 7);
    label = gtk_label_new("Displayed Size:");
    gtk_misc_set_alignment(GTK_MISC(label), 1., .5);
    gtk_table_attach_defaults(GTK_TABLE(box), label, 0, 1, 0, 1);
    gtk_widget_show(label);
    InfoDImgSize = gtk_label_new("0x0");
    gtk_misc_set_alignment(GTK_MISC(InfoDImgSize), 0., .5);
    gtk_table_attach_defaults(GTK_TABLE(box), InfoDImgSize, 1, 2, 0, 1);
    gtk_widget_show(InfoDImgSize);
    label = gtk_label_new("Actual Size:");
    gtk_misc_set_alignment(GTK_MISC(label), 1., .5);
    gtk_table_attach_defaults(GTK_TABLE(box), label, 0, 1, 1, 2);
    gtk_widget_show(label);
    InfoDOrigSize = gtk_label_new("0x0");
    gtk_misc_set_alignment(GTK_MISC(InfoDOrigSize), 0., .5);
    gtk_table_attach_defaults(GTK_TABLE(box), InfoDOrigSize, 1, 2, 1, 2);
    gtk_widget_show(InfoDOrigSize);

    label = gtk_label_new("Rotation:");
    gtk_misc_set_alignment(GTK_MISC(label), 1., .5);
    gtk_table_attach_defaults(GTK_TABLE(box), label, 0, 1, 2, 3);
    gtk_widget_show(label);
    InfoDImgRotation = gtk_label_new("imgRot");
    gtk_misc_set_alignment(GTK_MISC(InfoDImgRotation), 0., .5);
    gtk_table_attach_defaults(GTK_TABLE(box), InfoDImgRotation, 1, 2, 2, 3);
    gtk_widget_show(InfoDImgRotation);
    gtk_box_pack_start(GTK_BOX(vbox), box, TRUE, TRUE, 0);
    gtk_widget_show(box);

    /* Make the line of Notes buttons */
    box = gtk_table_new(2, 11, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(box), 2);
    gtk_table_set_col_spacings(GTK_TABLE(box), 7);
    label = gtk_label_new("Notes:");
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
    gtk_table_attach_defaults(GTK_TABLE(box), label, 0, 1, 0, 1);
    gtk_widget_show(label);
    for (i=0; i<10; ++i)
    {
        char str[2] = { '\0', '\0' };
        str[0] = i + '0';
        InfoFlag[i] = gtk_toggle_button_new_with_label(str);
        gtk_table_attach_defaults(GTK_TABLE(box), InfoFlag[i], i+1, i+2, 0, 1);
        gtk_widget_show(InfoFlag[i]);
    }

    label = gtk_label_new("Comment:");
    gtk_table_attach_defaults(GTK_TABLE(box), label, 0, 1, 1, 2);
    gtk_widget_show(label);
    InfoDEntry = gtk_entry_new();
    gtk_table_attach_defaults(GTK_TABLE(box), InfoDEntry, 1, 11, 1, 2);
    gtk_widget_show(InfoDEntry);
    gtk_box_pack_start(GTK_BOX(vbox), box, TRUE, TRUE, 0);
    gtk_widget_show(box);

    label = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 8);
    gtk_widget_show(label);

    label = gtk_label_new("Exif:");
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
    gtk_widget_show(label);

    InfoExifContainer = gtk_table_new(NUM_EXIF_FIELDS, 2, FALSE);
    /* set_padding doesn't work on tables, apparently */
    /*gtk_misc_set_padding(GTK_MISC(InfoExifContainer), 7, 7);*/
    gtk_table_set_row_spacings(GTK_TABLE(InfoExifContainer), 2);
    gtk_table_set_col_spacings(GTK_TABLE(InfoExifContainer), 5);
    gtk_box_pack_start(GTK_BOX(vbox), InfoExifContainer, TRUE, TRUE, 0);

    /* Loop over the various EXIF elements */
    for (i=0; i<NUM_EXIF_FIELDS; ++i)
    {
        label = gtk_label_new(ExifLabels[i].str);
        gtk_misc_set_alignment(GTK_MISC(label), 1., .5);
        gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(InfoExifContainer), label,
                                  0, 1, i, i+1);
        InfoExifEntries[i] = gtk_entry_new();
        gtk_table_attach_defaults(GTK_TABLE(InfoExifContainer),
                                  InfoExifEntries[i],
                                  1, 2, i, i+1);
        gtk_widget_show(InfoExifEntries[i]);
    }
    gtk_widget_show(InfoExifContainer);

    gtk_signal_connect(GTK_OBJECT(InfoDialog), "expose_event",
                       (GtkSignalFunc)InfoDialogExpose, 0);

    gtk_widget_show(InfoDialog);
    /* Don't call UpdateInfoDialog: it won't actually update
     * everything it needs until after the first expose.
     */
    UpdateInfoDialog(gCurImage);
}

/*
 * A generic Prompt dialog.
 */

static GtkWidget* promptDialog = 0;

static char* defaultYesChars = "yY\n";
static char* defaultNoChars = "nN";   /* ESC always works to cancel */
static char* gYesChars = 0;
static char* gNoChars = 0;

static gint
HandlePromptKeyPress(GtkWidget* widget, GdkEventKey* event)
{
    char c;

    if (event->keyval == GDK_Escape)
    {
        gtk_dialog_response(GTK_DIALOG(promptDialog), 0);
        return TRUE;
    }

    if (event->keyval == GDK_space)
        c = ' ';

    else if (event->keyval >= GDK_A && event->keyval <= GDK_Z)
        c = event->keyval - GDK_A + 'A';

    else if (event->keyval >= GDK_a && event->keyval <= GDK_z)
        c = event->keyval - GDK_a + 'a';

    else if (event->keyval >= GDK_0 && event->keyval <= GDK_9)
        c = event->keyval - GDK_0 + '0';

    else {
        gdk_beep();
        return FALSE;
    }

    /* Now we have a c: see if it's in the yes or no char lists */
    if (strchr(gYesChars, c))
    {
        gtk_dialog_response(GTK_DIALOG(promptDialog), 1);
        return TRUE;
    }
    else if (strchr(gNoChars, c))
    {
        gtk_dialog_response(GTK_DIALOG(promptDialog), 0);
        return TRUE;
    }

    gdk_beep();
    return FALSE;
}

int Prompt(char* msg, char* yesStr, char* noStr, char* yesChars, char* noChars)
{
    static GtkWidget* question = 0;
    static GtkWidget* yesBtn = 0;
    static GtkWidget* noBtn = 0;
    int qYesNo;

    if (!yesStr)
        yesStr = "Yes";
    if (!noStr)
        noStr = "Cancel";

    gYesChars = yesChars ? yesChars : defaultYesChars;
    gNoChars = noChars ? noChars : defaultNoChars;

    if (promptDialog && question && yesBtn && noBtn)
    {
        gtk_label_set_text(GTK_LABEL(question), msg);
        gtk_label_set_text(GTK_LABEL(GTK_BIN(yesBtn)->child), yesStr);
        gtk_label_set_text(GTK_LABEL(GTK_BIN(noBtn)->child), noStr);
    }
    else
    {
        /* First time through: make the dialog */
        promptDialog = gtk_dialog_new_with_buttons("Question",
                                                   GTK_WINDOW(gWin),
                                                   GTK_DIALOG_MODAL,
                                                   yesStr, 1,
                                                   noStr, 0,
                                                   0);
        KeepOnTop(promptDialog);

        /* Make sure Enter will activate OK, not Cancel */
        /*
        gtk_dialog_set_default_response(GTK_DIALOG(promptDialog),
                                        GTK_RESPONSE_OK);
         */

        gtk_signal_connect(GTK_OBJECT(promptDialog), "key_press_event",
                           (GtkSignalFunc)HandlePromptKeyPress, 0);

        /* Make the label: */
        question = gtk_label_new(msg);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(promptDialog)->vbox),
                           question, TRUE, TRUE, 15);
        gtk_widget_show(question);
    }

    gtk_widget_show(promptDialog);

    qYesNo = gtk_dialog_run(GTK_DIALOG(promptDialog));

    gtk_widget_hide(promptDialog);
    return qYesNo;
}

static void SetNewFiles(GtkWidget *dialog, gint res)
{
	GSList *files, *cur;
    gboolean overwrite;

    if (res == GTK_RESPONSE_ACCEPT)
        overwrite = FALSE;
    else if (res == GTK_RESPONSE_OK)
        overwrite = TRUE;
    else {
        gtk_widget_destroy (dialog);
        return;
    }

    files = cur = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));

    if (overwrite)
        ClearImageList();

    gCurImage = 0;

    while (cur)
    {
        PhoImage* img = AddImage((char*)(cur->data));
        if (!gCurImage)
            gCurImage = img;

        cur = cur->next;
    }
    if (files)
        g_slist_free (files);

    gtk_widget_destroy (dialog);

    ThisImage();
}

void ChangeWorkingFileSet()
{
    GtkWidget* fsd = gtk_file_chooser_dialog_new("Change file set", NULL,
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_ADD, GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_NEW, GTK_RESPONSE_OK,
                                         NULL);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(fsd), TRUE);

    if (gCurImage && gCurImage->filename && gCurImage->filename[0] == '/')
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fsd),
                                            g_dirname(gCurImage->filename));
    else {
        char buf[BUFSIZ];
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fsd),
                                            getcwd(buf, BUFSIZ));
    }

	g_signal_connect(G_OBJECT(fsd), "response", G_CALLBACK(SetNewFiles), 0);
	gtk_widget_show(fsd);
}



