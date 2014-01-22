#include "common.h"
#include <gst/gst.h>
#include <glib.h> 
#include <stdint.h>
#include <stdbool.h>

static gboolean config_decode
(GKeyFile *file, struct config_head *config);
/*it will set the eacho record config to default value
 * when it is not special in itself's group except the
 * src key. */
static gboolean config_set_default(struct config_head *config);

struct config_head *config_init(const gchar *path)
{
	GKeyFile *file = NULL;
	file = g_key_file_new();

	struct config_head *head = NULL;
	head = g_malloc(sizeof(struct config_head));
	head->general = g_malloc(sizeof(struct record_config));
	head->record_list = g_sequence_new(NULL);

	if (NULL == path) {
		if (g_key_file_load_from_file
		    (file, "/etc/record.conf", G_KEY_FILE_NONE,
		     NULL))
			config_decode(file, head);

		if (g_key_file_load_from_file
		     (file, "~/.record.conf", G_KEY_FILE_NONE,
		     NULL))
			config_decode(file, head);

		if (g_key_file_load_from_file
		    (file, "record.conf", G_KEY_FILE_NONE,
		     NULL))
			config_decode(file, head);

		return head;
	} else {
		g_key_file_load_from_file
			(file, path, G_KEY_FILE_NONE, NULL);
		config_decode(file, head);
		return head;
	}

	return NULL;
}

static gboolean config_decode(GKeyFile *file, struct config_head *config){
	if (NULL == file || NULL == config)
		return FALSE;
	gchar **groups = NULL;
	gsize length;

	GRegex *regex;

	groups = g_key_file_get_groups(file, &length);
	if (0 == length)
		return false;

	if (g_key_file_has_group(file, "general")){
		config->general->location =
			g_key_file_get_value
			(file, "general", "location", NULL);
		config->general->period =
			g_key_file_get_integer
			(file, "general", "switch_time", NULL);
		config->general->dst_format =
			g_key_file_get_value
			(file, "general", "file_name", NULL);

		/* we don't need them below*/
		config->general->src = NULL;
		config->general->r.bus_watch_id = -1;
		config->general->r.loop = NULL;
		config->general->r.pipeline = NULL;
		config->general->r.sink = NULL;
	}

	regex = g_regex_new("camera[0-9]{1,}", 0, 0, NULL);
	do{
		struct record_config *data =
		g_malloc0(sizeof(struct record_config));

		if (g_regex_match(regex, *groups, 0, 0)){
			data->name = g_strdup(*groups);
			data->src =
				g_key_file_get_value
				(file, *groups, "src_url", NULL);
			if (NULL == data->src){
				g_free(data);
				groups++;
				continue;
			}
			data->enable = 
				g_key_file_get_integer
				(file, *groups, "enable", NULL);
			
			data->location =
				g_key_file_get_value
				(file, *groups, "location", NULL);
			/*
			g_strlcat(data->dst, g_key_file_get_value
				(file, *groups, "locations", NULL), 128);
			*/
			
			data->period =
				g_key_file_get_integer
				(file, *groups, "switch_time", NULL);
			data->dst_format =
				g_key_file_get_value
				(file, *groups, "file_name", NULL);
			data->r.bus_watch_id = -1;
			data->r.loop = NULL;
			data->r.pipeline = NULL;
			data->r.sink = NULL;
			data->r.err_msg = NULL;

			g_sequence_append(config->record_list, (gpointer)data);
			/*
			g_free(data);
			*/
		}
		else{
			g_free(data);
		}

		groups++;
	}while(NULL != *groups);

	config_set_default(config);

	/*g_strfreev(groups);*/
	g_regex_unref(regex);

	return TRUE;
}

static gboolean config_set_default(struct config_head *config){
        GSequenceIter *begin, *current, *next;

        begin = g_sequence_get_begin_iter(config->record_list);
        current = begin;
        next = begin;
	struct record_config *data = NULL;

        while(!g_sequence_iter_is_end(next)){
        	current = next;
                data = g_sequence_get(current);
		/*
		g_print("name %s\n", data->name);
		g_print("name src %s\n", data->src);
		*/
		if (NULL == data->location)
			data->location = config->general->location;
		if (0 == data->period)
			data->period = config->general->period;
		if (NULL == data->dst_format)
			data->dst_format = config->general->dst_format;

                next = g_sequence_iter_next(current);
	}
	return TRUE;
}
