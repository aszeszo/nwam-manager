/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 shiftwidth=4: */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * CDDL HEADER START
 * 
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 * 
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * 
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 * 
 * CDDL HEADER END
 *
 * File:   nwam_ui_icons.c
 *
 * Created on May 9, 2007, 11:01 AM
 *
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include <config.h>
#include "nwam_ui_icons.h"

/* FIXME, will get each file name of icon files in BASE_ICON_PATH as stock-id */
/* So icon file should be named like nwam-stock-wireless-strength-* */
#define BASE_ICON_PATH PACKAGE "/icons" 

static GObjectClass *parent_class = NULL;

struct _NwamUIIconsPrivate
{
        GtkIconFactory* iconf;
        
    /* Other Data */
};

static void nwam_ui_icons_finalize(NwamUIIcons *self);

G_DEFINE_TYPE(NwamUIIcons, nwam_ui_icons, G_TYPE_OBJECT)

static void
nwam_ui_icons_class_init(NwamUIIconsClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
    
    /* Initialise Static Parent Class pointer */
    parent_class = g_type_class_peek_parent(klass);
    
    /* Override Some Function Pointers */
    gobject_class->finalize = (void (*)(GObject*)) nwam_ui_icons_finalize;
}


static void
nwam_ui_icons_load_filename (NwamUIIcons *self, const gchar *filename)
{
    GtkIconSet *iconset;
    GtkIconSource *iconsrc;
    gchar stockid[1024] = {"nwam-"};
    gchar *fn, *dotp;

    /* filename should be accessible, no test now */
    g_assert (filename);
    fn = g_path_get_basename (filename);
    dotp = g_strstr_len (fn, strlen (fn), ".");
    if (dotp)
        *dotp = '\0';
    g_strlcat (stockid, fn, sizeof (stockid));
    iconset = gtk_icon_set_new ();
    iconsrc = gtk_icon_source_new ();
    gtk_icon_source_set_filename (iconsrc, filename);

    gtk_icon_source_set_direction_wildcarded (iconsrc, TRUE);
    gtk_icon_source_set_size_wildcarded (iconsrc, TRUE);
    gtk_icon_source_set_state_wildcarded (iconsrc, TRUE);
    
    gtk_icon_set_add_source (iconset, iconsrc);
    g_debug("Loading icon: stock-id = '%s' ; file = '%s'", stockid, filename);
    gtk_icon_factory_add (self->prv->iconf, stockid, iconset);
    gtk_icon_source_free (iconsrc);
    gtk_icon_set_unref (iconset);
    g_free (fn);
}

static void
nwam_ui_icons_load (NwamUIIcons *self, const gchar* path)
{
    GDir *dir;
    gchar* icon_path;

    g_assert (path);
    dir = g_dir_open (path, 0, NULL);
    if(dir == NULL)
        return;

    while ((icon_path = (gchar *) g_dir_read_name (dir)) != NULL) {
        gchar *pathname = g_build_filename (path, icon_path, NULL);
        nwam_ui_icons_load_filename (self, pathname);
        g_free (pathname);
    }
    g_dir_close (dir);
}

static void
nwam_ui_icons_init(NwamUIIcons *self)
{
    static const gchar *build_datadir = NWAM_MANAGER_DATADIR;
    const gchar * const *sys_data_dirs;
    gint i;
    
    self->prv = g_new0(NwamUIIconsPrivate, 1);
    self->prv->iconf = gtk_icon_factory_new ();

    gtk_icon_factory_add_default (self->prv->iconf);
    
    if ( g_file_test(build_datadir, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR) ) {
        gchar* data_dir = g_build_filename (build_datadir, BASE_ICON_PATH, NULL);
        nwam_ui_icons_load (self, data_dir );
        g_free(data_dir);
    }
    sys_data_dirs = g_get_system_data_dirs ();
    for (i = 0; sys_data_dirs[i] != NULL; i++) {
        gchar *data_dir;
        data_dir = g_build_filename (sys_data_dirs[i], BASE_ICON_PATH, NULL);
        nwam_ui_icons_load (self, data_dir);
        g_free (data_dir);
    }
}

GdkPixbuf*   
nwam_ui_icons_get_pixbuf( NwamUIIcons* self, const gchar* stock_id )
{
    GtkIconSet* icon_set = gtk_icon_factory_lookup( self->prv->iconf, stock_id );

    return( gtk_icon_set_render_icon( icon_set, gtk_widget_get_default_style(), GTK_TEXT_DIR_NONE, GTK_STATE_NORMAL,
                                      GTK_ICON_SIZE_LARGE_TOOLBAR, NULL, NULL ));
}

/**
 * nwam_ui_icons_new:
 * @returns: a new #NwamUIIcons.
 *
 * Creates a new #NwamUIIcons with an empty ncu
 **/
NwamUIIcons*
nwam_ui_icons_get_instance(void)
{
    static NwamUIIcons* instance = NULL;

    if ( instance == NULL ) {
        instance = NWAM_UI_ICONS(g_object_new(NWAM_TYPE_UI_ICONS, NULL));
    }
    return instance;
}

static void
nwam_ui_icons_finalize(NwamUIIcons *self)
{
    gtk_icon_factory_remove_default (self->prv->iconf);
    g_object_unref (G_OBJECT (self->prv->iconf));
    g_free(self->prv);
    self->prv = NULL;
    
    (*G_OBJECT_CLASS(parent_class)->finalize) (G_OBJECT(self));
}
