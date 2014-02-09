#ifndef _FILE_H
#define _FILE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define FLAG_READ	0x01
#define FLAG_WRITE	0x02

enum path_type {
	PATH_DATA,
	PATH_SYSTEM,
	PATH_CONFIG
};

typedef FILE *file_handle_t;

file_handle_t file_open(enum path_type type, char *path, char *mode);
void file_close(file_handle_t);
uint32_t file_get_size(file_handle_t f);
bool file_read(file_handle_t f, void *dst, int size);
bool file_write(file_handle_t f, void *src, int size);
void *file_map(enum path_type type, char *path, int offset, int size);
void file_unmap(void *data, int size);

#endif

