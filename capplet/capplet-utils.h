#ifndef _CAPPLET_UTILS_H
#define _CAPPLET_UTILS_H

#include <gtk/gtk.h>
#include "nwam_pref_iface.h"
#include "libnwamui.h"

void daemon_compose_ncu_panel_mixed_combo(GtkComboBox *combo, NwamuiDaemon *daemon);
void daemon_update_ncu_panel_mixed_model(GtkComboBox *combo, NwamuiDaemon *daemon);
void daemon_compose_ncp_combo(GtkComboBox *combo, NwamuiDaemon *daemon,
    NwamPrefIFace *iface);
void daemon_update_ncp_combo(GtkComboBox *combo, NwamuiDaemon *daemon);

#endif /* _CAPPLET_UTILS_H */
