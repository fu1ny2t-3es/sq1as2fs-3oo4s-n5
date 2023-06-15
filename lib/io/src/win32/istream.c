/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * istream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"
#include "sqfs/io.h"
#include "sqfs/error.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct {
	sqfs_istream_t base;
	char *path;
	HANDLE hnd;

	bool eof;
	size_t buffer_offset;
	size_t buffer_used;
	sqfs_u8 buffer[BUFSZ];
} file_istream_t;

static int precache(sqfs_istream_t *strm)
{
	file_istream_t *file = (file_istream_t *)strm;
	DWORD diff, actual;

	if (file->eof)
		return 0;

	if (file->buffer_offset > 0 &&
	    file->buffer_offset < file->buffer_used) {
		memmove(file->buffer, file->buffer + file->buffer_offset,
			file->buffer_used - file->buffer_offset);
	}

	file->buffer_used -= file->buffer_offset;
	file->buffer_offset = 0;

	while (file->buffer_used < sizeof(file->buffer)) {
		diff = sizeof(file->buffer) - file->buffer_used;

		if (!ReadFile(file->hnd, file->buffer + file->buffer_used,
			      diff, &actual, NULL)) {
			DWORD error = GetLastError();

			if (error == ERROR_HANDLE_EOF ||
			    error == ERROR_BROKEN_PIPE) {
				file->eof = true;
				break;
			}

			SetLastError(error);
			return SQFS_ERROR_IO;
		}

		if (actual == 0) {
			file->eof = true;
			break;
		}

		file->buffer_used += actual;
	}

	return 0;
}

static int file_get_buffered_data(sqfs_istream_t *strm, const sqfs_u8 **out,
				  size_t *size, size_t want)
{
	file_istream_t *file = (file_istream_t *)strm;

	if (want > BUFSZ)
		want = BUFSZ;

	if (file->buffer_used == 0 ||
	    (file->buffer_used - file->buffer_offset) < want) {
		int ret = precache(strm);
		if (ret)
			return ret;
	}

	*out = file->buffer + file->buffer_offset;
	*size = file->buffer_used - file->buffer_offset;
	return (file->eof && *size == 0) ? 1 : 0;
}

static void file_advance_buffer(sqfs_istream_t *strm, size_t count)
{
	file_istream_t *file = (file_istream_t *)strm;

	assert(count <= file->buffer_used);

	file->buffer_offset += count;

	assert(file->buffer_offset <= file->buffer_used);
}

static const char *file_get_filename(sqfs_istream_t *strm)
{
	return ((file_istream_t *)strm)->path;
}

static void file_destroy(sqfs_object_t *obj)
{
	file_istream_t *file = (file_istream_t *)obj;

	CloseHandle(file->hnd);
	free(file->path);
	free(file);
}

int istream_open_handle(sqfs_istream_t **out, const char *path, HANDLE hnd)
{
	file_istream_t *file = calloc(1, sizeof(*file));
	sqfs_istream_t *strm = (sqfs_istream_t *)file;
	BOOL ret;

	if (file == NULL)
		return SQFS_ERROR_ALLOC;

	sqfs_object_init(file, file_destroy, NULL);

	file->path = strdup(path);
	if (file->path == NULL) {
		DWORD temp = GetLastError();
		free(file);
		SetLastError(temp);
		return SQFS_ERROR_ALLOC;
	}

	ret = DuplicateHandle(GetCurrentProcess(), hnd,
			      GetCurrentProcess(), &file->hnd,
			      0, FALSE, DUPLICATE_SAME_ACCESS);
	if (!ret) {
		DWORD temp = GetLastError();
		free(file->path);
		free(file);
		SetLastError(temp);
		return SQFS_ERROR_IO;
	}

	CloseHandle(hnd);

	strm->get_buffered_data = file_get_buffered_data;
	strm->advance_buffer = file_advance_buffer;
	strm->get_filename = file_get_filename;

	*out = strm;
	return 0;
}

int istream_open_file(sqfs_istream_t **out, const char *path)
{
	sqfs_file_handle_t hnd;
	int ret;

	ret = sqfs_open_native_file(&hnd, path, SQFS_FILE_OPEN_READ_ONLY);
	if (ret)
		return ret;

	ret = istream_open_handle(out, path, hnd);
	if (ret) {
		DWORD temp = GetLastError();
		CloseHandle(hnd);
		SetLastError(temp);
		return ret;
	}

	return 0;
}
