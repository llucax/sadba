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
#include <dbus/dbus.h>
#include <mce/dbus-names.h>


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

#define HOURS_GCONF_KEY   "/apps/Maemo/sadba/timed_inhibit_hours"
#define MINUTES_GCONF_KEY "/apps/Maemo/sadba/timed_inhibit_minutes"

#define INHIBIT_MSG_INTERVAL 30 // in seconds

#define GETTEXT_DOM "status-area-displayblanking-applet"
#define _(str) dgettext (GETTEXT_DOM, (str))
#define gettext_noop(str) (str)
#define N_(str) gettext_noop(str)

// Undocumented blanking modes as reported by David Weinehall from Nokia:
// http://www.gossamer-threads.com/lists/maemo/developers/61201#61201
#define BLANKING_MODES 5
static const char *mode_title[BLANKING_MODES] =
{
    N_ ("Both enabled"),
    N_ ("Both only on battery"),
    N_ ("Blanking only on battery"),
    N_ ("Both disabled"),
    N_ ("Only dimming")
};
static const char *mode_desc[BLANKING_MODES] =
{
    N_ ("Blanking and dimming always enabled"),
    N_ ("Blanking and dimming enabled only on battery"),
    N_ ("Always dim, blank only on battery"),
    N_ ("Blanking and dimming always disabled"),
    N_ ("Blanking disabled, dimming enabled")
};
static const char *mode_icon_name[BLANKING_MODES] =
{
    "display-blanking-icon.0",
    "display-blanking-icon.1",
    "display-blanking-icon.2",
    "display-blanking-icon.3",
    "display-blanking-icon.4",
};
#define INHIBIT_ICON_NAME       "display-blanking-inhibit-icon"
#define TIMED_INHIBIT_ICON_NAME "display-blanking-inhibit-icon.timed"
#define INHIBIT_STATUS_ICON_NAME "display-blanking-status"

struct _DisplayBlankingStatusPluginPrivate
{
    DisplayBlankingStatusPlugin* plugin;
    GConfClient *gconf_client;
    DBusConnection* dbus_conn;
    DBusMessage* dbus_msg;
    GtkWidget *mode_button;
    GtkWidget *mode_dialog;
    GtkWidget *inhibit_button;
    GtkWidget *timed_inhibit_button;
    GtkWidget *timed_inhibit_dialog;
    gint inhibit_timer_id;       // if == 0
    gint timed_inhibit_timer_id; //     no timer is set
    // gtk_toggle_button_set_active () triggers the "clicked" signal on the
    // affected button, since we don't want to process the signal while
    // changing the "pressed" state (we want just the GUI to change, we use
    // this flag to ignore the "clicke" signal handler when is TRUE
    gboolean inhibit_in_signal;
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
    GtkWidget *icon = gtk_image_new_from_icon_name (mode_icon_name[mode],
            GTK_ICON_SIZE_DIALOG);
    gtk_button_set_image (GTK_BUTTON (priv->mode_button), icon);
}

static void
disable_timer (gint *timer_id)
{
    g_assert (*timer_id != 0);
    gboolean ok = g_source_remove (*timer_id);
    g_assert (ok == TRUE);
    *timer_id = 0;
}

static void
disable_inhibition (DisplayBlankingStatusPluginPrivate *priv)
{
    disable_timer (&(priv->inhibit_timer_id));
    hd_status_plugin_item_set_status_area_icon (
            HD_STATUS_PLUGIN_ITEM (priv->plugin), NULL);
}

static void
inhibit_display_blanking (DisplayBlankingStatusPluginPrivate *priv)
{
    dbus_bool_t ok = dbus_connection_send (priv->dbus_conn, priv->dbus_msg,
            NULL);
    g_assert (ok == TRUE);
}

static gboolean
on_inhibit_timeout (DisplayBlankingStatusPluginPrivate *priv)
{
    inhibit_display_blanking (priv);

    return TRUE;
}

static gboolean
on_timed_inhibit_timeout (DisplayBlankingStatusPluginPrivate *priv)
{
    disable_inhibition (priv);
    disable_timer (&(priv->timed_inhibit_timer_id));

    priv->inhibit_in_signal = TRUE;
    gtk_toggle_button_set_active (
            GTK_TOGGLE_BUTTON (priv->timed_inhibit_button), FALSE);
    priv->inhibit_in_signal = FALSE;

    GtkWidget *banner = hildon_banner_show_information (
            priv->timed_inhibit_button, NULL,
            _ ("Display blanking inhibition disabled"));
    hildon_banner_set_timeout (HILDON_BANNER (banner), 5000);

    return FALSE;
}

static void
enable_inhibition (DisplayBlankingStatusPluginPrivate *priv)
{
    inhibit_display_blanking (priv);

    g_assert (priv->inhibit_timer_id == 0);
    priv->inhibit_timer_id = g_timeout_add_seconds (INHIBIT_MSG_INTERVAL,
            (GSourceFunc) on_inhibit_timeout, priv);
    g_assert (priv->inhibit_timer_id > 0);

    GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
    GdkPixbuf *pixbuf = gtk_icon_theme_load_icon (icon_theme,
            INHIBIT_STATUS_ICON_NAME, 18, GTK_ICON_LOOKUP_NO_SVG, NULL);
    hd_status_plugin_item_set_status_area_icon (
            HD_STATUS_PLUGIN_ITEM (priv->plugin), pixbuf);
}

static void
on_inhibit_button_clicked (GtkWidget *button,
        DisplayBlankingStatusPluginPrivate *priv)
{
    if (priv->inhibit_in_signal)
        return;

    GtkWidget *parent = gtk_widget_get_ancestor (button, GTK_TYPE_WINDOW);
    gtk_widget_hide (parent);

    gboolean self_pressed = gtk_toggle_button_get_active (
            GTK_TOGGLE_BUTTON (button));
    gboolean other_pressed = gtk_toggle_button_get_active (
            GTK_TOGGLE_BUTTON (priv->timed_inhibit_button));

    if (self_pressed && other_pressed) {
        g_assert (priv->inhibit_timer_id != 0);

        disable_timer (&(priv->timed_inhibit_timer_id));

        priv->inhibit_in_signal = TRUE;
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (
                priv->timed_inhibit_button), FALSE);
        priv->inhibit_in_signal = FALSE;
    }
    else if (self_pressed && !other_pressed) {
        g_assert (priv->timed_inhibit_timer_id == 0);

        enable_inhibition (priv);
    }
    else if (!self_pressed) {
        g_assert (!other_pressed);
        g_assert (priv->timed_inhibit_timer_id == 0);

        disable_inhibition (priv);
    }
    else
        g_assert (FALSE);
}

static GtkWidget *
timed_inhibit_picker_new (const gchar* title, gsize current, guint max,
        guint step)
{
    g_assert (max < 100);
    static gchar buffer[3]; // 2 for the number + 1 for \0

    GtkWidget *selector = hildon_touch_selector_entry_new_text ();
    gint selected = -1;
    for (int i = 0; i*step <= max; i++) {
        if (i*step == current)
            selected = i;
        hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (selector),
                g_ascii_formatd (buffer, 3, "%.0f", i*step));
    }
    hildon_gtk_entry_set_input_mode (GTK_ENTRY (
                hildon_touch_selector_entry_get_entry (
                    HILDON_TOUCH_SELECTOR_ENTRY (selector))),
            HILDON_GTK_INPUT_MODE_NUMERIC);

    GtkWidget *picker = hildon_picker_button_new (HILDON_SIZE_FINGER_HEIGHT,
            HILDON_BUTTON_ARRANGEMENT_VERTICAL);
    hildon_button_set_title (HILDON_BUTTON (picker), title);
    hildon_picker_button_set_selector (HILDON_PICKER_BUTTON (picker),
            HILDON_TOUCH_SELECTOR (selector));
    hildon_picker_button_set_active (HILDON_PICKER_BUTTON (picker),
            selected);
    hildon_button_set_value (HILDON_BUTTON (picker),
                g_ascii_formatd (buffer, 3, "%.0f", current));

    g_object_set_data (G_OBJECT (picker), "max", GUINT_TO_POINTER (max));

    return picker;
}

static guint
timed_inhibit_picker_get_value (GtkWidget *picker)
{
    guint max = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (picker), "max"));

    return CLAMP (g_ascii_strtod (hildon_button_get_value (
                    HILDON_BUTTON (picker)), NULL), 0, max);
}

static guint
timed_inhibit_get_input (DisplayBlankingStatusPluginPrivate *priv)
{
    g_assert (priv->timed_inhibit_dialog == NULL);
    priv->timed_inhibit_dialog = gtk_dialog_new_with_buttons (
            _ ("Inhibit display blanking for..."), NULL, GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);

    GtkWidget *h_picker = timed_inhibit_picker_new (_ ("Hours"),
            gconf_client_get_int (priv->gconf_client, HOURS_GCONF_KEY, NULL),
            24, 1);
    GtkWidget *m_picker = timed_inhibit_picker_new (_ ("Minutes"),
            gconf_client_get_int (priv->gconf_client, MINUTES_GCONF_KEY, NULL),
            60, 10);

    GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
    g_assert (hbox != NULL);

    gtk_container_add (GTK_CONTAINER (hbox), h_picker);
    gtk_container_add (GTK_CONTAINER (hbox), m_picker);

    GtkWidget *content_area = gtk_dialog_get_content_area (
            GTK_DIALOG (priv->timed_inhibit_dialog));
    gtk_container_add (GTK_CONTAINER (content_area), hbox);

    gtk_widget_show_all (priv->timed_inhibit_dialog);

    gint result = gtk_dialog_run (GTK_DIALOG (priv->timed_inhibit_dialog));

    guint timeout = 0;
    if (result == GTK_RESPONSE_ACCEPT) {
        gint hours = timed_inhibit_picker_get_value (h_picker);
        gint mins = timed_inhibit_picker_get_value (m_picker);

        GError *e = NULL;
        gconf_client_set_int (priv->gconf_client, HOURS_GCONF_KEY, hours, &e);
        g_assert (e == NULL);
        gconf_client_set_int (priv->gconf_client, MINUTES_GCONF_KEY, mins, &e);
        g_assert (e == NULL);

        timeout = hours*3600 + mins*60;
    }

    gtk_widget_destroy (priv->timed_inhibit_dialog);
    priv->timed_inhibit_dialog = NULL;

    return timeout;
}

static void
on_timed_inhibit_button_clicked (GtkWidget *button,
        DisplayBlankingStatusPluginPrivate *priv)
{
    if (priv->inhibit_in_signal)
        return;

    GtkWidget *parent = gtk_widget_get_ancestor (GTK_WIDGET (priv->mode_button),
            GTK_TYPE_WINDOW);
    gtk_widget_hide (parent);

    gboolean self_pressed = gtk_toggle_button_get_active (
            GTK_TOGGLE_BUTTON (button));
    gboolean other_pressed = gtk_toggle_button_get_active (
            GTK_TOGGLE_BUTTON (priv->inhibit_button));

    if (self_pressed) {
        g_assert (priv->timed_inhibit_timer_id == 0);
        if (other_pressed)
            g_assert (priv->inhibit_timer_id != 0);

        guint timeout = timed_inhibit_get_input (priv);
        if (timeout) {
            if (other_pressed) {
                priv->inhibit_in_signal = TRUE;
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (
                            priv->inhibit_button), FALSE);
                priv->inhibit_in_signal = FALSE;
            }
            else
                enable_inhibition (priv);

            priv->timed_inhibit_timer_id = g_timeout_add_seconds (timeout,
                    (GSourceFunc) on_timed_inhibit_timeout, priv);
            g_assert (priv->timed_inhibit_timer_id > 0);
        }
        else {
            priv->inhibit_in_signal = TRUE;
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (
                        priv->timed_inhibit_button), FALSE);
            priv->inhibit_in_signal = FALSE;
        }
    }
    else { // !self_pressed
        g_assert (!other_pressed);

        disable_inhibition (priv);
        disable_timer (&(priv->timed_inhibit_timer_id));
    }
}

static void
on_mode_dialog_button_clicked (GtkWidget *button, GtkDialog *dialog)
{
    const gchar *title = hildon_button_get_title (HILDON_BUTTON (button));

    gint *mode = (gint *) g_object_get_data (G_OBJECT (dialog), "mode");
    g_assert (mode != NULL);

    for (*mode = 0; *mode < BLANKING_MODES; (*mode)++) {
        if (strcmp (title, mode_title[*mode]) == 0)
            break;
    }
    g_assert (*mode < BLANKING_MODES);

    gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static gint
mode_get_input (DisplayBlankingStatusPluginPrivate *priv)
{
    g_assert (priv->mode_dialog == NULL);
    priv->mode_dialog = gtk_dialog_new ();
    gtk_window_set_modal (GTK_WINDOW (priv->mode_dialog), TRUE);
    gtk_window_set_title (GTK_WINDOW (priv->mode_dialog),
            _ ("Select display blanking mode"));

    GtkWidget *pan_area = hildon_pannable_area_new ();
    g_assert (pan_area != NULL);

    GtkWidget *vbox = gtk_vbox_new (FALSE, 0);
    g_assert (vbox != NULL);

    hildon_pannable_area_add_with_viewport (HILDON_PANNABLE_AREA (pan_area),
            vbox);
    GtkWidget *content_area = gtk_dialog_get_content_area (
            GTK_DIALOG (priv->mode_dialog));
    gtk_box_pack_start (GTK_BOX (content_area), pan_area, TRUE, TRUE, 0);

    gtk_widget_set_size_request (pan_area, -1, MIN (350, BLANKING_MODES * 70));

    gint mode = BLANKING_MODES;
    for (int i = 0; i < BLANKING_MODES; i++) {
        GtkWidget *button =
                hildon_button_new_with_text (HILDON_SIZE_FINGER_HEIGHT,
                    HILDON_BUTTON_ARRANGEMENT_VERTICAL, _ (mode_title[i]),
                    _ (mode_desc[i]));
        hildon_button_set_style (HILDON_BUTTON (button),
            HILDON_BUTTON_STYLE_PICKER);
        GtkWidget *icon = gtk_image_new_from_icon_name (mode_icon_name[i],
                GTK_ICON_SIZE_DIALOG);
        hildon_button_set_image (HILDON_BUTTON (button), icon);
        gtk_button_set_alignment (GTK_BUTTON (button), 0.0f, 0.5f);
        gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
        g_signal_connect (button, "clicked",
                G_CALLBACK (on_mode_dialog_button_clicked), priv->mode_dialog);
    }

    g_object_set_data (G_OBJECT (priv->mode_dialog), "mode", &mode);
    gtk_widget_show_all (priv->mode_dialog);
    gtk_dialog_run (GTK_DIALOG (priv->mode_dialog));

    gtk_widget_destroy (priv->mode_dialog);
    priv->mode_dialog = NULL;

    return mode;
}

static void
on_mode_button_clicked (GtkWidget *button,
        DisplayBlankingStatusPluginPrivate *priv)
{
    GtkWidget *parent = gtk_widget_get_ancestor (GTK_WIDGET (priv->mode_button),
            GTK_TYPE_WINDOW);
    gtk_widget_hide (parent);

    gint mode = mode_get_input (priv);

    if (mode != BLANKING_MODES) {
        // will trigger the gconf notify signal
        GError *error = NULL;
        gconf_client_set_int (priv->gconf_client, MODE_GCONF_KEY, mode, &error);
        g_assert (error == NULL);
    }
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
init_dbus (DisplayBlankingStatusPluginPrivate *priv)
{
    DBusError error;
    dbus_error_init (&error);

    priv->dbus_conn = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
    g_assert (!dbus_error_is_set (&error));
    g_assert (priv->dbus_conn != NULL);

    priv->dbus_msg = dbus_message_new_method_call (MCE_SERVICE,
            MCE_REQUEST_PATH, MCE_REQUEST_IF, MCE_PREVENT_BLANK_REQ);
    g_assert (priv->dbus_msg != NULL);
    dbus_message_set_no_reply (priv->dbus_msg, TRUE);
}

static void
init_mode_gui (DisplayBlankingStatusPluginPrivate *priv)
{
    priv->mode_dialog = NULL;
    priv->mode_button = hildon_gtk_button_new (HILDON_SIZE_FINGER_HEIGHT |
                HILDON_SIZE_AUTO_WIDTH);

    GError* error = NULL;
    gint mode = gconf_client_get_int (priv->gconf_client, MODE_GCONF_KEY,
            &error);
    g_assert (error == NULL);
    update_mode_gui (mode, priv);

    g_signal_connect (priv->mode_button, "clicked",
            G_CALLBACK (on_mode_button_clicked), priv);
}

static GtkWidget *
inhibit_button_new (const gchar *icon_name,
        void (*cb) (GtkWidget *, DisplayBlankingStatusPluginPrivate *),
        gpointer cb_data)
{

    GtkWidget *b = hildon_gtk_toggle_button_new (HILDON_SIZE_FINGER_HEIGHT |
            HILDON_SIZE_AUTO_WIDTH);
    gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (b), FALSE);
    GtkWidget *icon = gtk_image_new_from_icon_name (icon_name,
            GTK_ICON_SIZE_DIALOG);
    gtk_button_set_image (GTK_BUTTON (b), icon);
    g_signal_connect (b, "clicked", G_CALLBACK (cb), cb_data);

    return b;
}

static void
init_inhibit_gui (DisplayBlankingStatusPluginPrivate *priv)
{
    priv->inhibit_in_signal = FALSE;

    priv->inhibit_timer_id = 0;
    priv->timed_inhibit_timer_id = 0;

    priv->inhibit_button = inhibit_button_new (INHIBIT_ICON_NAME,
            on_inhibit_button_clicked, priv);

    priv->timed_inhibit_button = inhibit_button_new (
            TIMED_INHIBIT_ICON_NAME, on_timed_inhibit_button_clicked, priv);
}

static void
display_blanking_status_plugin_init (DisplayBlankingStatusPlugin *plugin)
{
    DisplayBlankingStatusPluginPrivate *priv;

    priv = DISPLAY_BLANKING_STATUS_PLUGIN_GET_PRIVATE (plugin);
    plugin->priv = priv;
    priv->plugin = plugin;

    init_gconf (priv);
    init_dbus (priv);
    init_mode_gui (priv);
    init_inhibit_gui (priv);

    GtkWidget *hbbox = gtk_hbutton_box_new ();
    g_assert (hbbox != NULL);

    gtk_container_add (GTK_CONTAINER (hbbox), priv->mode_button);
    gtk_container_add (GTK_CONTAINER (hbbox), priv->inhibit_button);
    gtk_container_add (GTK_CONTAINER (hbbox), priv->timed_inhibit_button);

    gtk_container_add (GTK_CONTAINER (plugin), hbbox);

    gtk_widget_show_all (GTK_WIDGET (plugin));
}

