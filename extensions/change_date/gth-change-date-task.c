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
#include "gth-change-date-task.h"


#define HOURS_TO_SECONDS(h) ((h) * 3600)


struct _GthChangeDateTaskPrivate {
	GList           *files; /* GFile */
	GList           *file_list; /* GthFileData */
	GthChangeFields  fields;
	GthChangeType    change_type;
	GthDateTime     *date_time;
	int              timezone_offset;
	int              n_files;
	int              n_current;
	GList           *current;
};


static gpointer parent_class = NULL;


static void
gth_change_date_task_finalize (GObject *object)
{
	GthChangeDateTask *self;

	self = GTH_CHANGE_DATE_TASK (object);

	_g_object_list_unref (self->priv->file_list);
	_g_object_list_unref (self->priv->files);
	gth_datetime_free (self->priv->date_time);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
set_date_time_from_change_type (GthChangeDateTask *self,
				GthDateTime       *date_time,
				GthChangeType      change_type,
				GthFileData       *file_data)
{
	if (change_type == GTH_CHANGE_TO_FOLLOWING_DATE) {
		gth_datetime_copy (self->priv->date_time, date_time);
	}
	else if (change_type == GTH_CHANGE_TO_FILE_MODIFIED_DATE) {
		gth_datetime_from_timeval (date_time, gth_file_data_get_modification_time (file_data));
	}
	else if (change_type == GTH_CHANGE_TO_FILE_CREATION_DATE) {
		gth_datetime_from_timeval (date_time, gth_file_data_get_creation_time (file_data));
	}
	else if (change_type == GTH_CHANGE_TO_PHOTO_ORIGINAL_DATE) {
		GTimeVal time_val;

		if (gth_file_data_get_digitalization_time (file_data, &time_val))
			gth_datetime_from_timeval (date_time, &time_val);
	}
}


static void
update_modification_time (GthChangeDateTask *self)
{
	GthDateTime *date_time;
	GList       *scan;
	GError      *error = NULL;

	if ((self->priv->fields & GTH_CHANGE_LAST_MODIFIED_DATE) == 0) {
		gth_task_completed (GTH_TASK (self), NULL);
		return;
	}

	date_time = gth_datetime_new ();
	for (scan = self->priv->file_list; scan; scan = scan->next) {
		GthFileData *file_data = scan->data;
		GTimeVal     timeval;

		gth_datetime_clear (date_time);
		if (self->priv->change_type == GTH_CHANGE_ADJUST_TIMEZONE)
			set_date_time_from_change_type (self, date_time, GTH_CHANGE_TO_FILE_MODIFIED_DATE, file_data);
		else
			set_date_time_from_change_type (self, date_time, self->priv->change_type, file_data);
		if (! gth_datetime_valid (date_time))
			continue;

		if (gth_datetime_to_timeval (date_time, &timeval)) {
			if (self->priv->change_type == GTH_CHANGE_ADJUST_TIMEZONE)
				timeval.tv_sec += HOURS_TO_SECONDS (self->priv->timezone_offset);
			if (! _g_file_set_modification_time (file_data->file,
							     &timeval,
							     gth_task_get_cancellable (GTH_TASK (self)),
							     &error))
			{
				break;
			}
		}
	}
	gth_task_completed (GTH_TASK (self), error);

	gth_datetime_free (date_time);
}


static void
write_metadata_reasy_cb (GError   *error,
			 gpointer  user_data)
{
	GthChangeDateTask *self = user_data;

	if (error != NULL) {
		gth_task_completed (GTH_TASK (self), error);
		return;
	}

	if (g_cancellable_set_error_if_cancelled (gth_task_get_cancellable (GTH_TASK (self)), &error)) {
		gth_task_completed (GTH_TASK (self), error);
		return;
	}

	update_modification_time (self);
}


static void
set_date_metadata (GthFileData *file_data,
		   const char  *attribute,
		   GthDateTime *date_time,
		   int          timezone_offset)
{
	char    *raw;
	char    *formatted;
	GObject *metadata;

	if (timezone_offset != 0) {
		GTimeVal     timeval;
		GthDateTime *adjusted_date;

		gth_datetime_to_timeval (date_time, &timeval);
		timeval.tv_sec += HOURS_TO_SECONDS (timezone_offset);
		adjusted_date = gth_datetime_new ();
		gth_datetime_from_timeval (adjusted_date, &timeval);
		raw = gth_datetime_to_exif_date (adjusted_date);
		formatted = gth_datetime_strftime (adjusted_date, "%x");

		gth_datetime_free (adjusted_date);
	}
	else {
		raw = gth_datetime_to_exif_date (date_time);
		formatted = gth_datetime_strftime (date_time, "%x");
	}

	metadata = (GObject *) gth_metadata_new ();
	g_object_set (metadata,
		      "id", attribute,
		      "raw", raw,
		      "formatted", formatted,
		      NULL);
	g_file_info_set_attribute_object (file_data->info, attribute, metadata);

	g_object_unref (metadata);
	g_free (formatted);
	g_free (raw);
}


static void
set_date_time_from_field (GthChangeDateTask *self,
			  GthDateTime       *date_time,
			  GthChangeFields    field,
			  GthFileData       *file_data)
{
	if (field & GTH_CHANGE_LAST_MODIFIED_DATE) {
		gth_datetime_from_timeval (date_time, gth_file_data_get_modification_time (file_data));
	}
	else if (field & GTH_CHANGE_COMMENT_DATE) {
		GthMetadata *m;
		GTimeVal     time_val;

		m = (GthMetadata *) g_file_info_get_attribute_object (file_data->info, "general::datetime");
		if ((m != NULL) && _g_time_val_from_exif_date (gth_metadata_get_raw (m), &time_val))
			gth_datetime_from_timeval (date_time, &time_val);
	}
	else if (field & GTH_CHANGE_EXIF_DATETIMEORIGINAL_TAG) {
		GthMetadata *m;
		GTimeVal     time_val;

		m = (GthMetadata *) g_file_info_get_attribute_object (file_data->info, "Exif::Photo::DateTimeOriginal");
		if ((m != NULL) && _g_time_val_from_exif_date (gth_metadata_get_raw (m), &time_val))
			gth_datetime_from_timeval (date_time, &time_val);
	}
}


static void
info_ready_cb (GList    *files,
	       GError   *error,
	       gpointer  user_data)
{
	GthChangeDateTask *self = user_data;
	GthDateTime       *date_time;
	GList             *scan;
	GPtrArray         *attribute_v;

	if (error != NULL) {
		gth_task_completed (GTH_TASK (self), error);
		return;
	}

	if (g_cancellable_set_error_if_cancelled (gth_task_get_cancellable (GTH_TASK (self)), &error)) {
		gth_task_completed (GTH_TASK (self), error);
		return;
	}

	date_time = gth_datetime_new ();
	self->priv->file_list = _g_object_list_ref (files);
	for (scan = self->priv->file_list; scan; scan = scan->next) {
		GthFileData *file_data = scan->data;

		if (self->priv->change_type == GTH_CHANGE_ADJUST_TIMEZONE) {
			if (self->priv->fields & GTH_CHANGE_COMMENT_DATE) {
				gth_datetime_clear (date_time);
				set_date_time_from_field (self, date_time, GTH_CHANGE_COMMENT_DATE, file_data);
				if (gth_datetime_valid (date_time))
					set_date_metadata (file_data, "general::datetime", date_time, self->priv->timezone_offset);
			}
			if (self->priv->fields & GTH_CHANGE_EXIF_DATETIMEORIGINAL_TAG) {
				gth_datetime_clear (date_time);
				set_date_time_from_field (self, date_time, GTH_CHANGE_EXIF_DATETIMEORIGINAL_TAG, file_data);
				if (gth_datetime_valid (date_time))
					set_date_metadata (file_data, "Exif::Photo::DateTimeOriginal", date_time, self->priv->timezone_offset);
			}
		}
		else {
			gth_datetime_clear (date_time);
			set_date_time_from_change_type (self, date_time, self->priv->change_type, file_data);
			if (gth_datetime_valid (date_time)) {
				if (self->priv->fields & GTH_CHANGE_COMMENT_DATE) {
					set_date_metadata (file_data, "general::datetime", date_time, 0);
				}
				if (self->priv->fields & GTH_CHANGE_EXIF_DATETIMEORIGINAL_TAG) {
					set_date_metadata (file_data, "Exif::Photo::DateTimeOriginal", date_time, 0);
				}
			}
		}
	}

	attribute_v = g_ptr_array_new ();
	if (self->priv->fields & GTH_CHANGE_COMMENT_DATE)
		g_ptr_array_add (attribute_v, "general::datetime");
	if (self->priv->fields & GTH_CHANGE_EXIF_DATETIMEORIGINAL_TAG)
		g_ptr_array_add (attribute_v, "Exif::Photo::DateTimeOriginal");
	if (attribute_v->len > 0) {
		char *attributes;

		attributes = _g_string_array_join (attribute_v, ",");
		_g_write_metadata_async (self->priv->file_list,
					 attributes,
					 gth_task_get_cancellable (GTH_TASK (self)),
					 write_metadata_reasy_cb,
					 self);

		g_free (attributes);
	}
	else
		update_modification_time (self);

	g_ptr_array_free (attribute_v, TRUE);
	gth_datetime_free (date_time);
}


static void
gth_change_date_task_exec (GthTask *task)
{
	GthChangeDateTask *self = GTH_CHANGE_DATE_TASK (task);

	_g_query_all_metadata_async (self->priv->files,
				     FALSE,
				     TRUE,
				     "*",
				     gth_task_get_cancellable (task),
				     info_ready_cb,
				     self);
}


static void
gth_change_date_task_class_init (GthChangeDateTaskClass *klass)
{
	GObjectClass *object_class;
	GthTaskClass *task_class;

	parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (GthChangeDateTaskPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gth_change_date_task_finalize;

	task_class = GTH_TASK_CLASS (klass);
	task_class->exec = gth_change_date_task_exec;
}


static void
gth_change_date_task_init (GthChangeDateTask *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GTH_TYPE_CHANGE_DATE_TASK, GthChangeDateTaskPrivate);
	self->priv->date_time = gth_datetime_new ();
}


GType
gth_change_date_task_get_type (void)
{
	static GType type = 0;

	if (! type) {
		GTypeInfo type_info = {
			sizeof (GthChangeDateTaskClass),
			NULL,
			NULL,
			(GClassInitFunc) gth_change_date_task_class_init,
			NULL,
			NULL,
			sizeof (GthChangeDateTask),
			0,
			(GInstanceInitFunc) gth_change_date_task_init
		};

		type = g_type_register_static (GTH_TYPE_TASK,
					       "GthChangeDateTask",
					       &type_info,
					       0);
	}

	return type;
}


GthTask *
gth_change_date_task_new (GList             *files, /* GthFileData */
			  GthChangeFields    fields,
			  GthChangeType      change_type,
			  GthDateTime       *date_time,
			  int                timezone_offset)
{
	GthChangeDateTask *self;

	self = GTH_CHANGE_DATE_TASK (g_object_new (GTH_TYPE_CHANGE_DATE_TASK, NULL));
	self->priv->files = gth_file_data_list_to_file_list (files);
	self->priv->fields = fields;
	self->priv->change_type = change_type;
	if (date_time != NULL)
		gth_datetime_copy (date_time, self->priv->date_time);
	self->priv->timezone_offset = timezone_offset;

	return (GthTask *) self;
}
