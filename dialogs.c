/*
 * dialogs.c: gtk dialogs used in pho, an image viewer.
 *
 * Copyright 2002 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>

#include "pho.h"

static GtkWidget* InfoDialog = 0;
static GtkWidget* InfoDEntry = 0;
static GtkWidget* InfoDImgName = 0;
static GtkWidget* InfoDImgSize = 0;
static GtkWidget* InfoDImgRotation = 0;
static GtkWidget* InfoFlag[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static GtkWidget* InfoDImgDeleted = 0;

void UpdateAndPopDown()
{
    int i, mask, flags;
    char* text = gtk_entry_get_text((GtkEntry*)InfoDEntry);
    gtk_widget_hide(InfoDialog);
    if (text && *text)
        AddComment(ArgP, text);
    SetDeleted(ArgP,
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(InfoDImgDeleted)));
    flags = 0;
    for (i=0, mask=1; i<10; ++i, mask <<= 1)
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(InfoFlag[i])))
            flags |= mask;
    }
    SetFlags(ArgP, flags);
}

static void PopdownInfoDialog(GtkWidget* widget, gpointer data)
{
    UpdateAndPopDown();
}

void UpdateInfoDialog()
{
    char buffer[256];
    char* s;
    int i, mask, flags;

    if (!InfoDialog || !(GTK_WIDGET_FLAGS(InfoDialog) & GTK_VISIBLE))
        return;

    s = GetComment(ArgP);
    gtk_entry_set_text(GTK_ENTRY(InfoDEntry), s ? s : "");

    gtk_label_set_text(GTK_LABEL(InfoDImgName), ArgV[ArgP]);
    sprintf(buffer, "Size: %d x %d", XSize, YSize);
    gtk_label_set_text(GTK_LABEL(InfoDImgSize), buffer);
    switch (GetRotation(ArgP))
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
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(InfoDImgDeleted),
                                 IsDeleted(ArgP) ? TRUE : FALSE);
    flags = GetFlags(ArgP);
    for (i=0, mask=1; i<10; ++i, mask <<= 1)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(InfoFlag[i]),
                                     (flags & mask) ? TRUE : FALSE);
}

static gint InfoDialogExpose(GtkWidget* widget, GdkEventKey* event)
{
    gtk_widget_grab_focus(InfoDEntry);
    UpdateInfoDialog();
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
    GtkWidget *label, *box;
    int i;

    if (InfoDialog)
    {
        if (GTK_WIDGET_FLAGS(InfoDialog) & GTK_VISIBLE)
            gtk_widget_hide(InfoDialog);
        else
            gtk_widget_show(InfoDialog);
        return;
    }

    InfoDialog = gtk_dialog_new();
    gtk_signal_connect(GTK_OBJECT(InfoDialog), "key_press_event",
                       (GtkSignalFunc)HandleInfoKeyPress, 0);

    // Make the button
    ok = gtk_button_new_with_label("Ok");
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(InfoDialog)->action_area),
                       ok, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(ok), "clicked",
                       (GtkSignalFunc)PopdownInfoDialog, 0);
    gtk_widget_show(ok);

    // Add the info items
    InfoDImgName = gtk_label_new("imgName");
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(InfoDialog)->vbox),
                       InfoDImgName, TRUE, TRUE, 0);
    InfoDImgSize = gtk_label_new("imgSize");
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(InfoDialog)->vbox),
                       InfoDImgSize, TRUE, TRUE, 0);
    gtk_widget_show(InfoDImgName);
    gtk_widget_show(InfoDImgSize);

    box = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new("Rotation:");
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    gtk_widget_show(label);
    InfoDImgRotation = gtk_label_new("imgRot");
    gtk_box_pack_start(GTK_BOX(box), InfoDImgRotation, TRUE, TRUE, 0);
    gtk_widget_show(InfoDImgRotation);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(InfoDialog)->vbox),
                       box, TRUE, TRUE, 0);
    gtk_widget_show(box);

    box = gtk_hbox_new(FALSE, 0);
    for (i=0; i<10; ++i)
    {
        char str[2] = { i + '0', '\0' };
        InfoFlag[i] = gtk_toggle_button_new_with_label(str);
        gtk_box_pack_start(GTK_BOX(box), InfoFlag[i], TRUE, TRUE, 0);
        gtk_widget_show(InfoFlag[i]);
    }
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(InfoDialog)->vbox),
                       box, TRUE, TRUE, 0);
    gtk_widget_show(box);

    InfoDImgDeleted = gtk_check_button_new_with_label("Deleted");
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(InfoDialog)->vbox),
                       InfoDImgDeleted, TRUE, TRUE, 0);
    gtk_widget_show(InfoDImgDeleted);

    box = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new("Comment:");
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    InfoDEntry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(box), InfoDEntry, TRUE, TRUE, 0);
    gtk_widget_show(label);
    gtk_widget_show(InfoDEntry);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(InfoDialog)->vbox),
                       box, TRUE, TRUE, 0);
    gtk_widget_show(box);

    gtk_signal_connect(GTK_OBJECT(InfoDialog), "expose_event",
                       (GtkSignalFunc)InfoDialogExpose, 0);

    gtk_widget_show(InfoDialog);
}
