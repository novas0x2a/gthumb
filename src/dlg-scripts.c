/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2001, 2003, 2004, 2005 Free Software Foundation, Inc.
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
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-help.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "file-utils.h"
#include "gth-utils.h"
#include "gth-window.h"
#include "gtk-utils.h"
#include "glib-utils.h"
#include "main.h"
#include "preferences.h"
#include "gconf-utils.h"

#define SCRIPT_GLADE_FILE "gthumb_tools.glade"

#define MAX_SCRIPTS 10

typedef struct {
        GthWindow    *window;
	DoneFunc      done_func;
	gpointer      done_data;
        GladeXML     *gui;
        GtkWidget    *dialog;
} DialogData;


typedef struct {
        GladeXML     *gui;
        GtkWidget    *dialog;
	gboolean      cancel;
} ProgressData;


typedef struct {
        gint   number;
        gchar *short_name;
        gchar *script_text;
} ScriptStruct;


enum {
        COLUMN_SCRIPT_NUMBER,
        COLUMN_SHORT_NAME,
        COLUMN_SCRIPT_TEXT,
        NUMBER_OF_COLUMNS
};


static GArray *script_array = NULL;


/* called when the main dialog is closed. */
static void
progress_cancel_cb (GtkWidget    *widget,
                    ProgressData *data)
{
	data->cancel = TRUE;	
}

static void
exec_shell_script (GtkWindow  *window,
		   const char *script,
		   const char *name,
		   GList      *file_list)
{
	ProgressData *data;
	GtkWidget    *label;
	GtkWidget    *bar;
	GtkWidget    *progress_cancel_button;
	GList        *scan;
	char	     *full_name;
	int           i, n;

	if ((script == NULL) || (file_list == NULL))
		return;

	data = g_new0 (ProgressData, 1);

	/* Add a progress indicator */
	data->gui = glade_xml_new (GTHUMB_GLADEDIR "/" SCRIPT_GLADE_FILE, NULL,
                                   NULL);

        if (!data->gui) {
                g_warning ("Could not find " SCRIPT_GLADE_FILE "\n");
                return;
        }

	data->dialog = glade_xml_get_widget (data->gui, "hotkey_progress");
	label = glade_xml_get_widget (data->gui, "progress_info");
	bar = glade_xml_get_widget (data->gui, "progress_progressbar");
	progress_cancel_button = glade_xml_get_widget (data->gui, "progress_cancel_button");

	n = g_list_length (file_list);

	if (name == NULL)
		gtk_window_set_title (GTK_WINDOW (data->dialog), _("Script Progress"));
	else {
		full_name = g_strconcat (_("Script Progress"), " - ", name, NULL);
		gtk_window_set_title (GTK_WINDOW (data->dialog), full_name);
	}

	data->cancel = FALSE;

        g_signal_connect (G_OBJECT (data->dialog),
                          "destroy",
                          G_CALLBACK (progress_cancel_cb),
                          data);

        g_signal_connect (G_OBJECT (progress_cancel_button),
                          "clicked",
                          G_CALLBACK (progress_cancel_cb),
                          data);

	gtk_window_set_transient_for (GTK_WINDOW (data->dialog), GTK_WINDOW (window));
	gtk_window_set_modal (GTK_WINDOW (data->dialog), TRUE);

	gtk_widget_show (data->dialog);

	while (gtk_events_pending())
		gtk_main_iteration();

	/* If the %F code is present, all selected files are processed by
	   one script instance. Otherwise, each file is handled sequentially. */

	if (strstr (script, "%F")) {
		char *command = NULL;
		char *file_list_string;

		file_list_string = g_strdup (" ");

		for (scan = file_list; scan; scan = scan->next) {
			char *filename;
			char *e_filename;
			char *new_file_list;

			if (is_local_file (scan->data))
				filename = gnome_vfs_unescape_string_for_display (remove_host_from_uri (scan->data));
			else
				filename = gnome_vfs_unescape_string_for_display (scan->data);

			e_filename = shell_escape (filename);

			new_file_list = g_strconcat (file_list_string, e_filename, " ", NULL);

			g_free (e_filename);
			g_free (file_list_string);
			file_list_string = g_strdup (new_file_list);

			g_free (new_file_list);
		}
		command = _g_substitute_pattern (script, 'F', file_list_string);
		g_free (file_list_string);

		system (command);
		g_free (command);

		_gtk_label_set_filename_text (GTK_LABEL (label), script);
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (bar),
					       (gdouble) 1.0);
		while (gtk_events_pending())
			gtk_main_iteration();

		if (data->cancel)
			_gtk_error_dialog_run (GTK_WINDOW (window), 
					       _("Scripts using the %%F code can not be cancelled. Sorry!"));

	} else {
		i = 0;
		for (scan = file_list; scan && !data->cancel; scan = scan->next) {
			char *filename;
			char *e_filename;
			char *name_wo_ext = NULL;
			char *extension = NULL;
			char *parent = NULL;
			char *basename = NULL;
			char *basename_wo_ext = NULL;
			char *command0 = NULL;
			char *command1 = NULL;
			char *command2 = NULL;
			char *command3 = NULL;
			char *command4 = NULL;
			char *command5 = NULL;

			if (is_local_file (scan->data))
				filename = gnome_vfs_unescape_string_for_display (remove_host_from_uri (scan->data));
			else
				filename = gnome_vfs_unescape_string_for_display (scan->data);

			name_wo_ext = remove_extension_from_path (filename);
			extension = g_filename_to_utf8 (strrchr (filename, '.'), -1, 0, 0, 0);
			parent = remove_level_from_path (filename);
			basename = g_strdup (file_name_from_path (filename));
			basename_wo_ext = remove_extension_from_path (basename);

			e_filename = shell_escape (filename);
			command5 = _g_substitute_pattern (script, 'f', e_filename);
			g_free (e_filename);

                        e_filename = shell_escape (basename);
                        command4 = _g_substitute_pattern (command5, 'b', e_filename);
                        g_free (e_filename);
                        g_free (command5);

			e_filename = shell_escape (name_wo_ext);
			command3 = _g_substitute_pattern (command4, 'n', e_filename);
			g_free (e_filename);
			g_free (command4);

                        e_filename = shell_escape (basename_wo_ext);
                        command2 = _g_substitute_pattern (command3, 'm', e_filename);
                        g_free (e_filename);
                        g_free (command3);

			e_filename = shell_escape (extension);
			command1 = _g_substitute_pattern (command2, 'e', e_filename);
			g_free (e_filename);
			g_free (command2);

			e_filename = shell_escape (parent);
			command0 = _g_substitute_pattern (command1, 'p', e_filename);
			g_free (e_filename);
			g_free (command1);

			g_free (filename);
			g_free (name_wo_ext);
			g_free (extension);
			g_free (parent);
			g_free (basename);
			g_free (basename_wo_ext);

			_gtk_label_set_filename_text (GTK_LABEL (label), command0);
			gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (bar),
						       (gdouble) (i + 0.5) / n);

			system (command0);
			g_free (command0);

			while (gtk_events_pending())
				gtk_main_iteration();

			i++;
		}
	}

	if (data->dialog)
		gtk_widget_destroy (data->dialog);

	g_object_unref (data->gui);
	g_free (data);
	g_free (full_name);
}


void exec_script0 (GtkAction *action, GthWindow *window) {
	GList *list = gth_window_get_file_list_selection (window);
        if (list != NULL) {
        	exec_shell_script ( GTK_WINDOW (window), 
				    eel_gconf_get_string (PREF_HOTKEY0, NULL), 
				    eel_gconf_get_string (PREF_HOTKEY0_NAME, NULL),
				    list);
                path_list_free (list);
	}
}

void exec_script1 (GtkAction *action, GthWindow *window) {
        GList *list = gth_window_get_file_list_selection (window);
        if (list != NULL) {
                exec_shell_script ( GTK_WINDOW (window), 
                                    eel_gconf_get_string (PREF_HOTKEY1, NULL), 
                                    eel_gconf_get_string (PREF_HOTKEY1_NAME, NULL),
                                    list);
                path_list_free (list);
        }
}

void exec_script2 (GtkAction *action, GthWindow *window) {
        GList *list = gth_window_get_file_list_selection (window);
        if (list != NULL) {
                exec_shell_script ( GTK_WINDOW (window), 
                                    eel_gconf_get_string (PREF_HOTKEY2, NULL), 
                                    eel_gconf_get_string (PREF_HOTKEY2_NAME, NULL),
                                    list);
                path_list_free (list);
        }
}

void exec_script3 (GtkAction *action, GthWindow *window) {
        GList *list = gth_window_get_file_list_selection (window);
        if (list != NULL) {
                exec_shell_script ( GTK_WINDOW (window), 
                                    eel_gconf_get_string (PREF_HOTKEY3, NULL), 
                                    eel_gconf_get_string (PREF_HOTKEY3_NAME, NULL),
                                    list);
                path_list_free (list);
        }
}

void exec_script4 (GtkAction *action, GthWindow *window) {
        GList *list = gth_window_get_file_list_selection (window);
        if (list != NULL) {
                exec_shell_script ( GTK_WINDOW (window), 
                                    eel_gconf_get_string (PREF_HOTKEY4, NULL), 
                                    eel_gconf_get_string (PREF_HOTKEY4_NAME, NULL),
                                    list);
                path_list_free (list);
        }
}

void exec_script5 (GtkAction *action, GthWindow *window) {
        GList *list = gth_window_get_file_list_selection (window);
        if (list != NULL) {
                exec_shell_script ( GTK_WINDOW (window), 
                                    eel_gconf_get_string (PREF_HOTKEY5, NULL), 
                                    eel_gconf_get_string (PREF_HOTKEY5_NAME, NULL),
                                    list);
                path_list_free (list);
        }
}

void exec_script6 (GtkAction *action, GthWindow *window) {
        GList *list = gth_window_get_file_list_selection (window);
        if (list != NULL) {
                exec_shell_script ( GTK_WINDOW (window), 
                                    eel_gconf_get_string (PREF_HOTKEY6, NULL), 
                                    eel_gconf_get_string (PREF_HOTKEY6_NAME, NULL),
                                    list);
                path_list_free (list);
        }
}

void exec_script7 (GtkAction *action, GthWindow *window) {
        GList *list = gth_window_get_file_list_selection (window);
        if (list != NULL) {
                exec_shell_script ( GTK_WINDOW (window), 
                                    eel_gconf_get_string (PREF_HOTKEY7, NULL), 
                                    eel_gconf_get_string (PREF_HOTKEY7_NAME, NULL),
                                    list);
                path_list_free (list);
        }
}

void exec_script8 (GtkAction *action, GthWindow *window) {
        GList *list = gth_window_get_file_list_selection (window);
        if (list != NULL) {
                exec_shell_script ( GTK_WINDOW (window), 
                                    eel_gconf_get_string (PREF_HOTKEY8, NULL), 
                                    eel_gconf_get_string (PREF_HOTKEY8_NAME, NULL),
                                    list);
                path_list_free (list);
        }
}

void exec_script9 (GtkAction *action, GthWindow *window) {
        GList *list = gth_window_get_file_list_selection (window);
        if (list != NULL) {
                exec_shell_script ( GTK_WINDOW (window), 
                                    eel_gconf_get_string (PREF_HOTKEY9, NULL), 
                                    eel_gconf_get_string (PREF_HOTKEY9_NAME, NULL),
                                    list);
                path_list_free (list);
        }
}

void exec_upload_flickr (GtkAction *action, GthWindow *window) {
        GList *list = gth_window_get_file_list_selection (window);
        if (gnome_vfs_is_executable_command_string("postr")) {
                exec_shell_script ( GTK_WINDOW (window), 
                                    "postr %F &", 
                                    "Upload to Flickr",
                                    list);
                path_list_free (list);
        }
        else
        	_gtk_error_dialog_run (GTK_WINDOW (window), _("The \"Postr\" software package must be installed to use this function."));
}

static void
add_scripts (void)
{
        ScriptStruct new_entry;

        g_return_if_fail (script_array != NULL);

        new_entry.number = 0;
        new_entry.short_name = eel_gconf_get_string (PREF_HOTKEY0_NAME, _("Script 0"));
        new_entry.script_text = eel_gconf_get_string (PREF_HOTKEY0, "gimp-remote %F");
        g_array_append_vals (script_array, &new_entry, 1);

        new_entry.number = 1;
        new_entry.short_name = eel_gconf_get_string (PREF_HOTKEY1_NAME, _("Script 1"));
        new_entry.script_text = eel_gconf_get_string (PREF_HOTKEY1, "convert %f -font Helvetica -pointsize 20 -fill white  -box '#00000080'  -gravity South -annotate +0+5 ' Copyright 2007, Your Name Here ' %n-copyright%e");
        g_array_append_vals (script_array, &new_entry, 1);

        new_entry.number = 2;
        new_entry.short_name = eel_gconf_get_string (PREF_HOTKEY2_NAME, _("Script 2"));
        new_entry.script_text = eel_gconf_get_string (PREF_HOTKEY2, "mkdir -p %p/approved ; cp %f %p/approved/");
        g_array_append_vals (script_array, &new_entry, 1);

        new_entry.number = 3;
        new_entry.short_name = eel_gconf_get_string (PREF_HOTKEY3_NAME, _("Script 3"));
        new_entry.script_text = eel_gconf_get_string (PREF_HOTKEY3, "uuencode %f %f | mail -s Photos your@emailaddress.com");
        g_array_append_vals (script_array, &new_entry, 1);

        new_entry.number = 4;
        new_entry.short_name = eel_gconf_get_string (PREF_HOTKEY4_NAME, _("Script 4"));
        new_entry.script_text = eel_gconf_get_string (PREF_HOTKEY4, "rm ~/myarchive.zip; zip -j ~/myarchive %F");
        g_array_append_vals (script_array, &new_entry, 1);

        new_entry.number = 5;
        new_entry.short_name = eel_gconf_get_string (PREF_HOTKEY5_NAME, _("Script 5"));
        new_entry.script_text = eel_gconf_get_string (PREF_HOTKEY5, "rm ~/myarchive.zip; zip -j ~/myarchive %F; uuencode ~/myarchive.zip ~/myarchive.zip | mail -s Photos your@emailaddress.com");
        g_array_append_vals (script_array, &new_entry, 1);

        new_entry.number = 6;
        new_entry.short_name = eel_gconf_get_string (PREF_HOTKEY6_NAME, _("Script 6"));
        new_entry.script_text = eel_gconf_get_string (PREF_HOTKEY6, "");
        g_array_append_vals (script_array, &new_entry, 1);

        new_entry.number = 7;
        new_entry.short_name = eel_gconf_get_string (PREF_HOTKEY7_NAME, _("Script 7"));
        new_entry.script_text = eel_gconf_get_string (PREF_HOTKEY7, "");
        g_array_append_vals (script_array, &new_entry, 1);

        new_entry.number = 8;
        new_entry.short_name = eel_gconf_get_string (PREF_HOTKEY8_NAME, _("Script 8"));
        new_entry.script_text = eel_gconf_get_string (PREF_HOTKEY8, "");
        g_array_append_vals (script_array, &new_entry, 1);

        new_entry.number = 9;
        new_entry.short_name = eel_gconf_get_string (PREF_HOTKEY9_NAME, _("Script 9"));
        new_entry.script_text = eel_gconf_get_string (PREF_HOTKEY9, "");
        g_array_append_vals (script_array, &new_entry, 1);
}


static GtkTreeModel *
create_script_model (void)
{
        gint i = 0;
        GtkListStore *model;
        GtkTreeIter iter;

        /* create array */
        script_array = g_array_sized_new (FALSE, FALSE, sizeof (ScriptStruct), 1);

        add_scripts ();

        /* create list store */
        model = gtk_list_store_new (NUMBER_OF_COLUMNS, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);

        /* add scripts */
        for (i = 0; i < script_array->len; i++) {
                gtk_list_store_append (model, &iter);

                gtk_list_store_set (model, &iter,
                                    COLUMN_SCRIPT_NUMBER,
                                    g_array_index (script_array, ScriptStruct, i).number,
                                    COLUMN_SHORT_NAME,
                                    g_array_index (script_array, ScriptStruct, i).short_name,
                                    COLUMN_SCRIPT_TEXT,
                                    g_array_index (script_array, ScriptStruct, i).script_text,
                                    -1);
        }

        return GTK_TREE_MODEL (model);
}


static gboolean
separator_row (GtkTreeModel *model,
               GtkTreeIter  *iter,
               gpointer      data)
{
        GtkTreePath *path;
        gint idx;

        path = gtk_tree_model_get_path (model, iter);
        idx = gtk_tree_path_get_indices (path)[0];

        gtk_tree_path_free (path);

        return idx == 5;
}


static void
editing_started (GtkCellRenderer *cell,
                 GtkCellEditable *editable,
                 const gchar     *path,
                 gpointer         data)
{
        gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (editable),
                                              separator_row, NULL, NULL);
}


static void
cell_edited (GtkCellRendererText *cell,
             const gchar         *path_string,
             const gchar         *new_text,
             gpointer             data)
{
        GtkTreeModel *model = (GtkTreeModel *)data;
        GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
        GtkTreeIter iter;

        gint column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));

        gtk_tree_model_get_iter (model, &iter, path);

        switch (column) {

        case COLUMN_SHORT_NAME: {
                gint i;
                gchar *old_text;

                gtk_tree_model_get (model, &iter, column, &old_text, -1);
                g_free (old_text);

                i = gtk_tree_path_get_indices (path)[0];
                g_free (g_array_index (script_array, ScriptStruct, i).short_name);
                g_array_index (script_array, ScriptStruct, i).short_name = g_strdup (new_text);

                gtk_list_store_set (GTK_LIST_STORE (model), &iter, column,
                                    g_array_index (script_array, ScriptStruct, i).short_name, -1);
                break;
        }

        case COLUMN_SCRIPT_TEXT: {
                gint i;
                gchar *old_text;

                gtk_tree_model_get (model, &iter, column, &old_text, -1);
                g_free (old_text);

                i = gtk_tree_path_get_indices (path)[0];
                g_free (g_array_index (script_array, ScriptStruct, i).script_text);
                g_array_index (script_array, ScriptStruct, i).script_text = g_strdup (new_text);

                gtk_list_store_set (GTK_LIST_STORE (model), &iter, column,
                                    g_array_index (script_array, ScriptStruct, i).script_text, -1);
                break;
        }

        }

        gtk_tree_path_free (path);
}


static void
add_columns (GtkTreeView  *treeview,
             GtkTreeModel *script_model)
{
        GtkCellRenderer *renderer;

        /* number column */
        renderer = gtk_cell_renderer_text_new ();
        g_object_set (renderer, "editable", FALSE, NULL);
        g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), script_model);
        g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (COLUMN_SCRIPT_NUMBER));

        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
                        -1, _("Hot key"), renderer,
                        "text", COLUMN_SCRIPT_NUMBER,
                        NULL);


        /* short_name column */
        renderer = gtk_cell_renderer_text_new ();
        g_object_set (renderer, "editable", TRUE, NULL);
        g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), script_model);
        g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (COLUMN_SHORT_NAME));

        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
                        -1, _("Short Name"), renderer,
                        "text", COLUMN_SHORT_NAME,
                        NULL);

        /* script_text column */
        renderer = gtk_cell_renderer_text_new ();
        g_object_set (renderer, "editable", TRUE, NULL);
        g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), script_model);
        g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (COLUMN_SCRIPT_TEXT));

        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
                        -1, _("Script"), renderer,
                        "text", COLUMN_SCRIPT_TEXT,
                        NULL);
}


/* called when the "help" button is clicked. */
static void
help_cb (GtkWidget  *widget,
         DialogData *data)
{
        gthumb_display_help (GTK_WINDOW (data->dialog), "scripts");
}


/* called when the main dialog is closed. */
static void
destroy_cb (GtkWidget  *widget,
            DialogData *data)
{
	g_object_unref (data->gui);
        g_free (data);
}


/* called when the main dialog is closed. */
static void
cancel_cb (GtkWidget  *widget,
           DialogData *data)
{
	gtk_widget_destroy (data->dialog);
}


/* called when the main dialog is closed. */
static void
save_cb (GtkWidget  *widget,
         DialogData *data)
{
        eel_gconf_set_string (PREF_HOTKEY0, g_array_index (script_array, ScriptStruct, 0).script_text);
        eel_gconf_set_string (PREF_HOTKEY1, g_array_index (script_array, ScriptStruct, 1).script_text);
        eel_gconf_set_string (PREF_HOTKEY2, g_array_index (script_array, ScriptStruct, 2).script_text);
        eel_gconf_set_string (PREF_HOTKEY3, g_array_index (script_array, ScriptStruct, 3).script_text);
        eel_gconf_set_string (PREF_HOTKEY4, g_array_index (script_array, ScriptStruct, 4).script_text);
        eel_gconf_set_string (PREF_HOTKEY5, g_array_index (script_array, ScriptStruct, 5).script_text);
        eel_gconf_set_string (PREF_HOTKEY6, g_array_index (script_array, ScriptStruct, 6).script_text);
        eel_gconf_set_string (PREF_HOTKEY7, g_array_index (script_array, ScriptStruct, 7).script_text);
        eel_gconf_set_string (PREF_HOTKEY8, g_array_index (script_array, ScriptStruct, 8).script_text);
        eel_gconf_set_string (PREF_HOTKEY9, g_array_index (script_array, ScriptStruct, 9).script_text);

        eel_gconf_set_string (PREF_HOTKEY0_NAME, g_array_index (script_array, ScriptStruct, 0).short_name);
        eel_gconf_set_string (PREF_HOTKEY1_NAME, g_array_index (script_array, ScriptStruct, 1).short_name);
        eel_gconf_set_string (PREF_HOTKEY2_NAME, g_array_index (script_array, ScriptStruct, 2).short_name);
        eel_gconf_set_string (PREF_HOTKEY3_NAME, g_array_index (script_array, ScriptStruct, 3).short_name);
        eel_gconf_set_string (PREF_HOTKEY4_NAME, g_array_index (script_array, ScriptStruct, 4).short_name);
        eel_gconf_set_string (PREF_HOTKEY5_NAME, g_array_index (script_array, ScriptStruct, 5).short_name);
        eel_gconf_set_string (PREF_HOTKEY6_NAME, g_array_index (script_array, ScriptStruct, 6).short_name);
        eel_gconf_set_string (PREF_HOTKEY7_NAME, g_array_index (script_array, ScriptStruct, 7).short_name);
        eel_gconf_set_string (PREF_HOTKEY8_NAME, g_array_index (script_array, ScriptStruct, 8).short_name);
        eel_gconf_set_string (PREF_HOTKEY9_NAME, g_array_index (script_array, ScriptStruct, 9).short_name);

	data->done_func (data->done_data);
	gtk_widget_destroy (data->dialog);

}


void
dlg_scripts (GthWindow *window, DoneFunc done_func, gpointer done_data)
{
	DialogData   *data;
        GtkWidget    *script_help_button;
        GtkWidget    *script_cancel_button;
        GtkWidget    *script_save_button;
 	GtkWidget    *vbox_tree;
        GtkWidget    *sw;
        GtkWidget    *treeview;
        GtkTreeModel *script_model;
        int	      width, height;

        /* create window, etc */

        data = g_new0 (DialogData, 1);

        data->window = window;
	data->done_func = done_func;
	data->done_data = done_data;
        data->gui = glade_xml_new (GTHUMB_GLADEDIR "/" SCRIPT_GLADE_FILE, NULL,
                                   NULL);

        if (! data->gui) {
                g_warning ("Could not find " SCRIPT_GLADE_FILE "\n");
                g_free (data);
                return;
        }

        data->dialog = glade_xml_get_widget (data->gui, "scripts_dialog");
        script_help_button = glade_xml_get_widget (data->gui, "script_help_button");
        script_cancel_button = glade_xml_get_widget (data->gui, "script_cancel_button");
        script_save_button = glade_xml_get_widget (data->gui, "script_save_button");
	vbox_tree = glade_xml_get_widget (data->gui, "vbox_tree");

        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                             GTK_SHADOW_ETCHED_IN);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        gtk_box_pack_start (GTK_BOX (vbox_tree), sw, TRUE, TRUE, 0);

        /* create models */
        script_model = create_script_model ();

        /* create tree view */
        treeview = gtk_tree_view_new_with_model (script_model);
        gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
        gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
                                     GTK_SELECTION_SINGLE);

        add_columns (GTK_TREE_VIEW (treeview), script_model);

        g_object_unref (script_model);

        gtk_container_add (GTK_CONTAINER (sw), treeview);


        g_signal_connect (G_OBJECT (data->dialog),
                          "destroy",
                          G_CALLBACK (destroy_cb),
                          data);

        g_signal_connect (G_OBJECT (script_cancel_button),
                          "clicked",
                          G_CALLBACK (cancel_cb),
                          data);

        g_signal_connect (G_OBJECT (script_help_button),
                          "clicked",
                          G_CALLBACK (help_cb),
                          data);

        g_signal_connect (G_OBJECT (script_save_button),
                          "clicked",
                          G_CALLBACK (save_cb),
                          data);


        get_screen_size (GTK_WINDOW (window), &width, &height);
        gtk_window_set_default_size (GTK_WINDOW (data->dialog),
                                     width * 8 / 10,
                                     height * 7 / 10);

        gtk_window_set_transient_for (GTK_WINDOW (data->dialog),
                                      GTK_WINDOW (window));
        gtk_window_set_modal (GTK_WINDOW (data->dialog), TRUE);
        gtk_widget_show_all (data->dialog);
}


static void add_menu_item_and_action (GtkUIManager   *ui, 
   				      GtkActionGroup *action_group,
				      GthWindow      *window,
				      guint           merge_id,
				      int	      hotkey,
				      char           *pref_string, 
				      GCallback       callback_func)
{
	GtkAction *action;
	char      *full_label;
	char	  *label_from_pref;
	char	  *default_label;
	char      *name;

	name = g_strdup_printf ("Script_%d",hotkey);

	default_label = g_strdup_printf (_("Script %d"), hotkey);
	label_from_pref = eel_gconf_get_string (pref_string, default_label);
	full_label = g_strdup_printf ("%d: %s", hotkey, label_from_pref);

	action = g_object_new (GTK_TYPE_ACTION,
      			       "name", name,
			       "label", full_label,
			       "stock_id", GTK_STOCK_EXECUTE,
			       NULL);
        g_signal_connect (action, "activate",
                          G_CALLBACK (callback_func),
                          window);
	gtk_action_group_add_action (action_group, action);
	g_object_unref (action);	
	gtk_ui_manager_add_ui (ui, 
			       merge_id,
			       "/MenuBar/Scripts/User_Defined_Scripts",
			       name,
			       name, 
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	g_free (full_label);
	g_free (label_from_pref);
	g_free (default_label);
	g_free (name);
}


guint
generate_script_menu (GtkUIManager   *ui,
		      GtkActionGroup *action_group,
		      GthWindow      *window,
		      guint	      merge_id)
{
	/* Remove the previously-defined menu items and their associated actions */
        if (merge_id != 0) {
		int i;	
		char *action_name;

		for (i = 0; i < MAX_SCRIPTS; i++) {
			action_name = g_strdup_printf ("Script_%d", i);
			gtk_action_group_remove_action (action_group, 
				gtk_action_group_get_action (action_group, action_name));
			g_free (action_name);
		}

	        gtk_ui_manager_remove_ui (ui, merge_id);
	}

	/* Identify this batch of menu additions (for later removal, if required) */
	merge_id = gtk_ui_manager_new_merge_id (ui);                

	add_menu_item_and_action (ui, action_group, window, merge_id, 
				  0,
                                  PREF_HOTKEY0_NAME,
                                  G_CALLBACK (exec_script0));
	add_menu_item_and_action (ui, action_group, window, merge_id,
                                  1,
                                  PREF_HOTKEY1_NAME,
                                  G_CALLBACK (exec_script1));
	add_menu_item_and_action (ui, action_group, window, merge_id,
				  2,
                                  PREF_HOTKEY2_NAME,
                                  G_CALLBACK (exec_script2));
	add_menu_item_and_action (ui, action_group, window, merge_id,
				  3,
                                  PREF_HOTKEY3_NAME,
                                  G_CALLBACK (exec_script3));
	add_menu_item_and_action (ui, action_group, window, merge_id,
				  4,
                                  PREF_HOTKEY4_NAME,
                                  G_CALLBACK (exec_script4));
	add_menu_item_and_action (ui, action_group, window, merge_id,
				  5,
                                  PREF_HOTKEY5_NAME,
                                  G_CALLBACK (exec_script5));
	add_menu_item_and_action (ui, action_group, window, merge_id,
				  6,
                                  PREF_HOTKEY6_NAME,
                                  G_CALLBACK (exec_script6));
	add_menu_item_and_action (ui, action_group, window, merge_id,
				  7,
                                  PREF_HOTKEY7_NAME,
                                  G_CALLBACK (exec_script7));
	add_menu_item_and_action (ui, action_group, window, merge_id,
				  8,
                                  PREF_HOTKEY8_NAME,
                                  G_CALLBACK (exec_script8));
	add_menu_item_and_action (ui, action_group, window, merge_id,
				  9,
                                  PREF_HOTKEY9_NAME,
                                  G_CALLBACK (exec_script9));

	return merge_id;
}