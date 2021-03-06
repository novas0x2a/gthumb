/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2010 Free Software Foundation, Inc.
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
#include <glib/gi18n.h>
#include "flickr-account.h"
#include "flickr-account-manager-dialog.h"


#define GET_WIDGET(x) (_gtk_builder_get_widget (self->priv->builder, (x)))


enum {
	ACCOUNT_DATA_COLUMN,
	ACCOUNT_NAME_COLUMN,
};


static gpointer parent_class = NULL;


struct _FlickrAccountManagerDialogPrivate {
	GtkBuilder *builder;
};


static void
flickr_account_manager_dialog_finalize (GObject *object)
{
	FlickrAccountManagerDialog *self;

	self = FLICKR_ACCOUNT_MANAGER_DIALOG (object);

	_g_object_unref (self->priv->builder);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
flickr_account_manager_dialog_class_init (FlickrAccountManagerDialogClass *klass)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (FlickrAccountManagerDialogPrivate));

	object_class = (GObjectClass*) klass;
	object_class->finalize = flickr_account_manager_dialog_finalize;
}


static void
delete_button_clicked_cb (GtkWidget *button,
		          gpointer   user_data)
{
	FlickrAccountManagerDialog *self = user_data;
	GtkTreeModel               *tree_model;
	GtkTreeIter                 iter;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (GET_WIDGET ("account_treeview"))),
					     &tree_model,
					     &iter))
	{
		gtk_list_store_remove (GTK_LIST_STORE (tree_model), &iter);
	}
}


static void
text_renderer_edited_cb (GtkCellRendererText *renderer,
			 char                *path,
			 char                *new_text,
			 gpointer             user_data)
{
	FlickrAccountManagerDialog *self = user_data;
	GtkTreePath                *tree_path;
	GtkTreeIter                 iter;
	FlickrAccount              *account;

	tree_path = gtk_tree_path_new_from_string (path);
	tree_path = gtk_tree_path_new_from_string (path);
	if (! gtk_tree_model_get_iter (GTK_TREE_MODEL (GET_WIDGET ("accounts_liststore")),
				       &iter,
				       tree_path))
	{
		gtk_tree_path_free (tree_path);
		return;
	}
	gtk_tree_path_free (tree_path);

	gtk_tree_model_get (GTK_TREE_MODEL (GET_WIDGET ("accounts_liststore")), &iter,
			    ACCOUNT_DATA_COLUMN, &account,
			    -1);
	flickr_account_set_username (account, new_text);

	gtk_list_store_set (GTK_LIST_STORE (GET_WIDGET ("accounts_liststore")), &iter,
			    ACCOUNT_DATA_COLUMN, account,
			    ACCOUNT_NAME_COLUMN, new_text,
			    -1);

	g_object_unref (account);
}


static void
flickr_account_manager_dialog_init (FlickrAccountManagerDialog *self)
{
	GtkWidget *content;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, FLICKR_TYPE_ACCOUNT_MANAGER_DIALOG, FlickrAccountManagerDialogPrivate);
	self->priv->builder = _gtk_builder_new_from_file ("flicker-account-manager.ui", "flicker");

	gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
	gtk_dialog_set_has_separator (GTK_DIALOG (self), FALSE);
	gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))), 5);
	gtk_container_set_border_width (GTK_CONTAINER (self), 5);

	content = _gtk_builder_get_widget (self->priv->builder, "account_manager");
	gtk_container_set_border_width (GTK_CONTAINER (content), 5);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))), content, TRUE, TRUE, 0);

	gtk_dialog_add_button (GTK_DIALOG (self),
			       GTK_STOCK_NEW,
			       FLICKR_ACCOUNT_MANAGER_RESPONSE_NEW);
	gtk_dialog_add_button (GTK_DIALOG (self),
			       GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (self),
			       GTK_STOCK_OK,
			       GTK_RESPONSE_OK);

	g_object_set (GET_WIDGET ("account_cellrenderertext"), "editable", TRUE, NULL);
        g_signal_connect (GET_WIDGET ("account_cellrenderertext"),
                          "edited",
                          G_CALLBACK (text_renderer_edited_cb),
                          self);

	g_signal_connect (GET_WIDGET ("delete_button"),
			  "clicked",
			  G_CALLBACK (delete_button_clicked_cb),
			  self);
}


GType
flickr_account_manager_dialog_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo g_define_type_info = {
			sizeof (FlickrAccountManagerDialogClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) flickr_account_manager_dialog_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (FlickrAccountManagerDialog),
			0,
			(GInstanceInitFunc) flickr_account_manager_dialog_init,
			NULL
		};
		type = g_type_register_static (GTK_TYPE_DIALOG,
					       "FlickrAccountManagerDialog",
					       &g_define_type_info,
					       0);
	}

	return type;
}


static void
flickr_account_manager_dialog_construct (FlickrAccountManagerDialog *self,
				         GList                      *accounts)
{
	GtkListStore *list_store;
	GtkTreeIter   iter;
	GList        *scan;

	list_store = GTK_LIST_STORE (GET_WIDGET ("accounts_liststore"));
	gtk_list_store_clear (list_store);
	for (scan = accounts; scan; scan = scan->next) {
		FlickrAccount *account = scan->data;

		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter,
				    ACCOUNT_DATA_COLUMN, account,
				    ACCOUNT_NAME_COLUMN, account->username,
				    -1);
	}
}


GtkWidget *
flickr_account_manager_dialog_new (GList *accounts)
{
	FlickrAccountManagerDialog *self;

	self = g_object_new (FLICKR_TYPE_ACCOUNT_MANAGER_DIALOG, NULL);
	flickr_account_manager_dialog_construct (self, accounts);

	return (GtkWidget *) self;
}


GList *
flickr_account_manager_dialog_get_accounts (FlickrAccountManagerDialog *self)
{
	GList        *accounts;
	GtkTreeModel *tree_model;
	GtkTreeIter   iter;

	tree_model = (GtkTreeModel *) GET_WIDGET ("accounts_liststore");
	if (! gtk_tree_model_get_iter_first (tree_model, &iter))
		return NULL;

	accounts = NULL;
	do {
		FlickrAccount *account;

		gtk_tree_model_get (tree_model, &iter,
				    ACCOUNT_DATA_COLUMN, &account,
				    -1);
		accounts = g_list_prepend (accounts, account);
	}
	while (gtk_tree_model_iter_next (tree_model, &iter));

	return g_list_reverse (accounts);
}
