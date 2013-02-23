#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

void *memory_map_file(char *path, int offset, int size)
{
	int fd;
	struct stat sb;
	void *data;

	fd = open(path, O_RDONLY);
	if (fd == -1)
		return NULL;

	fstat(fd, &sb);
	if (!S_ISREG(sb.st_mode)) {
		close(fd);
		return NULL;
	}

	data = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, offset);
	if (data == MAP_FAILED) {
		close(fd);
		return NULL;
	}

	close(fd);
	return data;
}

void memory_unmap_file(void *data, int size)
{
	munmap(data, size);
}

