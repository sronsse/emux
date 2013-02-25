#ifndef _MEMORY_H
#define _MEMORY_H

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#define KB(x) (x * 1024)

void *memory_map_file(char *path, int offset, int size);
void memory_unmap_file(void *data, int size);

#endif

