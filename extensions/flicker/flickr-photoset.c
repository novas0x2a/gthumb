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
#include <stdlib.h>
#include <string.h>
#include <gthumb.h>
#include "flickr-photoset.h"


static gpointer flickr_photoset_parent_class = NULL;


static void
flickr_photoset_finalize (GObject *obj)
{
	FlickrPhotoset *self;

	self = FLICKR_PHOTOSET (obj);

	g_free (self->id);
	g_free (self->title);
	g_free (self->description);
	g_free (self->primary);
	g_free (self->secret);
	g_free (self->server);
	g_free (self->farm);

	G_OBJECT_CLASS (flickr_photoset_parent_class)->finalize (obj);
}


static void
flickr_photoset_class_init (FlickrPhotosetClass *klass)
{
	flickr_photoset_parent_class = g_type_class_peek_parent (klass);
	G_OBJECT_CLASS (klass)->finalize = flickr_photoset_finalize;
}


static DomElement*
flickr_photoset_create_element (DomDomizable *base,
				DomDocument  *doc)
{
	FlickrPhotoset *self;
	DomElement     *element;
	char           *value;

	self = FLICKR_PHOTOSET (base);

	element = dom_document_create_element (doc, "photoset", NULL);
	if (self->id != NULL)
		dom_element_set_attribute (element, "id", self->id);
	if (self->primary != NULL)
		dom_element_set_attribute (element, "primary", self->primary);
	if (self->secret != NULL)
		dom_element_set_attribute (element, "secret", self->secret);
	if (self->server != NULL)
		dom_element_set_attribute (element, "server", self->server);
	if (self->n_photos >= 0) {
		value = g_strdup_printf ("%d", self->n_photos);
		dom_element_set_attribute (element, "photos", value);
		g_free (value);
	}
	if (self->farm != NULL)
		dom_element_set_attribute (element, "farm", self->farm);

	if (self->title != NULL)
		dom_element_append_child (element, dom_document_create_element_with_text (doc, self->title, "title", NULL));
	if (self->description != NULL)
		dom_element_append_child (element, dom_document_create_element_with_text (doc, self->description, "description", NULL));

	return element;
}


static void
flickr_photoset_load_from_element (DomDomizable *base,
				   DomElement   *element)
{
	FlickrPhotoset *self;
	DomElement     *node;

	self = FLICKR_PHOTOSET (base);

	flickr_photoset_set_id (self, dom_element_get_attribute (element, "id"));
	flickr_photoset_set_title (self, NULL);
	flickr_photoset_set_description (self, NULL);
	flickr_photoset_set_n_photos (self, dom_element_get_attribute (element, "photos"));
	flickr_photoset_set_primary (self, dom_element_get_attribute (element, "primary"));
	flickr_photoset_set_secret (self, dom_element_get_attribute (element, "secret"));
	flickr_photoset_set_server (self, dom_element_get_attribute (element, "server"));
	flickr_photoset_set_farm (self, dom_element_get_attribute (element, "farm"));
	flickr_photoset_set_url (self, dom_element_get_attribute (element, "url"));

	for (node = element->first_child; node; node = node->next_sibling) {
		if (g_strcmp0 (node->tag_name, "title") == 0) {
			flickr_photoset_set_title (self, dom_element_get_inner_text (node));
		}
		else if (g_strcmp0 (node->tag_name, "description") == 0) {
			flickr_photoset_set_description (self, dom_element_get_inner_text (node));
		}
	}
}


static void
flickr_photoset_dom_domizable_interface_init (DomDomizableIface *iface)
{
	iface->create_element = flickr_photoset_create_element;
	iface->load_from_element = flickr_photoset_load_from_element;
}


static void
flickr_photoset_instance_init (FlickrPhotoset *self)
{
	self->id = NULL;
	self->title = NULL;
	self->description = NULL;
	self->primary = NULL;
	self->secret = NULL;
	self->server = NULL;
	self->farm = NULL;
	self->url = NULL;
}


GType
flickr_photoset_get_type (void)
{
	static GType flickr_photoset_type_id = 0;

	if (flickr_photoset_type_id == 0) {
		static const GTypeInfo g_define_type_info = {
			sizeof (FlickrPhotosetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) flickr_photoset_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (FlickrPhotoset),
			0,
			(GInstanceInitFunc) flickr_photoset_instance_init,
			NULL
		};
		static const GInterfaceInfo dom_domizable_info = {
			(GInterfaceInitFunc) flickr_photoset_dom_domizable_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		flickr_photoset_type_id = g_type_register_static (G_TYPE_OBJECT,
								   "FlickrPhotoset",
								   &g_define_type_info,
								   0);
		g_type_add_interface_static (flickr_photoset_type_id, DOM_TYPE_DOMIZABLE, &dom_domizable_info);
	}

	return flickr_photoset_type_id;
}


FlickrPhotoset *
flickr_photoset_new (void)
{
	return g_object_new (FLICKR_TYPE_PHOTOSET, NULL);
}


void
flickr_photoset_set_id (FlickrPhotoset *self,
			const char     *value)
{
	_g_strset (&self->id, value);
}


void
flickr_photoset_set_title (FlickrPhotoset *self,
			   const char     *value)
{
	_g_strset (&self->title, value);
}


void
flickr_photoset_set_description (FlickrPhotoset *self,
			         const char     *value)
{
	_g_strset (&self->description, value);
}


void
flickr_photoset_set_n_photos (FlickrPhotoset *self,
			      const char     *value)
{
	if (value != NULL)
		self->n_photos = atoi (value);
	else
		self->n_photos = 0;
}


void
flickr_photoset_set_primary (FlickrPhotoset *self,
			     const char     *value)
{
	_g_strset (&self->primary, value);
}


void
flickr_photoset_set_secret (FlickrPhotoset *self,
			    const char     *value)
{
	_g_strset (&self->secret, value);
}


void
flickr_photoset_set_server (FlickrPhotoset *self,
			    const char     *value)
{
	_g_strset (&self->server, value);
}


void
flickr_photoset_set_farm (FlickrPhotoset *self,
			  const char     *value)
{
	_g_strset (&self->farm, value);
}


void
flickr_photoset_set_url (FlickrPhotoset *self,
			 const char     *value)
{
	_g_strset (&self->url, value);
}
