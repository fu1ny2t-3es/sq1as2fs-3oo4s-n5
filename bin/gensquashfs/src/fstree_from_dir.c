/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_dir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mkfs.h"

int fstree_from_dir(fstree_t *fs, sqfs_dir_iterator_t *dir)
{
	for (;;) {
		sqfs_dir_entry_t *ent = NULL;
		tree_node_t *n = NULL;
		char *extra = NULL;

		int ret = dir->next(dir, &ent);
		if (ret > 0)
			break;
		if (ret < 0) {
			sqfs_perror("readdir", NULL, ret);
			return -1;
		}

		if (S_ISLNK(ent->mode)) {
			ret = dir->read_link(dir, &extra);
			if (ret) {
				sqfs_perror("readlink", ent->name, ret);
				free(ent);
				return -1;
			}
		}

		n = fstree_add_generic(fs, ent, extra);
		free(extra);
		free(ent);

		if (n == NULL) {
			perror("creating tree node");
			return -1;
		}
	}

	return 0;
}
