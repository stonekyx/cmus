/*
 * Copyright 2013 <max@fs.lmu.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "lyrics.h"
#include "window.h"
#include "search.h"
#include "misc.h"
#include "xmalloc.h"
#include "keys.h"
#include "command_mode.h"
#include "ui_curses.h"
#include "options.h"
#include "cmdline.h"
#include "debug.h"
#include "worker.h"
#include "cmus.h"

#include <stdio.h>
#include <signal.h>

struct window *lyrics_win;
struct searchable *lyrics_searchable;

LIST_HEAD(lyrics_head);

static inline void lyrics_entry_to_iter(struct lyrics_entry *e, struct iter *iter)
{
	iter->data0 = &lyrics_head;
	iter->data1 = e;
	iter->data2 = NULL;
}

void lyrics_add_line_(const char *line);

static GENERIC_ITER_PREV(lyrics_get_prev, struct lyrics_entry, node)
static GENERIC_ITER_NEXT(lyrics_get_next, struct lyrics_entry, node)

static int lyrics_search_get_current(void *data, struct iter *iter)
{
	return window_get_sel(lyrics_win, iter);
}

static int lyrics_search_matches(void *data, struct iter *iter, const char *text)
{
	int matched = 0;
	char **words;
	words = get_words(text);

	if (words[0] != NULL) {
		struct lyrics_entry *ent;
		int i;

		ent = iter_to_lyrics_entry(iter);
		for (i = 0; ; i++) {
			if (words[i] == NULL) {
				window_set_sel(lyrics_win, iter);
				matched = 1;
				break;
			}
			if (!u_strcasestr(ent->line, words[i]))
				break;
		}
	}
	free_str_array(words);
	return matched;
}

static const struct searchable_ops lyrics_search_ops = {
	.get_prev = lyrics_get_prev,
	.get_next = lyrics_get_next,
	.get_current = lyrics_search_get_current,
	.matches = lyrics_search_matches
};

void lyrics_clear(void)
{
	struct lyrics_entry *ent, *tmp;

	list_for_each_entry_safe(ent, tmp, &lyrics_head, node){
		list_del(&(ent->node));
		free((char *)ent->line);
		free(ent);
	}
	window_set_empty(lyrics_win);
}

void lyrics_add_line_(const char *line)
{
	struct lyrics_entry *ent;
	ent = xnew(struct lyrics_entry, 1);
	ent->line = xstrdup(line);
	list_add_tail(&ent->node, &lyrics_head);
}

void lyrics_add_line(const char *line)
{
	lyrics_add_line_(line);
	window_set_contents(lyrics_win, &lyrics_head);
	window_changed(lyrics_win);
}

void lyrics_show(const char * artist, const char * title, const char *lines)
{
	char *line, *save_ptr, *line_bak;

	lyrics_clear();
	if (artist && title){
		line = xmalloc(4 + strlen(artist) + strlen(title));
		sprintf(line, "%s - %s", artist, title);
		lyrics_add_line(line);
		free(line);
	}
	line_bak = xstrdup(lines);
	line = strtok_r(line_bak, "\n", &save_ptr);
	/* we expect the lines to be newline separated */
	while (line){
		lyrics_add_line_(line);
		line = strtok_r(NULL, "\n", &save_ptr);
	}
	free(line_bak);
	window_set_contents(lyrics_win, &lyrics_head);
	window_changed(lyrics_win);
}

void lyrics_init(void)
{
	struct iter iter;

	lyrics_win = window_new(lyrics_get_prev, lyrics_get_next);
	lyrics_add_line("No lyrics searched yet. To search, select a track/file "\
		"in view 1-4 and use lyrics-fetch");

	iter.data0 = &lyrics_head;
	iter.data1 = NULL;
	iter.data2 = NULL;
	lyrics_searchable = searchable_new(NULL, &iter, &lyrics_search_ops);
}

void lyrics_exit(void)
{
	kill(0, SIGUSR1);
	worker_remove_jobs(JOB_TYPE_LYRICS);
	lyrics_clear();
	searchable_free(lyrics_searchable);
	window_free(lyrics_win);
}
