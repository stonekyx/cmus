/*
 * Copyright 2008-2011 Various Authors
 * Copyright 2006 <ft@bewatermyfriend.org>
 *
 * heavily based on filters.h
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

#ifndef LYRICS_H
#define LYRICS_H

#include "list.h"
#include "window.h"
#include "search.h"
#include "keys.h"

struct lyrics_entry {
	struct list_head node;
	const char *line;
};

static inline struct lyrics_entry *iter_to_lyrics_entry(struct iter *iter)
{
	return iter->data1;
}

extern struct window *lyrics_win;
extern struct searchable *lyrics_searchable;

void lyrics_init(void);
void lyrics_show(const char *artist, const char *title, const char *lyrics);
void lyrics_add_line(const char *line);
void lyrics_clear(void);
void lyrics_exit(void);

void lyrics_save(const char *filename, const char *lyrics);
char *lyrics_load(const char *filename);

#endif /* LYRICS_H */
