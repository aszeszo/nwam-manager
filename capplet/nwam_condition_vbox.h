#ifndef _nwam_condition_vbox_H
#define	_nwam_condition_vbox_H

G_BEGIN_DECLS

#define NWAM_TYPE_CONDITION_VBOX               (nwam_condition_vbox_get_type ())
#define NWAM_CONDITION_VBOX(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAM_TYPE_CONDITION_VBOX, NwamConditionVBox))
#define NWAM_CONDITION_VBOX_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAM_TYPE_CONDITION_VBOX, NwamConditionVBoxClass))
#define NWAM_IS_CONDITION_VBOX(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAM_TYPE_CONDITION_VBOX))
#define NWAM_IS_CONDITION_VBOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAM_TYPE_CONDITION_VBOX))
#define NWAM_CONDITION_VBOX_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAM_TYPE_CONDITION_VBOX, NwamConditionVBoxClass))


typedef struct _NwamConditionVBox		NwamConditionVBox;
typedef struct _NwamConditionVBoxClass          NwamConditionVBoxClass;
typedef struct _NwamConditionVBoxPrivate	NwamConditionVBoxPrivate;

struct _NwamConditionVBox
{
	GtkVBox                      object;

	/*< private >*/
	NwamConditionVBoxPrivate    *prv;
};

struct _NwamConditionVBoxClass
{
	GtkVBoxClass                parent_class;
	void (*condition_add)    (NwamConditionVBox *self, GObject *data, gpointer user_data);
	void (*condition_remove)  (NwamConditionVBox *self, GObject *data, gpointer user_data);
};


extern  GType                   nwam_condition_vbox_get_type (void) G_GNUC_CONST;
extern  NwamConditionVBox*      nwam_condition_vbox_new (void);

G_END_DECLS

#endif	/* _nwam_condition_vbox_H */
