#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#endif
#include <env.h>
#include <file.h>
#include <log.h>

#define MAX_PATH_LENGTH 1024

static file_handle_t open_file(char *path, char *mode);
static void *map_file(char *path, int offset, int size);
static void get_full_path(char *dst, int len, enum path_type type, char *path);

file_handle_t open_file(char *path, char *mode)
{
	file_handle_t file;

	/* Open requested file */
	LOG_D("Opening \"%s\".\n", path);
	file = fopen(path, mode);

	/* Warn if file was not opened */
	if (!file)
		LOG_W("Could not open \"%s\"!\n", path);

	/* Return file handle */
	return file;
}

void *map_file(char *path, int offset, int size)
{
#ifdef _WIN32
	SYSTEM_INFO system_info;
	HANDLE file;
	HANDLE mapping;
	char *data;
	int pa_offset;

	LOG_D("Mapping \"%s\".\n", path);

	GetSystemInfo(&system_info);
	pa_offset = offset & ~(system_info.dwAllocationGranularity - 1);
	size += offset - pa_offset;

	file = CreateFile(path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	if (file == INVALID_HANDLE_VALUE) {
		LOG_W("Could not open \"%s\"!\n", path);
		return NULL;
	}

	mapping = CreateFileMapping(file, 0, PAGE_READONLY, 0, size, 0);
	CloseHandle(file);
	if (!mapping) {
		LOG_W("Could not map \"%s\"!\n", path);
		return NULL;
	}

	data = MapViewOfFile(mapping, FILE_MAP_READ, 0, pa_offset, size);
	CloseHandle(mapping);

	data += offset - pa_offset;
	return data;
#else
	int fd;
	struct stat sb;
	char *data;
	int pa_offset;

	LOG_D("Mapping \"%s\".\n", path);

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		LOG_W("Could not open \"%s\"!\n", path);
		return NULL;
	}

	fstat(fd, &sb);
	if (!S_ISREG(sb.st_mode)) {
		LOG_W("Could not open \"%s\"!\n", path);
		close(fd);
		return NULL;
	}

	if (offset + size > sb.st_size) {
		LOG_W("Could not map \"%s\"!\n", path);
		close(fd);
		return NULL;
	}

	/* Offset for mmap must be page-aligned */
	pa_offset = offset & ~(sysconf(_SC_PAGE_SIZE) - 1);
	size += offset - pa_offset;

	data = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, pa_offset);
	if (data == MAP_FAILED) {
		LOG_W("Could not map \"%s\"!\n", path);
		close(fd);
		return NULL;
	}

	/* Adapt returned pointer to point to requested location */
	data += offset - pa_offset;

	close(fd);
	return data;
#endif
}

void get_full_path(char *dst, int len, enum path_type type, char *path)
{
	char *dir;

	/* Compute full path based on path type */
	switch (type) {
	case PATH_SYSTEM:
		dir = env_get_system_path();
		snprintf(dst, len, "%s/%s", dir, path);
		break;
	case PATH_CONFIG:
		dir = env_get_config_path();
		snprintf(dst, len, "%s/%s", dir, path);
		break;
	case PATH_DATA:
	default:
		snprintf(dst, len, "%s", path);
		break;
	}
}

file_handle_t file_open(enum path_type type, char *path, char *mode)
{
	char full_path[MAX_PATH_LENGTH + 1];
	file_handle_t file;

	/* Get full path based on type */
	get_full_path(full_path, MAX_PATH_LENGTH, type, path);

	/* Open file and fall back to original path if needed */
	file = open_file(full_path, mode);
	if (!file && (type != PATH_DATA))
		file = open_file(path, mode);

	/* Return file handle */
	return file;
}

void file_close(file_handle_t f)
{
	/* Close specified file handle */
	fclose(f);
}

uint32_t file_get_size(file_handle_t f)
{
	uint32_t size;

	/* Get ROM file size and return it */
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	return size;
}

bool file_read(file_handle_t f, void *dst, int offset, int size)
{
	int n;
	fseek(f, offset, SEEK_SET);
	n = fread(dst, 1, size, f);
	return (n == size);
}

bool file_write(file_handle_t f, void *src, int offset, int size)
{
	int n;
	fseek(f, offset, SEEK_SET);
	n = fwrite(src, 1, size, f);
	return (n == size);
}

void *file_map(enum path_type type, char *path, int offset, int size)
{
	char full_path[MAX_PATH_LENGTH + 1];
	void *address;

	/* Get full path based on type */
	get_full_path(full_path, MAX_PATH_LENGTH, type, path);

	/* Map file and fall back to original path if needed */
	address = map_file(full_path, offset, size);
	if (!address && (type != PATH_DATA))
		address = map_file(path, offset, size);

	/* Return mapped address */
	return address;
}

void file_unmap(void *data, int size)
{
#ifdef _WIN32
	SYSTEM_INFO system_info;
	int pa_data;
	GetSystemInfo(&system_info);
	pa_data = (int)data & ~(system_info.dwPageSize - 1);
	size += (int)data - pa_data;
	UnmapViewOfFile((void *)pa_data);
#else
	intptr_t pa_data = (intptr_t)data & ~(sysconf(_SC_PAGE_SIZE) - 1);
	size += (intptr_t)data - pa_data;
	munmap((void *)pa_data, size);
#endif
}

