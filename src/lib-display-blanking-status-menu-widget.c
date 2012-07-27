 /***********************************************************************************
 *  Display blanking status area plugin
 *  Copyright (C) 2012 Leandro Lucarella
 *  Based on status-area-orientationlock-applet by Mohammad Abu-Garbeyyeh.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ***********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include <libhildondesktop/libhildondesktop.h>
#include <gconf/gconf-client.h>


#define TYPE_DISPLAY_BLANKING_STATUS_PLUGIN (display_blanking_status_plugin_get_type ())

#define DISPLAY_BLANKING_STATUS_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                    TYPE_DISPLAY_BLANKING_STATUS_PLUGIN, DisplayBlankingStatusPlugin))

#define DISPLAY_BLANKING_STATUS_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                TYPE_DISPLAY_BLANKING_STATUS_PLUGIN, DisplayBlankingStatusPluginClass))

#define IS_DISPLAY_BLANKING_STATUS_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                                    TYPE_DISPLAY_BLANKING_STATUS_PLUGIN))

#define IS_DISPLAY_BLANKING_STATUS_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                                    TYPE_DISPLAY_BLANKING_STATUS_PLUGIN))

#define DISPLAY_BLANKING_STATUS_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                            TYPE_DISPLAY_BLANKING_STATUS_PLUGIN, DisplayBlankingStatusPluginClass))

#define STATUS_AREA_DISPLAY_BLANKING_ICON_SIZE 18

typedef struct _DisplayBlankingStatusPlugin        DisplayBlankingStatusPlugin;
typedef struct _DisplayBlankingStatusPluginClass   DisplayBlankingStatusPluginClass;
typedef struct _DisplayBlankingStatusPluginPrivate DisplayBlankingStatusPluginPrivate;

struct _DisplayBlankingStatusPlugin
{
    HDStatusMenuItem parent;

    DisplayBlankingStatusPluginPrivate *priv;
};

struct _DisplayBlankingStatusPluginClass
{
    HDStatusMenuItemClass parent;
};

GType display_blanking_status_plugin_get_type (void);

#define DISPLAY_BLANKING_STATUS_PLUGIN_GET_PRIVATE(obj) \
        (G_TYPE_INSTANCE_GET_PRIVATE (obj, \
                TYPE_DISPLAY_BLANKING_STATUS_PLUGIN, \
                DisplayBlankingStatusPluginPrivate))

#define MODE_GCONF_ROOT "/system/osso/dsm/display"
#define MODE_GCONF_KEY  MODE_GCONF_ROOT "/inhibit_blank_mode"

#define INHIBIT_MSG_INTERVAL 30 // in seconds

#define GETTEXT_DOM "status-area-displayblanking-applet"
#define gettext_noop(str) (str)

// Undocumented blanking modes as reported by David Weinehall from Nokia:
// http://www.gossamer-threads.com/lists/maemo/developers/61201#61201
#define BLANKING_MODES 5
static const char *_DisplayBlankingDescription[BLANKING_MODES] =
{
    gettext_noop ("Both enabled"),
    gettext_noop ("Both only on battery"),
    gettext_noop ("Blanking only on battery"),
    gettext_noop ("Both disabled"),
    gettext_noop ("Only dimming")
};
static const char *mode_icon_name[BLANKING_MODES] =
{
    "display-blanking-icon.0",
    "display-blanking-icon.1",
    "display-blanking-icon.2",
    "display-blanking-icon.3",
    "display-blanking-icon.4",
};

struct _DisplayBlankingStatusPluginPrivate
{
    GConfClient *gconf_client;
    GtkWidget *mode_button;
    GtkWidget *mode_dialog;
    gpointer data;
};

HD_DEFINE_PLUGIN_MODULE (DisplayBlankingStatusPlugin,
        display_blanking_status_plugin, HD_TYPE_STATUS_MENU_ITEM)

static void
display_blanking_status_plugin_class_finalize (
        DisplayBlankingStatusPluginClass *klass)
{
}

static void
display_blanking_status_plugin_class_init (DisplayBlankingStatusPluginClass *c)
{
    g_type_class_add_private (c, sizeof (DisplayBlankingStatusPluginPrivate));
}

static void
update_mode_gui (gint mode, DisplayBlankingStatusPluginPrivate *priv)
{
    hildon_button_set_value (HILDON_BUTTON (priv->mode_button),
            dgettext (GETTEXT_DOM, _DisplayBlankingDescription[mode]));
    GtkWidget *icon = gtk_image_new_from_icon_name (mode_icon_name[mode],
            GTK_ICON_SIZE_DIALOG);
    hildon_button_set_image (HILDON_BUTTON (priv->mode_button), icon);
}

static void
on_mode_dialog_button_clicked (GtkWidget *button, GtkDialog *dialog)
{
    const gchar *title = hildon_button_get_title (HILDON_BUTTON (button));

    gint *mode = (gint *) g_object_get_data (G_OBJECT(dialog), "mode");
    g_assert (mode != NULL);

    for (*mode = 0; *mode < BLANKING_MODES; (*mode)++) {
        if (strcmp (title, _DisplayBlankingDescription[*mode]) == 0)
            break;
    }
    g_assert (*mode < BLANKING_MODES);

    gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
on_mode_button_clicked (GtkWidget *button,
        DisplayBlankingStatusPluginPrivate *priv)
{
    GtkWidget *parent = gtk_widget_get_ancestor (GTK_WIDGET (priv->mode_button),
            GTK_TYPE_WINDOW);
    gtk_widget_hide (parent);

    g_assert (priv->mode_dialog == NULL);
    priv->mode_dialog = gtk_dialog_new ();
    gtk_window_set_modal (GTK_WINDOW (priv->mode_dialog), TRUE);
    gtk_window_set_title (GTK_WINDOW (priv->mode_dialog),
            dgettext (GETTEXT_DOM, "Select display blanking mode"));

    GtkWidget *pan_area = hildon_pannable_area_new ();
    g_assert (pan_area != NULL);

    GtkWidget *vbox = gtk_vbox_new (FALSE, 0);
    g_assert (vbox != NULL);

    hildon_pannable_area_add_with_viewport (HILDON_PANNABLE_AREA (pan_area),
            vbox);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (priv->mode_dialog)->vbox),
            pan_area, TRUE, TRUE, 0);

    gtk_widget_set_size_request (pan_area, -1, MIN (350, BLANKING_MODES * 70));

    gint mode = BLANKING_MODES;
    for (int i = 0; i < BLANKING_MODES; i++) {
        GtkWidget *button =
                hildon_button_new_with_text (HILDON_SIZE_FINGER_HEIGHT,
                    HILDON_BUTTON_ARRANGEMENT_VERTICAL,
                    dgettext (GETTEXT_DOM, (_DisplayBlankingDescription[i])),
                    NULL);
        GtkWidget *icon = gtk_image_new_from_icon_name (mode_icon_name[i],
                GTK_ICON_SIZE_DIALOG);
        hildon_button_set_image (HILDON_BUTTON (button), icon);
        gtk_button_set_alignment (GTK_BUTTON (button), 0.0f, 0.5f);
        gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
        g_signal_connect (button, "clicked",
                G_CALLBACK (on_mode_dialog_button_clicked), priv->mode_dialog);
    }

    gtk_widget_show_all (priv->mode_dialog);

    g_object_set_data (G_OBJECT (priv->mode_dialog), "mode", &mode);
    gtk_dialog_run (GTK_DIALOG (priv->mode_dialog));

    if (mode != BLANKING_MODES) {
        // will trigger the gconf notify signal
        GError *error = NULL;
        gconf_client_set_int (priv->gconf_client, MODE_GCONF_KEY, mode, &error);
        g_assert (error == NULL);
    }

    gtk_widget_destroy (priv->mode_dialog);
    priv->mode_dialog = NULL;
}

static void
on_gconf_notify (GConfClient* client, guint cnxn_id, GConfEntry* entry,
        DisplayBlankingStatusPluginPrivate* priv)
{
    const gchar* key = gconf_entry_get_key (entry);
    g_assert (key != NULL);

    // Ignore notification about keys we don't care about
    if (strcmp (key, MODE_GCONF_KEY) != 0)
        return;

    const GConfValue* value = gconf_entry_get_value (entry);
    g_assert (value != NULL);
    g_assert (GCONF_VALUE_TYPE_VALID (value->type));
    g_assert (value->type == GCONF_VALUE_INT);

    gint mode = gconf_value_get_int (value);
    update_mode_gui (mode, priv);
}

static void
init_gconf (DisplayBlankingStatusPluginPrivate *priv)
{
    GError* error = NULL;

    priv->gconf_client = gconf_client_get_default ();
    g_assert (GCONF_IS_CLIENT (priv->gconf_client));

    gconf_client_add_dir (priv->gconf_client, MODE_GCONF_ROOT,
            GCONF_CLIENT_PRELOAD_NONE, &error);
    g_assert (error == NULL);

    gconf_client_notify_add (priv->gconf_client, MODE_GCONF_KEY,
            (GConfClientNotifyFunc) &on_gconf_notify, priv, NULL, &error);
    g_assert (error == NULL);
}

static void
init_mode_gui (DisplayBlankingStatusPluginPrivate *priv)
{
    priv->mode_dialog = NULL;
    priv->mode_button = hildon_button_new (HILDON_SIZE_FINGER_HEIGHT |
                HILDON_SIZE_AUTO_WIDTH, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
    gtk_button_set_alignment (GTK_BUTTON (priv->mode_button), 0, 0);
    hildon_button_set_style (HILDON_BUTTON (priv->mode_button),
            HILDON_BUTTON_STYLE_PICKER);
    hildon_button_set_title (HILDON_BUTTON (priv->mode_button),
            dgettext (GETTEXT_DOM, "Display blanking mode"));

    GError* error = NULL;
    gint mode = gconf_client_get_int (priv->gconf_client, MODE_GCONF_KEY,
            &error);
    g_assert (error == NULL);
    update_mode_gui (mode, priv);

    g_signal_connect (priv->mode_button, "clicked",
            G_CALLBACK (on_mode_button_clicked), priv);
}

static void
display_blanking_status_plugin_init (DisplayBlankingStatusPlugin *plugin)
{
    DisplayBlankingStatusPluginPrivate *priv;

    priv = DISPLAY_BLANKING_STATUS_PLUGIN_GET_PRIVATE (plugin);
    plugin->priv = priv;

    init_gconf (priv);
    init_mode_gui (priv);

    gtk_container_add (GTK_CONTAINER (plugin), priv->mode_button);

    gtk_widget_show_all (priv->mode_button);

    gtk_widget_show (GTK_WIDGET (plugin));
}

