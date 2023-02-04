/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mkfs.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 * Copyright (C) 2022 Enno Boland <mail@eboland.de>
 */
#ifndef MKFS_H
#define MKFS_H

#include "config.h"

#include "common.h"
#include "fstree.h"
#include "util/util.h"
#include "io/file.h"

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>

#if defined(__APPLE__) && defined(__MACH__)
#define llistxattr(path, list, size) \
	listxattr(path, list, size, XATTR_NOFOLLOW)

#define lgetxattr(path, name, value, size) \
	getxattr(path, name, value, size, 0, XATTR_NOFOLLOW)
#endif
#endif

#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#include <selinux/label.h>
#endif

#include <getopt.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

typedef struct {
	sqfs_writer_cfg_t cfg;
	unsigned int dirscan_flags;
	const char *infile;
	const char *selinux;
	const char *xattr_file;
	const char *sortfile;
	bool no_tail_packing;

	/* copied from command line or constructed from infile argument
	   if not specified. Must be free'd. */
	char *packdir;

	unsigned int force_uid_value;
	unsigned int force_gid_value;
	bool force_uid;
	bool force_gid;

	bool scan_xattr;
} options_t;

struct XattrMapEntry {
	char *key;
	sqfs_u8 *value;
	size_t value_len;
	struct XattrMapEntry *next;
};

struct XattrMapPattern {
	char *path;
	struct XattrMapEntry *entries;
	struct XattrMapPattern *next;
};

struct XattrMap {
	struct XattrMapPattern *patterns;
};

void process_command_line(options_t *opt, int argc, char **argv);

int xattrs_from_dir(fstree_t *fs, const char *path, void *selinux_handle,
		    void *xattr_map, sqfs_xattr_writer_t *xwr, bool scan_xattr);

void *xattr_open_map_file(const char *path);

int
xattr_apply_map_file(char *path, void *map, sqfs_xattr_writer_t *xwr);

void xattr_close_map_file(void *xattr_map);

void *selinux_open_context_file(const char *filename);

int selinux_relable_node(void *sehnd, sqfs_xattr_writer_t *xwr,
			 tree_node_t *node, const char *path);

void selinux_close_context_file(void *sehnd);

/*
  Parses the file format accepted by gensquashfs and produce a file system
  tree from it. File input paths are interpreted as relative to the current
  working directory.

  On failure, an error report with filename and line number is written
  to stderr.

  Returns 0 on success.
 */
int fstree_from_file(fstree_t *fs, const char *filename,
		     const char *basepath);

int fstree_from_file_stream(fstree_t *fs, istream_t *file,
			    const char *basepath);

/*
  Recursively scan a directory to build a file system tree.

  Returns 0 on success, prints to stderr on failure.
 */
int fstree_from_dir(fstree_t *fs, tree_node_t *root,
		    const char *path, scan_node_callback cb, void *user,
		    unsigned int flags);

/*
  Same as fstree_from_dir, but scans a sub-directory inside the specified path.

  Returns 0 on success, prints to stderr on failure.
 */
int fstree_from_subdir(fstree_t *fs, tree_node_t *root,
		       const char *path, const char *subdir,
		       scan_node_callback cb, void *user, unsigned int flags);

int fstree_sort_files(fstree_t *fs, istream_t *sortfile);

#endif /* MKFS_H */