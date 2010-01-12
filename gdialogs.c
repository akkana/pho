/*
 * gdialogs.c: gtk dialogs used in pho, an image viewer.
 *
 * Copyright 2004 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include "pho.h"
#include "exif/phoexif.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>    /* for free() */

static GtkWidget* InfoDialog = 0;
static GtkWidget* InfoDEntry = 0;
static GtkWidget* InfoDImgName = 0;
static GtkWidget* InfoDImgSize = 0;
static GtkWidget* InfoDOrigSize = 0;
static GtkWidget* InfoDImgRotation = 0;
static GtkWidget* InfoFlag[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static GtkWidget* InfoExifContainer;
static GtkWidget* InfoExifEntries[NUM_EXIF_FIELDS];

static void AddComment(PhoImage* img, char* txt)
{
    if (img->comment != 0)
        free(img->comment);
    img->comment = strdup(txt);
}

void UpdateAndPopDown()
{
    int i, mask, flags;
    char* text = (char*)gtk_entry_get_text((GtkEntry*)InfoDEntry);
    gtk_widget_hide(InfoDialog);
    if (text && *text)
        AddComment(gCurImage, text);
            
    flags = 0;
    for (i=0, mask=1; i<10; ++i, mask <<= 1)
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(InfoFlag[i])))
            flags |= mask;
    }
    gCurImage->noteFlags = flags;
}

static void PopdownInfoDialog(GtkWidget* widget, gpointer data)
{
    UpdateAndPopDown();
}

void UpdateInfoDialog(PhoImage* img)
{
    char buffer[256];
    char* s;
    int i, mask, flags;

    if (!InfoDialog || !InfoDialog->window)
        /*|| !(GTK_WIDGET_FLAGS(InfoDialog) & GTK_VISIBLE)*/
        return;

    sprintf(buffer, "pho: %s info", img->filename);
    gtk_window_set_title(GTK_WINDOW(InfoDialog), buffer);

    s = img->comment;
    gtk_entry_set_text(GTK_ENTRY(InfoDEntry), s ? s : "");

    gtk_label_set_text(GTK_LABEL(InfoDImgName), img->filename);
    sprintf(buffer, "%d x %d", img->trueWidth, img->trueHeight);
    gtk_label_set_text(GTK_LABEL(InfoDOrigSize), buffer);
    sprintf(buffer, "%d x %d", img->curWidth, img->curHeight);
    gtk_label_set_text(GTK_LABEL(InfoDImgSize), buffer);
    switch (img->rotation)
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
    flags = img->noteFlags;
    for (i=0, mask=1; i<10; ++i, mask <<= 1)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(InfoFlag[i]),
                                     (flags & mask) ? TRUE : FALSE);

    /* Loop over the various EXIF elements.
     * Expect we already called ExifReadInfo, back in LoadImageFromFile.
     */
    if (HasExif(img))
        gtk_widget_set_sensitive(InfoExifContainer, TRUE);
    else
        gtk_widget_set_sensitive(InfoExifContainer, FALSE);
    for (i=0; i<NUM_EXIF_FIELDS; ++i)
    {
        if (HasExif()) {
            gtk_entry_set_text(GTK_ENTRY(InfoExifEntries[i]),
                               ExifGetString(i));
            //gtk_entry_set_editable(GTK_ENTRY(InfoExifEntries[i]), TRUE);
        }
        else {
            gtk_entry_set_text(GTK_ENTRY(InfoExifEntries[i]), " ");
            gtk_entry_set_editable(GTK_ENTRY(InfoExifEntries[i]), FALSE);
        }
    }
}

static gint InfoDialogExpose(GtkWidget* widget, GdkEventKey* event)
{
    printf("InfoDialogExpose\n");
    gtk_signal_handler_block_by_func(GTK_OBJECT(InfoDialog),
                                     (GtkSignalFunc)InfoDialogExpose, 0);
    gtk_widget_grab_focus(InfoDEntry);
    printf("Calling UpdateInfoDialog\n");
    UpdateInfoDialog(gCurImage);
    printf("Called UpdateInfoDialog\n");
    gtk_signal_handler_unblock_by_func(GTK_OBJECT(InfoDialog),
                                       (GtkSignalFunc)InfoDialogExpose, 0);
    return TRUE;
}

static gint HandleInfoKeyPress(GtkWidget* widget, GdkEventKey* event)
{
    switch (event->keyval)
    {
      case GDK_Escape:
      case GDK_Return:
      case GDK_KP_Enter:
          UpdateAndPopDown();
          return TRUE;
      default:
          return FALSE;
    }
    return FALSE;
}

/* Show a dialog with info about the current image.
 */
void ToggleInfo()
{
    GtkWidget *ok;
    GtkWidget *label, *vbox, *box, *scroller;
    int i;

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

    //vbox = GTK_DIALOG(InfoDialog)->vbox;
    //gtk_box_set_spacing(GTK_BOX(vbox), 3);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

    // Make the button
    ok = gtk_button_new_with_label("Ok");
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(InfoDialog)->action_area),
                       ok, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(ok), "clicked",
                       (GtkSignalFunc)PopdownInfoDialog, 0);
    gtk_widget_show(ok);

    // Add the info items
    InfoDImgName = gtk_label_new("imgName");
    gtk_box_pack_start(GTK_BOX(vbox), InfoDImgName, TRUE, TRUE, 4);
    gtk_widget_show(InfoDImgName);

    box = gtk_table_new(3, 2, FALSE);
    //gtk_table_set_row_spacings(GTK_TABLE(box), 2);
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
        char str[2] = { i + '0', '\0' };
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
    // set_padding doesn't work on tables, apparently
    //gtk_misc_set_padding(GTK_MISC(InfoExifContainer), 7, 7);
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

//    gtk_signal_connect(GTK_OBJECT(InfoDialog), "expose_event",
//                       (GtkSignalFunc)InfoDialogExpose, 0);

    gtk_widget_show(InfoDialog);
    UpdateInfoDialog(gCurImage);
}

/*
 * A generic Prompt dialog.
 */

static GtkWidget* promptDialog = 0;
static int qYesNo = -1;

static char* defaultYesChars = "yY\n";
static char* defaultNoChars = "nN";   /* ESC always works to cancel */
static char* gYesChars = 0;
static char* gNoChars = 0;

static void
promptCB(GtkWidget *widget, gpointer data)
{
    qYesNo = (int)data;
}

static gint
HandlePromptKeyPress(GtkWidget* widget, GdkEventKey* event)
{
    char c;

    if (event->keyval == GDK_Escape)
    {
        qYesNo = 0;
        return TRUE;
    }

    /* For anything else, map it to a printable and search in the strings */
    if (event->keyval == GDK_Return || event->keyval == GDK_KP_Enter)
        c = '\n';

    else if (event->keyval == GDK_space)
        c = ' ';

    else if (event->keyval >= GDK_A && event->keyval <= GDK_Z)
        c = event->keyval - GDK_A + 'A';

    else if (event->keyval >= GDK_a && event->keyval <= GDK_z)
        c = event->keyval - GDK_a + 'a';

    else if (event->keyval >= GDK_0 && event->keyval <= GDK_9)
        c = event->keyval - GDK_0 + '0';

    else {
        qYesNo = -1;
        gdk_beep();
        return FALSE;
    }

    /* Now we have a c: see if it's in the yes or no char lists */
    if (strchr(gYesChars, c))
    {
        qYesNo = 1;
        return TRUE;
    }
    else if (strchr(gNoChars, c))
    {
        qYesNo = 0;
        return TRUE;
    }

    qYesNo = -1;
    return FALSE;
}

int Prompt(char* msg, char* yesStr, char* noStr, char* yesChars, char* noChars)
{
    static GtkWidget* question = 0;
    static GtkWidget* yesBtn = 0;
    static GtkWidget* noBtn = 0;

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

        promptDialog = gtk_dialog_new();

        // This is stupid: to make the dialog modal we have to use
        // new_with_buttons, but that doesn't let us get to to the buttons
        // to change their labels later!
        // Figure out how to add modality some other time.
        //promptDialog = gtk_dialog_new_with_buttons("Prompt", 

        gtk_signal_connect(GTK_OBJECT(promptDialog), "key_press_event",
                           (GtkSignalFunc)HandlePromptKeyPress, 0);

        // Make the buttons:
        yesBtn = gtk_button_new_with_label(yesStr);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(promptDialog)->action_area),
                           yesBtn, TRUE, TRUE, 0);
        gtk_signal_connect(GTK_OBJECT(yesBtn), "clicked",
                           (GtkSignalFunc)promptCB, (gpointer)1);

        noBtn = gtk_button_new_with_label(noStr);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(promptDialog)->action_area),
                           noBtn, TRUE, TRUE, 0);
        gtk_signal_connect(GTK_OBJECT(noBtn), "clicked",
                           (GtkSignalFunc)promptCB, (gpointer)0);
        gtk_widget_show(yesBtn);
        gtk_widget_show(noBtn);

        // Make the label:
        question = gtk_label_new(msg);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(promptDialog)->vbox),
                           question, TRUE, TRUE, 15);
        gtk_widget_show(question);
    }

    gtk_widget_show(promptDialog);

    qYesNo = -1;
    while (qYesNo < 0)
        gtk_main_iteration();

    gtk_widget_hide(promptDialog);
    return qYesNo;
}



