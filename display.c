/* display.c
 * Routines for packet display windows
 *
 * $Id: display.c,v 1.2 1999/06/19 01:47:43 guy Exp $
 *
 * Ethereal - Network traffic analyzer
 * By Gerald Combs <gerald@zing.org>
 * Copyright 1998 Gerald Combs
 *
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include <gtk/gtk.h>
#include <pcap.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <signal.h>
#include <errno.h>

#ifdef NEED_SNPRINTF_H
# ifdef HAVE_STDARG_H
#  include <stdarg.h>
# else
#  include <varargs.h>
# endif
# include "snprintf.h"
#endif

#ifdef HAVE_SYS_SOCKIO_H
# include <sys/sockio.h>
#endif

#include "timestamp.h"
#include "column.h"
#include "packet.h"
#include "file.h"
#include "display.h"

extern capture_file  cf;
extern GtkWidget *packet_list;

/* Display callback data keys */
#define E_DISPLAY_TIME_ABS_KEY   "display_time_abs"
#define E_DISPLAY_TIME_REL_KEY   "display_time_rel"
#define E_DISPLAY_TIME_DELTA_KEY "display_time_delta"

static void display_opt_ok_cb(GtkWidget *, gpointer);
static void display_opt_close_cb(GtkWidget *, gpointer);

void
display_opt_cb(GtkWidget *w, gpointer d) {
  GtkWidget     *display_opt_w, *button, *main_vb, *bbox, *ok_bt, *cancel_bt;

  display_opt_w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(display_opt_w), "Ethereal: Display Options");
  
  /* Container for each row of widgets */
  main_vb = gtk_vbox_new(FALSE, 3);
  gtk_container_border_width(GTK_CONTAINER(main_vb), 5);
  gtk_container_add(GTK_CONTAINER(display_opt_w), main_vb);
  gtk_widget_show(main_vb);
  
  button = gtk_radio_button_new_with_label(NULL, "Time of day");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
               (timestamp_type == ABSOLUTE));
  gtk_object_set_data(GTK_OBJECT(display_opt_w), E_DISPLAY_TIME_ABS_KEY,
               button);
  gtk_box_pack_start(GTK_BOX(main_vb), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_radio_button_new_with_label(
               gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
               "Seconds since beginning of capture");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
               (timestamp_type == RELATIVE));
  gtk_object_set_data(GTK_OBJECT(display_opt_w), E_DISPLAY_TIME_REL_KEY,
               button);
  gtk_box_pack_start(GTK_BOX(main_vb), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_radio_button_new_with_label(
               gtk_radio_button_group(GTK_RADIO_BUTTON(button)),
               "Seconds since previous frame");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
               (timestamp_type == DELTA));
  gtk_object_set_data(GTK_OBJECT(display_opt_w), E_DISPLAY_TIME_DELTA_KEY,
		button);
  gtk_box_pack_start(GTK_BOX(main_vb), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
  
  /* Button row: OK and cancel buttons */
  bbox = gtk_hbutton_box_new();
  gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
  gtk_container_add(GTK_CONTAINER(main_vb), bbox);
  gtk_widget_show(bbox);

  ok_bt = gtk_button_new_with_label ("OK");
  gtk_signal_connect_object(GTK_OBJECT(ok_bt), "clicked",
    GTK_SIGNAL_FUNC(display_opt_ok_cb), GTK_OBJECT(display_opt_w));
  GTK_WIDGET_SET_FLAGS(ok_bt, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (bbox), ok_bt, TRUE, TRUE, 0);
  gtk_widget_grab_default(ok_bt);
  gtk_widget_show(ok_bt);

  cancel_bt = gtk_button_new_with_label ("Cancel");
  gtk_signal_connect_object(GTK_OBJECT(cancel_bt), "clicked",
    GTK_SIGNAL_FUNC(display_opt_close_cb), GTK_OBJECT(display_opt_w));
  GTK_WIDGET_SET_FLAGS(cancel_bt, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (bbox), cancel_bt, TRUE, TRUE, 0);
  gtk_widget_show(cancel_bt);

  gtk_widget_show(display_opt_w);
}

static void
display_opt_ok_cb(GtkWidget *w, gpointer data) {
  GtkWidget *button;
  GtkStyle  *pl_style;
  int i;
  gint col_fmt;

#ifdef GTK_HAVE_FEATURES_1_1_0
  data = w;
#endif

  button = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(data),
                                              E_DISPLAY_TIME_ABS_KEY);
  if (GTK_TOGGLE_BUTTON (button)->active)
    timestamp_type = ABSOLUTE;

  button = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(data),
                                              E_DISPLAY_TIME_REL_KEY);
  if (GTK_TOGGLE_BUTTON (button)->active)
    timestamp_type = RELATIVE;

  button = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(data),
                                              E_DISPLAY_TIME_DELTA_KEY);
  if (GTK_TOGGLE_BUTTON (button)->active)
    timestamp_type = DELTA;

  gtk_widget_destroy(GTK_WIDGET(data));

  /* Recompute the column widths; we may have changed the format of the
     time stamp column. */
  pl_style = gtk_widget_get_style(packet_list);
  for (i = 0; i < cf.cinfo.num_cols; i++) {
    col_fmt = get_column_format(i);
    gtk_clist_set_column_width(GTK_CLIST(packet_list), i,
      get_column_width(col_fmt, pl_style->font));
    if (col_fmt == COL_NUMBER)
      gtk_clist_set_column_justification(GTK_CLIST(packet_list), i, 
        GTK_JUSTIFY_RIGHT);
  }

  redisplay_packets(&cf);
}

static void
display_opt_close_cb(GtkWidget *w, gpointer win) {

#ifdef GTK_HAVE_FEATURES_1_1_0
  win = w;
#endif
  gtk_grab_remove(GTK_WIDGET(win));
  gtk_widget_destroy(GTK_WIDGET(win));
}
