#ifndef _COMMON_H_
#define _COMMON_H_
#include <glib.h>

#ifdef DEBUG
#define PDEBUG(fmt, args...) g_printerr(fmt, ## args)
#else
#define PDEBUG(fmt, args...)
#endif

#define ERR_MSG_SIZE 100

struct config_head;
struct record_config;

struct config_head{
	struct record_config *general;
	GSequence *record_list;
};

struct record_config{
	/* get from config file*/
	gboolean enable;
	gchar *name;
	gchar *src;
	gchar *location;
	gchar *dst_format;
	guint period;
	/* some runtime data */
	struct {
		gchar *err_msg;
		guint bus_watch_id;
		gint status;
		guint32 seqnum;
		void *loop;
		void *pipeline;
		void *sink;
	}r;
};
#endif
