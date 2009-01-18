#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>
#include <strings.h>

#include "libnwamui.h"
#include "nwam_pref_iface.h"
#include "nwam_condition_vbox.h"

#define	_CU_COND_FILTER_VALUE	"custom_cond_combo_filter_value"
#define TABLE_PREF_PARENT "table_pref_parent"
#define TABLE_ROW_COMBO1 "condition_vbox_row_combo1"
#define TABLE_ROW_COMBO2 "condition_vbox_row_combo2"
#define TABLE_ROW_ENTRY "condition_vbox_row_entry"
#define TABLE_ROW_ADD "condition_vbox_row_add"
#define TABLE_ROW_REMOVE "condition_vbox_row_remove"
#define TABLE_ROW_CDATA "condition_vbox_row_cdata"

enum {
    S_CONDITION_ADD,
    S_CONDITION_REMOVE,
    LAST_SIGNAL
};

static guint cond_signals[LAST_SIGNAL] = {0};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	NWAM_TYPE_CONDITION_VBOX, NwamConditionVBoxPrivate)) 

struct _NwamConditionVBoxPrivate {

    /* Widget Pointers */
    GList*                      table_box_cache;
    guint                       table_line_num;

    NwamuiObject*                  selected_object;
};

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);
static gboolean refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force);
static gboolean apply(NwamPrefIFace *iface, gpointer user_data);
static gboolean cancel(NwamPrefIFace *iface, gpointer user_data);

static void nwam_condition_vbox_finalize (NwamConditionVBox *self);
static void table_lines_init (NwamConditionVBox *self, NwamuiObject *object);
static void table_lines_free (NwamConditionVBox *self);
static GtkWidget *table_conditon_new (NwamConditionVBox *self, NwamuiCond* cond);
static GtkWidget* table_condition_insert (GtkWidget *clist, guint row, NwamuiCond* cond);
static void table_condition_delete(GtkWidget *clist, GtkWidget *crow);

static void table_add_condition_cb(GtkButton *button, gpointer  data);
static void table_delete_condition_cb(GtkButton *button, gpointer  data);
static void table_line_cache_all_cb (GtkWidget *widget, gpointer data);

static void condition_field_op_changed_cb( GtkWidget* widget, gpointer data );
static void condition_value_changed_cb( GtkWidget* widget, gpointer data );

static void default_condition_add_signal_handler (NwamConditionVBox *self, GObject* data, gpointer user_data);
static void default_condition_remove_signal_handler (NwamConditionVBox *self, GObject* data, gpointer user_data);


G_DEFINE_TYPE_EXTENDED (NwamConditionVBox,
    nwam_condition_vbox,
    GTK_TYPE_VBOX,
    0,
    G_IMPLEMENT_INTERFACE (NWAM_TYPE_PREF_IFACE, nwam_pref_init))

static gboolean
refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force)
{
	NwamConditionVBox *self = NWAM_CONDITION_VBOX(iface);
	NwamConditionVBoxPrivate *prv = GET_PRIVATE(iface);

	if (prv->selected_object != user_data) {
		if (prv->selected_object) {
			g_object_unref(prv->selected_object);
		}
		if ((prv->selected_object = user_data) != NULL) {
			g_object_ref(prv->selected_object);
		}
		force = TRUE;
	}

	if (force) {
		table_lines_free(self);
		if (prv->selected_object) {
			table_lines_init (self, NWAMUI_OBJECT(user_data));
		}
	}

	return TRUE;
}

static gboolean
apply(NwamPrefIFace *iface, gpointer user_data)
{
	NwamConditionVBoxPrivate *prv = GET_PRIVATE(iface);
	nwamui_object_set_conditions(NWAMUI_OBJECT(prv->selected_object), NULL);
}

static gboolean
cancel(NwamPrefIFace *iface, gpointer user_data)
{
	NwamConditionVBoxPrivate *prv = GET_PRIVATE(iface);
	NwamConditionVBox *self = NWAM_CONDITION_VBOX(iface);

/* 	nwamui_object_get_conditions(NWAMUI_OBJECT(prv->selected_object)); */
	nwam_pref_refresh(NWAM_PREF_IFACE(self), user_data, TRUE);
}

static void
nwam_pref_init (gpointer g_iface, gpointer iface_data)
{
	NwamPrefInterface *iface = (NwamPrefInterface *)g_iface;
	iface->refresh = refresh;
	iface->apply = apply;
	iface->help = NULL;
	iface->dialog_run = NULL;
}

static void
nwam_condition_vbox_class_init (NwamConditionVBoxClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    /* Override Some Function Pointers */
    gobject_class->finalize = (void (*)(GObject*)) nwam_condition_vbox_finalize;

    g_type_class_add_private (klass, sizeof (NwamConditionVBoxPrivate));

    /* Create some signals */
    cond_signals[S_CONDITION_ADD] =   
            g_signal_new ("condition_add",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (NwamConditionVBoxClass, condition_add),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER, 
                  G_TYPE_NONE,                  /* Return Type */
                  1,                            /* Number of Args */
                  G_TYPE_POINTER);               /* Types of Args */
    
    cond_signals[S_CONDITION_REMOVE] =   
            g_signal_new ("condition_remove",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (NwamConditionVBoxClass, condition_remove),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER, 
                  G_TYPE_NONE,                  /* Return Type */
                  1,                            /* Number of Args */
                  G_TYPE_POINTER);               /* Types of Args */

    klass->condition_add = default_condition_add_signal_handler;
    klass->condition_remove = default_condition_remove_signal_handler;
}
        
static void
nwam_condition_vbox_init (NwamConditionVBox *self)
{
}

static void
nwam_condition_vbox_finalize (NwamConditionVBox *self)
{
	NwamConditionVBoxPrivate *prv = GET_PRIVATE(self);

	table_lines_free (self);

	if (prv->selected_object) {
		g_object_unref(prv->selected_object);
	}

	G_OBJECT_CLASS(nwam_condition_vbox_parent_class)->finalize(G_OBJECT(self));
}

static void
_cu_cond_combo_filter_value (GtkWidget *combo, gint value)
{
    g_object_set_data (G_OBJECT(combo),
      _CU_COND_FILTER_VALUE,
      (gpointer)value);
    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER(gtk_combo_box_get_model(GTK_COMBO_BOX(combo))));
}

static void
_cu_cond_combo_cell_cb (GtkCellLayout *cell_layout,
  GtkCellRenderer   *renderer,
  GtkTreeModel      *model,
  GtkTreeIter       *iter,
  gpointer           data)
{
    GtkTreeModel *realmodel;
    GtkTreeModelFilter *filter;
    GtkTreeIter realiter;
    const gchar *str;

    gint row_data = 0;

    filter = GTK_TREE_MODEL_FILTER(model);
    realmodel = gtk_tree_model_filter_get_model (filter);
    gtk_tree_model_filter_convert_iter_to_child_iter (filter,
      &realiter, iter);
	gtk_tree_model_get(realmodel, &realiter, 0, &row_data, -1);

	if (realmodel == (gpointer)nwamui_env_get_condition_subject ()) {
        if (row_data != NWAMUI_COND_FIELD_LAST) {
            str = nwamui_cond_field_to_str ((nwamui_cond_field_t)row_data);
        } else {
            str = _("< No conditions >");
        }
    } else {
        str = nwamui_cond_op_to_str ((nwamui_cond_op_t)row_data);
    }
    g_object_set (G_OBJECT(renderer),
      "text",
      str,
      NULL);
}

static gboolean
_cu_cond_combo_filter_visible_cb (GtkTreeModel *model,
  GtkTreeIter *iter,
  gpointer data)
{
    gint value;
    gint row_data;
    value = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(data),
                              _CU_COND_FILTER_VALUE));

    gtk_tree_model_get (model, iter, 0, &row_data, -1);

    if (model == (gpointer)nwamui_env_get_condition_subject ()) {
        if (value != NWAMUI_COND_FIELD_LAST) {
            return row_data != NWAMUI_COND_FIELD_LAST;
        } else {
            return TRUE;
        }
    } else {
        switch (row_data) {
        case NWAMUI_COND_OP_IS:
        case NWAMUI_COND_OP_IS_NOT:
            return TRUE;
        case NWAMUI_COND_OP_IS_IN_RANGE:
        case NWAMUI_COND_OP_IS_NOT_IN_RANGE:
            return value == NWAMUI_COND_FIELD_IP_ADDRESS;
        case NWAMUI_COND_OP_CONTAINS:
        case NWAMUI_COND_OP_DOES_NOT_CONTAIN:
            return (value == NWAMUI_COND_FIELD_DOMAINNAME) ||
              (value == NWAMUI_COND_FIELD_ESSID);
        default:
            return FALSE;
        }
    }
}

static GtkWidget*
_cu_cond_combo_new (GtkTreeModel *model)
{
    GtkWidget *combo;
    GtkTreeModel      *filter;
    GtkCellRenderer *renderer;

    combo = gtk_combo_box_new ();

    filter = gtk_tree_model_filter_new (model, NULL);
    gtk_combo_box_set_model (GTK_COMBO_BOX(combo), GTK_TREE_MODEL (filter));
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER(filter),
	_cu_cond_combo_filter_visible_cb,
	(gpointer)combo,
	NULL);

    gtk_cell_layout_clear (GTK_CELL_LAYOUT(combo));
    renderer = gtk_cell_renderer_combo_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT(combo), renderer, TRUE);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT(combo),
	renderer,
	_cu_cond_combo_cell_cb,
	NULL,
	NULL);

    return combo;
}

/**
 * nwam_condition_vbox_new:
 * @returns: a new #NwamConditionVBox.
 *
 * Creates a new #NwamConditionVBox.
 **/
NwamConditionVBox*
nwam_condition_vbox_new (void)
{
    return NWAM_CONDITION_VBOX(g_object_new(NWAM_TYPE_CONDITION_VBOX,
	    "homogeneous", FALSE,
	    "spacing", 8,
	    NULL));
}

static void
table_condition_add_row( gpointer data, gpointer user_data )
{
    NwamConditionVBoxPrivate*   prv = GET_PRIVATE(user_data);
    NwamuiCond*                 cond = NWAMUI_COND(data);
    GtkWidget*                  crow = NULL;
    
    crow = table_condition_insert ( GTK_WIDGET(user_data), 
                                    prv->table_line_num - 1, cond); 

    if (prv->table_line_num == 1) {
        GtkWidget *remove;
        remove = GTK_WIDGET(g_object_get_data(G_OBJECT(crow),
                                              TABLE_ROW_REMOVE));
        gtk_widget_set_sensitive (remove, FALSE);
    }
}

static void
table_one_line_state (GtkWidget *box)
{
	GtkComboBox *combo1, *combo2;
	GtkEntry *entry;
	GtkButton *add, *remove;

	combo1 = GTK_COMBO_BOX(g_object_get_data(G_OBJECT(box), TABLE_ROW_COMBO1));
	combo2 = GTK_COMBO_BOX(g_object_get_data(G_OBJECT(box), TABLE_ROW_COMBO2));
	entry = GTK_ENTRY(g_object_get_data(G_OBJECT(box), TABLE_ROW_ENTRY));
	add = GTK_BUTTON(g_object_get_data(G_OBJECT(box), TABLE_ROW_ADD));
	remove = GTK_BUTTON(g_object_get_data(G_OBJECT(box), TABLE_ROW_REMOVE));
}

static void
table_lines_init (NwamConditionVBox *self, NwamuiObject *object)
{
    NwamConditionVBoxPrivate *prv = GET_PRIVATE(self);
    GtkWidget *crow;
    NwamuiCond* cond;
    
    /* cache all, then remove all */
    gtk_container_foreach (GTK_CONTAINER(self),
                           (GtkCallback)table_line_cache_all_cb,
                           (gpointer)self);

    if ( object ) {
        GList*  condition_list = nwamui_object_get_conditions( object );

        /* for each condition of NwamuiObject; do */
        g_list_foreach( condition_list,
          table_condition_add_row, 
          (gpointer)self);

        nwamui_util_free_obj_list( condition_list );

        /* If there are no rows inserted */
        if (prv->table_line_num == 0) {
            GtkWidget *remove;
            cond = nwamui_cond_new();
            /* FIXME - Why are we adding here, after removing? */
            crow = table_condition_insert (GTK_WIDGET(self), 0, cond); 
            
            /* I'm the only one, disable remove button */
            remove = GTK_WIDGET(g_object_get_data(G_OBJECT(crow),
                                                  TABLE_ROW_REMOVE));
            gtk_widget_set_sensitive (remove, FALSE);
        }
    }

    gtk_widget_show_all (GTK_WIDGET(self));
}

static void
table_lines_free (NwamConditionVBox *self)
{
    NwamConditionVBoxPrivate *prv = GET_PRIVATE(self);
    g_list_foreach (prv->table_box_cache, (GFunc)g_object_unref, NULL);
    g_list_free(prv->table_box_cache);
}

static GtkWidget *
table_conditon_new (NwamConditionVBox *self, NwamuiCond* cond )
{
	NwamConditionVBoxPrivate *prv = GET_PRIVATE(self);
	GtkBox *box = NULL;
	GtkComboBox *combo1, *combo2;
	GtkEntry *entry;
	GtkButton *add, *remove;
	
	if (prv->table_box_cache) {
		box = GTK_BOX(prv->table_box_cache->data);
		prv->table_box_cache = g_list_delete_link(prv->table_box_cache, prv->table_box_cache);
		combo1 = GTK_COMBO_BOX(g_object_get_data(G_OBJECT(box), TABLE_ROW_COMBO1));
		combo2 = GTK_COMBO_BOX(g_object_get_data(G_OBJECT(box), TABLE_ROW_COMBO2));
		entry = GTK_ENTRY(g_object_get_data(G_OBJECT(box), TABLE_ROW_ENTRY));
		add = GTK_BUTTON(g_object_get_data(G_OBJECT(box), TABLE_ROW_ADD));
		remove = GTK_BUTTON(g_object_get_data(G_OBJECT(box), TABLE_ROW_REMOVE));
	} else {
		box = GTK_BOX(gtk_hbox_new (FALSE, 2));
		combo1 = GTK_COMBO_BOX(_cu_cond_combo_new (nwamui_env_get_condition_subject()));
		combo2 = GTK_COMBO_BOX(_cu_cond_combo_new (nwamui_env_get_condition_predicate()));

		entry = GTK_ENTRY(gtk_entry_new ());

		g_signal_connect(combo1, "changed",
		    G_CALLBACK(condition_field_op_changed_cb),
		    (gpointer)box);

		g_signal_connect(combo2, "changed",
		    G_CALLBACK(condition_field_op_changed_cb),
		    (gpointer)box);

		g_signal_connect(entry, "changed",
		    G_CALLBACK(condition_value_changed_cb),
		    (gpointer)box);

		add = GTK_BUTTON(gtk_button_new());
		remove = GTK_BUTTON(gtk_button_new ());
		gtk_button_set_image (add, gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
		gtk_button_set_image (remove, gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON));
		g_signal_connect(add, "clicked",
		    G_CALLBACK(table_add_condition_cb),
		    (gpointer)self);
		g_signal_connect(remove, "clicked",
		    G_CALLBACK(table_delete_condition_cb),
		    (gpointer)self);
		gtk_box_pack_start (box, GTK_WIDGET(combo1), TRUE, TRUE, 0);
		gtk_box_pack_start (box, GTK_WIDGET(combo2), TRUE, TRUE, 0);
		gtk_box_pack_start (box, GTK_WIDGET(entry), TRUE, TRUE, 0);
		gtk_box_pack_start (box, GTK_WIDGET(add), FALSE, FALSE, 0);
		gtk_box_pack_start (box, GTK_WIDGET(remove), FALSE, FALSE, 0);
		g_object_set_data (G_OBJECT(box), TABLE_ROW_COMBO1, (gpointer)combo1);
		g_object_set_data (G_OBJECT(box), TABLE_ROW_COMBO2, (gpointer)combo2);
		g_object_set_data (G_OBJECT(box), TABLE_ROW_ENTRY, (gpointer)entry);
		g_object_set_data (G_OBJECT(box), TABLE_ROW_ADD, (gpointer)add);
		g_object_set_data (G_OBJECT(box), TABLE_ROW_REMOVE, (gpointer)remove);
	}
	g_object_set_data (G_OBJECT(box), TABLE_ROW_CDATA, cond);
	
	if (cond && NWAMUI_IS_COND( cond )) {
		// Initialize box according to data
		nwamui_cond_field_t field = nwamui_cond_get_field( cond );
		nwamui_cond_op_t    oper  = nwamui_cond_get_oper( cond );
		gchar*              value = nwamui_cond_get_value( cond );
		gtk_combo_box_set_active (GTK_COMBO_BOX(combo1), (gint)field); 
		gtk_combo_box_set_active (GTK_COMBO_BOX(combo2), (gint)oper);
		gtk_entry_set_text (GTK_ENTRY(entry), value?value:"" );
	} else {
		// default initialize box
		_cu_cond_combo_filter_value (GTK_COMBO_BOX(combo1),
		    NWAMUI_COND_FIELD_LAST);
		gtk_entry_set_text (GTK_ENTRY(entry), "");
	}
	return GTK_WIDGET(box);
}

static void
table_line_cache_all_cb (GtkWidget *widget, gpointer data)
{
    GtkWidget *clist = GTK_WIDGET (data);
    /* cache the remove one */
    table_condition_delete(clist, widget);
}

static GtkWidget*
table_condition_insert (GtkWidget *clist, guint row, NwamuiCond* cond)
{
	gpointer self = clist;
	NwamConditionVBoxPrivate *prv = GET_PRIVATE(self);
	GtkWidget *crow_new;

	crow_new = table_conditon_new (NWAM_CONDITION_VBOX(self), cond);
	gtk_box_pack_start (GTK_BOX(clist), crow_new, FALSE, FALSE, 0);
	prv->table_line_num++;
	gtk_box_reorder_child (GTK_BOX(clist), crow_new, row);
	gtk_widget_show_all (clist);
	g_debug ("num %d",     prv->table_line_num);
	return crow_new;
}

static void
table_condition_delete(GtkWidget *clist, GtkWidget *crow)
{
	gpointer self = clist;
	NwamConditionVBoxPrivate *prv = GET_PRIVATE(self);

	/* cache the remove one */
	prv->table_box_cache = g_list_prepend (prv->table_box_cache,
	    g_object_ref (G_OBJECT(crow)));
	gtk_container_remove (GTK_CONTAINER(clist), crow);
	prv->table_line_num--;
	gtk_widget_show_all (clist);
	g_debug ("num %d",     prv->table_line_num);
}

static void
table_add_condition_cb (GtkButton *button, gpointer data)
{
    /* a crow is condition row, a clist is an set of crows */
    GtkWidget *crow = GTK_WIDGET(gtk_widget_get_parent (GTK_WIDGET(button)));
    GtkWidget *clist = gtk_widget_get_parent (crow);
	NwamConditionVBoxPrivate *prv = GET_PRIVATE(data);
	guint row;
    NwamuiCond* cond;

    gtk_container_child_get (GTK_CONTAINER(clist),
                             crow,
                             "position", &row,
                             NULL);
    if (prv->table_line_num == 1 && row == 0) {
        /* emit add signal to add the condition from NwamuiObject */
        cond = NWAMUI_COND(g_object_get_data (G_OBJECT(crow), TABLE_ROW_CDATA));
        /* FIXME - Should signal come from table_condition_insert/remove? */
        g_signal_emit (data,
                       cond_signals[S_CONDITION_ADD],
                       0, /* details */
                       cond);
        /* I'm not single now, enable remove button */
        GtkWidget *remove;
        remove = GTK_WIDGET(g_object_get_data(G_OBJECT(crow),
                                              TABLE_ROW_REMOVE));
        gtk_widget_set_sensitive (remove, TRUE);
    }
    cond = nwamui_cond_new();
    crow = table_condition_insert (clist, row + 1, cond ); 
    /* emit add signal to add the condition from NwamuiObject */
    cond = g_object_get_data (G_OBJECT(crow), TABLE_ROW_CDATA);
    g_signal_emit (data,
                   cond_signals[S_CONDITION_ADD],
                   0, /* details */
                   cond);
}

static void
table_delete_condition_cb(GtkButton *button, gpointer data)
{
    /* a crow is condition row, a clist is an set of crows */
    GtkWidget *crow = GTK_WIDGET(gtk_widget_get_parent (GTK_WIDGET(button)));
    GtkWidget *clist = gtk_widget_get_parent (crow);
	NwamConditionVBoxPrivate *prv = GET_PRIVATE(data);
	guint row;
    NwamuiCond* cond = NULL;

    gtk_container_child_get (GTK_CONTAINER(clist),
                             crow,
                             "position", &row,
                             NULL);
    /* emit add signal to remove the condition from NwamuiObject */
    cond = NWAMUI_COND(g_object_get_data (G_OBJECT(crow), TABLE_ROW_CDATA));
    g_signal_emit (data,
         cond_signals[S_CONDITION_REMOVE],
         0, /* details */
         cond);

    table_condition_delete(clist, crow);
    
    if (prv->table_line_num == 0) {
        GtkWidget *remove;
        cond = nwamui_cond_new();
        /* FIXME - Why are we adding here, after removing? */
        crow = table_condition_insert (clist, 0, cond); /* XXX - NULL = cdata = cond obj? */
        /* I'm the last one, disable remove button */
        remove = GTK_WIDGET(g_object_get_data(G_OBJECT(crow),
                                              TABLE_ROW_REMOVE));
        gtk_widget_set_sensitive (remove, FALSE);
    }
}

static void
condition_field_op_changed_cb( GtkWidget* widget, gpointer data )
{
    NwamuiCond* cond = NWAMUI_COND(g_object_get_data (G_OBJECT(data), TABLE_ROW_CDATA));
    gint index;
    GtkTreeIter iter, realiter;
    GtkTreeModelFilter *filter;
    GtkTreeModel *model;

    filter = GTK_TREE_MODEL_FILTER(gtk_combo_box_get_model (GTK_COMBO_BOX(widget)));
    model = gtk_tree_model_filter_get_model (filter);

    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter)) {
        gtk_tree_model_filter_convert_iter_to_child_iter (filter,
          &realiter, &iter);
        gtk_tree_model_get (model, &realiter, 0, &index, -1);

        if (model == nwamui_env_get_condition_subject ()) {
            GtkComboBox *combo2 = GTK_COMBO_BOX(g_object_get_data(G_OBJECT(data), TABLE_ROW_COMBO2));
            /*
             * work out thec contents depends on the selection
             * of the first combo and also update self to remove
             * < No conditions >.
             */
            _cu_cond_combo_filter_value (GTK_COMBO_BOX(widget), index);
            _cu_cond_combo_filter_value (combo2, index);
            nwamui_cond_set_field( cond, (nwamui_cond_field_t)index);
        } else {
            nwamui_cond_set_oper( cond, (nwamui_cond_op_t)index);
        }
    }
}

static void
condition_value_changed_cb( GtkWidget* widget, gpointer data )
{
    NwamuiCond* cond = NWAMUI_COND(g_object_get_data (G_OBJECT(data), TABLE_ROW_CDATA));
    
    nwamui_cond_set_value(cond, gtk_entry_get_text(GTK_ENTRY(widget)));
}

static void
default_condition_add_signal_handler (NwamConditionVBox *self, GObject* data, gpointer user_data)
{
    g_debug("Condition Add : 0x%p", data);
    /* TODO - Move this from default sig handler to specific handler. */
/*     if ( self->prv->selected_object != NULL && data != NULL ) { */
/*         nwamui_object_condition_add(self->prv->selected_object, NWAMUI_COND(data) ); */
/*     } */
}

static void
default_condition_remove_signal_handler (NwamConditionVBox *self, GObject* data, gpointer user_data)
{
    g_debug("Condition Remove : 0x%p", data);
    /* TODO - Move this from default sig handler to specific handler. */
/*     if ( self->prv->selected_object != NULL && data != NULL ) { */
/*         nwamui_object_condition_remove(self->prv->selected_object, NWAMUI_COND(data) ); */
/*     } */
}

