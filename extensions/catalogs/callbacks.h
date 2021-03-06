/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2009 Free Software Foundation, Inc.
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

#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <gthumb.h>
#include "gth-catalog.h"

void catalogs__initialize_cb                              (void);
void catalogs__gth_browser_construct_cb                   (GthBrowser    *browser);
void catalogs__gth_browser_update_sensitivity_cb          (GthBrowser    *browser);
void catalogs__gth_browser_file_list_popup_before_cb      (GthBrowser    *browser);
void catalogs__gth_browser_file_popup_before_cb           (GthBrowser    *browser);
void catalogs__gth_browser_folder_tree_popup_before_cb    (GthBrowser    *browser,
							   GthFileSource *file_source,
							   GFile         *folder);
GFile *      catalogs__command_line_files_cb              (GList         *files);
GthCatalog * catalogs__gth_catalog_load_from_data_cb      (const void    *buffer);
void         catalogs__gth_browser_load_location_after_cb (GthBrowser    *browser,
					                   GFile         *location,
					                   const GError  *error);
void         catalogs__gth_browser_update_extra_widget_cb (GthBrowser    *browser);

#endif /* CALLBACKS_H */
