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

#define GCONF_KEY_DISPLAY_BLANKING "/system/osso/dsm/display/inhibit_blank_mode"

// Shoud contain one, and only one "%d"
#define DISPLAY_BLANKING_ICON_TEMPLATE "display-blanking-icon.%d"

#define GETTEXT_DOM "status-area-displayblanking-applet"
#define gettext_noop(str) (str)

#define DISPLAY_BLANKING_MODES 5
static const char *_DisplayBlankingDescription[DISPLAY_BLANKING_MODES] =
{
    gettext_noop ("Both enabled"),
    gettext_noop ("Both only on battery"),
    gettext_noop ("Blanking only on battery"),
    gettext_noop ("Both disabled"),
    gettext_noop ("Only dimming")
};

struct _DisplayBlankingStatusPluginPrivate
{
    GtkWidget *button;
    GConfClient *gconf_client;
    gpointer data;
};

HD_DEFINE_PLUGIN_MODULE (DisplayBlankingStatusPlugin,
        display_blanking_status_plugin, HD_TYPE_STATUS_MENU_ITEM);

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
display_blanking_status_plugin_mode_set (DisplayBlankingStatusPluginPrivate *priv,
        gboolean update)
{
    // Should be enough if DISPLAY_BLANKING_MODES stays in 1 digit, "%d"
    // provides space for that digit and '\0'
    static char icon_name[sizeof (DISPLAY_BLANKING_ICON_TEMPLATE)];

    // Get the mode to set
    gint mode = gconf_client_get_int (priv->gconf_client,
            GCONF_KEY_DISPLAY_BLANKING, NULL);
    if (update)
        mode = (mode + 1) % DISPLAY_BLANKING_MODES;

    // Toggle display blanking
    gconf_client_set_int (priv->gconf_client, GCONF_KEY_DISPLAY_BLANKING,
            mode, NULL);

    // Update button text and status bar icon
    hildon_button_set_value (HILDON_BUTTON (priv->button),
            dgettext (GETTEXT_DOM, _DisplayBlankingDescription[mode]));
    int r = snprintf (icon_name, sizeof (icon_name),
            DISPLAY_BLANKING_ICON_TEMPLATE, mode);
    g_assert(r < sizeof (icon_name)); // otherwise it was truncated
    GtkWidget *icon = gtk_image_new_from_icon_name (icon_name,
            GTK_ICON_SIZE_DIALOG);
    hildon_button_set_image (HILDON_BUTTON (priv->button), icon);

    // Show a notification banner (only if updating)
    if (update) {
        GtkWidget *banner = hildon_banner_show_informationf (priv->button, NULL,
                dgettext (GETTEXT_DOM, "Changed display blanking mode to: %s"),
                _DisplayBlankingDescription[mode]);
        hildon_banner_set_timeout (HILDON_BANNER (banner), 5000);
    }
}

static void
display_blanking_status_plugin_on_button_clicked (GtkWidget *button,
        DisplayBlankingStatusPlugin *plugin)
{
    display_blanking_status_plugin_mode_set (
            DISPLAY_BLANKING_STATUS_PLUGIN_GET_PRIVATE (plugin), TRUE);
}

static void
display_blanking_status_plugin_init (DisplayBlankingStatusPlugin *plugin)
{
    plugin->priv = DISPLAY_BLANKING_STATUS_PLUGIN_GET_PRIVATE (plugin);

    plugin->priv->gconf_client = gconf_client_get_default();
    g_assert(GCONF_IS_CLIENT(plugin->priv->gconf_client));

    plugin->priv->button = hildon_button_new (HILDON_SIZE_FINGER_HEIGHT |
                HILDON_SIZE_AUTO_WIDTH, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
    gtk_button_set_alignment (GTK_BUTTON (plugin->priv->button), 0, 0);
    hildon_button_set_style (HILDON_BUTTON (plugin->priv->button),
            HILDON_BUTTON_STYLE_PICKER);
    hildon_button_set_title (HILDON_BUTTON (plugin->priv->button),
            dgettext (GETTEXT_DOM, "Display blanking mode"));

    display_blanking_status_plugin_mode_set (plugin->priv, FALSE);

    g_signal_connect (plugin->priv->button, "clicked",
            G_CALLBACK (display_blanking_status_plugin_on_button_clicked),
            plugin);

    gtk_container_add (GTK_CONTAINER (plugin), plugin->priv->button);

    gtk_widget_show_all (plugin->priv->button);

    gtk_widget_show (GTK_WIDGET (plugin));
}

