#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>
#include <strings.h>

#include "libnwamui.h"
#include "nwam_pref_iface.h"
#include "nwam_condition_vbox.h"
#include "nwam_rules_dialog.h"

#define RULES_DIALOG "rules_dialog"
#define RULES_VBOX_SW "rules_vbox_sw"
#define RULES_MATCH_ALL_RB "rules_match_all_rb"
#define RULES_MATCH_ANY_RB "rules_match_any_rb"

enum {
	PLACE_HOLDER,
	LAST_SIGNAL
};

static guint cond_signals[LAST_SIGNAL] = {0};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	NWAM_TYPE_RULES_DIALOG, NwamRulesDialogPrivate)) 

struct _NwamRulesDialogPrivate {
	GtkDialog *rules_dialog;
	GtkRadioButton *rules_match_any_rb;
	GtkRadioButton *rules_match_all_rb;
	NwamConditionVBox *rules_vbox;

	NwamuiObject*                  selected_object;
};

static void nwam_pref_init (gpointer g_iface, gpointer iface_data);
static gboolean refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force);
static gboolean apply(NwamPrefIFace *iface, gpointer user_data);
static gboolean cancel(NwamPrefIFace *iface, gpointer user_data);

static void nwam_rules_dialog_finalize (NwamRulesDialog *self);
static void response_cb(GtkWidget* widget, gint responseid, gpointer user_data);

G_DEFINE_TYPE_EXTENDED (NwamRulesDialog,
    nwam_rules_dialog,
    G_TYPE_OBJECT,
    0,
    G_IMPLEMENT_INTERFACE (NWAM_TYPE_PREF_IFACE, nwam_pref_init))

static gboolean
refresh(NwamPrefIFace *iface, gpointer user_data, gboolean force)
{
	NwamRulesDialogPrivate *prv = GET_PRIVATE(iface);

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
		if (prv->selected_object) {
			gchar *title;
			gchar *name;

			name = nwamui_object_get_name(prv->selected_object);
			title = g_strdup_printf("Edit Rules : %s", name);
			g_object_set(prv->rules_dialog,
			    "title", title,
			    NULL);
			g_free(name);
			g_free(title);
		}
	}
	return nwam_pref_refresh(NWAM_PREF_IFACE(prv->rules_vbox), prv->selected_object, force);
}

static gboolean
apply(NwamPrefIFace *iface, gpointer user_data)
{
	NwamRulesDialogPrivate *prv = GET_PRIVATE(iface);
	return nwam_pref_apply(NWAM_PREF_IFACE(prv->rules_vbox), user_data);
}

static gboolean
cancel(NwamPrefIFace *iface, gpointer user_data)
{
	NwamRulesDialogPrivate *prv = GET_PRIVATE(iface);
	return nwam_pref_cancel(NWAM_PREF_IFACE(prv->rules_vbox), user_data);
}

static void
nwam_pref_init (gpointer g_iface, gpointer iface_data)
{
	NwamPrefInterface *iface = (NwamPrefInterface *)g_iface;
	iface->refresh = refresh;
	iface->apply = apply;
	iface->cancel = cancel;
	iface->help = NULL;
    iface->dialog_run = nwam_rules_dialog_run;
}

static void
nwam_rules_dialog_class_init (NwamRulesDialogClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    /* Override Some Function Pointers */
    gobject_class->finalize = (void (*)(GObject*)) nwam_rules_dialog_finalize;

    g_type_class_add_private (klass, sizeof (NwamRulesDialogPrivate));
}
        
static void
nwam_rules_dialog_init (NwamRulesDialog *self)
{
	NwamRulesDialogPrivate *prv = GET_PRIVATE(self);
	GtkScrolledWindow *rules_vbox_sw;

	self->prv = prv;
	prv->rules_dialog = GTK_DIALOG(nwamui_util_glade_get_widget(RULES_DIALOG));
	g_signal_connect(GTK_DIALOG(self->prv->rules_dialog), "response", (GCallback)response_cb, (gpointer)self);

	prv->rules_match_all_rb = GTK_RADIO_BUTTON(nwamui_util_glade_get_widget(RULES_MATCH_ALL_RB));
	prv->rules_match_any_rb = GTK_RADIO_BUTTON(nwamui_util_glade_get_widget(RULES_MATCH_ANY_RB));

	prv->rules_vbox = nwam_condition_vbox_new();
	rules_vbox_sw = GTK_SCROLLED_WINDOW(nwamui_util_glade_get_widget(RULES_VBOX_SW));
	gtk_scrolled_window_add_with_viewport(rules_vbox_sw, GTK_WIDGET(prv->rules_vbox));

	/* Initializing */
	nwam_pref_refresh(NWAM_PREF_IFACE(self), NULL, TRUE);
}

static void
nwam_rules_dialog_finalize (NwamRulesDialog *self)
{
	NwamRulesDialogPrivate *prv = GET_PRIVATE(self);

	g_object_unref(prv->rules_vbox);

	if (prv->selected_object) {
		g_object_unref(prv->selected_object);
	}

	G_OBJECT_CLASS(nwam_rules_dialog_parent_class)->finalize(G_OBJECT(self));
}

/**
 * nwam_rules_dialog_new:
 * @returns: a new #NwamRulesDialog.
 *
 * Creates a new #NwamRulesDialog.
 **/
NwamRulesDialog*
nwam_rules_dialog_new (void)
{
    return NWAM_RULES_DIALOG(g_object_new(NWAM_TYPE_RULES_DIALOG,
	    NULL));
}

gint
nwam_rules_dialog_run(NwamRulesDialog  *self, GtkWindow* parent)
{
	gint response = GTK_RESPONSE_NONE;
    
	g_assert(NWAM_IS_RULES_DIALOG(self));

	if ( self->prv->rules_dialog != NULL ) {
		if (parent) {
			gtk_window_set_transient_for (GTK_WINDOW(self->prv->rules_dialog), parent);
			gtk_window_set_modal (GTK_WINDOW(self->prv->rules_dialog), TRUE);
		} else {
			gtk_window_set_transient_for (GTK_WINDOW(self->prv->rules_dialog), NULL);
			gtk_window_set_modal (GTK_WINDOW(self->prv->rules_dialog), FALSE);		
		}

		response = gtk_dialog_run(GTK_DIALOG(self->prv->rules_dialog));
	}
	return response;
}

static void
response_cb(GtkWidget* widget, gint responseid, gpointer user_data)
{
	NwamRulesDialog *self = NWAM_RULES_DIALOG(user_data);
	gboolean           stop_emission = FALSE;
	
	switch (responseid) {
	case GTK_RESPONSE_NONE:
		g_debug("GTK_RESPONSE_NONE");
		stop_emission = TRUE;
		break;
	case GTK_RESPONSE_DELETE_EVENT:
		g_debug("GTK_RESPONSE_DELETE_EVENT");
		break;
	case GTK_RESPONSE_OK:
		g_debug("GTK_RESPONSE_OK");
		if (nwam_pref_apply(NWAM_PREF_IFACE(self), self->prv->selected_object)) {
			gtk_widget_hide_all (GTK_WIDGET(self->prv->rules_dialog));
		}
		else {
			/* TODO - report error to user */
			stop_emission = TRUE;
		}
		break;
	case GTK_RESPONSE_REJECT: /* Generated by Revert Button */
		/* TODO - Implement refresh functionality */
		g_debug("GTK_RESPONSE_REJECT");
		nwam_pref_cancel(NWAM_PREF_IFACE(self), self->prv->selected_object);
		stop_emission = TRUE;
		break;
	case GTK_RESPONSE_CANCEL:
		g_debug("GTK_RESPONSE_CANCEL");
		break;
	case GTK_RESPONSE_HELP:
		break;
	}
	if ( stop_emission ) {
		g_signal_stop_emission_by_name(widget, "response" );
	} else {
		gtk_widget_hide(GTK_WIDGET(self->prv->rules_dialog));
	}
}
