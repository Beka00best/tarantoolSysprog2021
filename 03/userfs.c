#include "userfs.h"

enum {
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 1024,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_errno_code = UFS_ERR_NO_ERR;

struct block {
	/** Block memory. */
	char *memory;
	/** How many bytes are occupied. */
	int occupied;
	/** Next block in the file. */
	struct block *next;
	/** Previous block in the file. */
	struct block *prev;

};

struct file {
	/** Double-linked list of file blocks. */
	struct block *block_list;
	/**
	 * Last block in the list above for fast access to the end
	 * of file.
	 */
	struct block *last_block;
	/** How many file descriptors are opened on the file. */
	int refs;
	int blocks;
	int deleted;
	/** File name. */
	char *name;
	/** Files are stored in a double-linked list. */
	struct file *next;
	struct file *prev;

};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
	struct file *file;
	struct block *block_pos;
	char * mem_pos;
	int flags;
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;
static const size_t fd_min_alloc_size = 4;

enum ufs_error_code
ufs_errno()
{
	return ufs_errno_code;
}

int
ufs_open(const char *filename, int flags)
{
	struct file *curr = file_list;
	struct file *prev = NULL;
	struct filedesc *curr_desc = NULL;
	int i = 0;

	while (curr != NULL) {
		if (strcmp(filename, curr->name) == 0 && \
			curr->deleted == 0) break;
		prev = curr;
		curr = curr->next;
	}
	if (curr == NULL) {
		if (flags & UFS_CREATE) {
			curr = calloc(1, sizeof(struct file));
			if (curr == NULL) {
				ufs_errno_code = UFS_ERR_NO_MEM;
				return -1;
			}
			curr->name = calloc(strlen(filename) + 1, 1);
			if (curr->name == NULL) {
				ufs_errno_code = UFS_ERR_NO_MEM;
				return -1;
			}
			strcpy(curr->name, filename);
			curr->prev = prev;
			if (prev == NULL) file_list = curr;
			else prev->next = curr;
		} else {
			ufs_errno_code = UFS_ERR_NO_FILE;
			return -1;
		}
	}

	if (file_descriptor_capacity == file_descriptor_count) {
		file_descriptors = realloc(file_descriptors, \
				sizeof(struct filedesc *) * \
				(file_descriptor_capacity \
				+ fd_min_alloc_size));
		if (file_descriptors == NULL) {
			ufs_errno_code = UFS_ERR_NO_MEM;
			return -1;
		}
		memset(file_descriptors + file_descriptor_capacity, \
			0, fd_min_alloc_size * sizeof(struct filedesc *));
		file_descriptor_capacity += fd_min_alloc_size;
	}
	for (i = 0; i < file_descriptor_capacity; i++) {
		if (file_descriptors[i] == NULL) {
			file_descriptors[i] = malloc(sizeof(struct filedesc));
			if (file_descriptors[i] == NULL) {
				ufs_errno_code = UFS_ERR_NO_MEM;
				return -1;
			}
			file_descriptor_count++;
			break;
		}
	}
	
	if (flags & UFS_READ_ONLY || flags & UFS_WRITE_ONLY)
		file_descriptors[i]->flags = flags;
	else file_descriptors[i]->flags = UFS_READ_WRITE;
	file_descriptors[i]->file = curr;
	file_descriptors[i]->block_pos = curr->block_list;
	file_descriptors[i]->mem_pos = (curr->block_list != NULL) \
				? curr->block_list->memory : NULL;
	curr->refs++;

	return i;
}

static struct block *
ufs_new_block() {
	struct block *b = NULL;
	b = calloc(1, sizeof(struct block));
	if (b == NULL) return NULL;
	b->memory = calloc(BLOCK_SIZE, sizeof(char));
	if (b->memory == NULL) {
		free(b);
		return NULL;
	}
	return b;
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
	struct filedesc *desc = NULL;
	struct file *f = NULL;
	struct block *curr = NULL;
	char * pos = NULL;
	int buf_size = size;
	if (fd < 0 || fd > file_descriptor_capacity || \
	file_descriptors[fd] == NULL) {
		ufs_errno_code = UFS_ERR_NO_FILE;
		return -1;
	}
	desc = file_descriptors[fd];
	if (!(desc->flags & UFS_WRITE_ONLY) &&
	!(desc->flags & UFS_READ_WRITE)) {
		ufs_errno_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}
	if (size == 0) return 0;

	f = desc->file;
	curr = desc->block_pos;
	pos = desc->mem_pos;
	while (buf_size > 0) {
		if (pos == NULL) {
			if (curr == NULL) {
				if (f->blocks >= MAX_FILE_SIZE / BLOCK_SIZE || \
				(curr = ufs_new_block()) == NULL) {
					ufs_errno_code = UFS_ERR_NO_MEM;
					return -1;
				}
				f->blocks++;
				if (f->block_list == NULL)
					f->block_list = curr;
				if (f->last_block != NULL) {
					f->last_block->next = curr;
					curr->prev = f->last_block;
				}
				f->last_block = curr;
			}
			pos = curr->memory;
		}
		if (BLOCK_SIZE - (pos - curr->memory) > buf_size) {
			memcpy(pos, buf, buf_size);
			pos += buf_size;
			buf += buf_size;
			if (curr->occupied < pos - curr->memory)
				curr->occupied = pos - curr->memory;
			buf_size = 0;
		} else {
			int offset = BLOCK_SIZE - (pos - curr->memory);
			memcpy(pos, buf, offset);
			buf_size -= offset;
			buf += offset;
			curr->occupied = BLOCK_SIZE;
			curr = curr->next;
			if (curr) pos = curr->memory;
			else pos = NULL;
		}
	}
	desc->block_pos = curr;
	desc->mem_pos = pos;

	return size;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
	struct filedesc *desc = NULL;
	struct file *f = NULL;
	struct block *curr = NULL;
	char *pos = NULL;
	int buf_size = size;
	int offset = 0;
	if (fd < 0 || fd > file_descriptor_capacity || \
	file_descriptors[fd] == NULL) {
		ufs_errno_code = UFS_ERR_NO_FILE;
		return -1;
	}
	desc = file_descriptors[fd];
	if (!(desc->flags & UFS_READ_ONLY) &&
	!(desc->flags & UFS_READ_WRITE)) {
		ufs_errno_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}
	if (size == 0) return 0;
	f = desc->file;
	curr = desc->block_pos;
	pos = desc->mem_pos;
	if (curr == NULL && f->block_list != NULL)
		curr = f->block_list;
	while (buf_size > 0) {
		if (pos == NULL) {
			if (curr == NULL) break;
			pos = curr->memory;
		}
		offset = curr->occupied - (pos - curr->memory);
		if (offset == 0) {
			curr = curr->next;
			if (curr) pos = curr->memory;
			else pos = NULL;
			continue;
		}
		if (offset > buf_size) {
			memcpy(buf, pos, buf_size);
			pos += buf_size;
			buf += buf_size;
			buf_size = 0;
		} else {
			memcpy(buf, pos, offset);
			buf += offset;
			buf_size -= offset;
			if (curr->occupied == BLOCK_SIZE) {
				curr = curr->next;
				if (curr != NULL) pos = curr->memory;
				else pos = NULL;
			} else pos += offset;
		}
	}
	desc->block_pos = curr;
	desc->mem_pos = pos;

	if (buf_size > 0) return size - buf_size;
	return size;
}

static void
ufs_real_delete(struct file *f) {
	struct block *curr_block = f->block_list;
	while (curr_block != NULL) {
		f->block_list = curr_block->next;
		if (curr_block->memory != NULL)
			free(curr_block->memory);
		free(curr_block);
		curr_block = f->block_list;
	}
	if (f->prev) f->prev->next = f->next;
	else file_list = f->next;
	if (f->next) f->next->prev = f->prev;
	if (f->name != NULL) free(f->name);
	free(f);
}

int
ufs_close(int fd)
{
	struct file *f = NULL;
	if (fd < 0 || fd >= file_descriptor_capacity || \
	file_descriptors[fd] == NULL) {
		ufs_errno_code = UFS_ERR_NO_FILE;
		return -1;
	}
	f = file_descriptors[fd]->file;
	f->refs--;
	free(file_descriptors[fd]);
	file_descriptor_count--;
	file_descriptors[fd] = NULL;
	if (f->refs == 0 && f->deleted)
		ufs_real_delete(f);
	return UFS_ERR_NO_ERR;
}

int
ufs_delete(const char *filename) {
	int i = 0;
	struct file *curr = file_list;
	while (curr != NULL) {
		if (strcmp(curr->name, filename) == 0) break;
		curr = curr->next;
	}
	if (curr == NULL) {
		ufs_errno_code = UFS_ERR_NO_FILE;
		return -1;
	}
	if (curr->refs > 0) {
		curr->deleted = 1;
		return UFS_ERR_NO_ERR;
	}
	ufs_real_delete(curr);
	return UFS_ERR_NO_ERR;
}

static void
free_block_list(struct block *block_list) {
	struct block *curr = block_list;
	while (curr != NULL) {
		block_list = curr->next;
		if (curr->memory != NULL)
			free(curr->memory);
		free(curr);
		curr = block_list;
	}
}

void
ufs_free(void) {
	int i = 0;
	struct file *curr = file_list;
	while (curr != NULL) {
		file_list = file_list->next;
		free_block_list(curr->block_list);
		free(curr);
		curr = file_list;
	}
	if (file_descriptors != NULL) {
		for (i = 0; i < file_descriptor_capacity; i++) {
			if (file_descriptors[i] != NULL)
				free(file_descriptors[i]);
		}
		free(file_descriptors);
	}
}

void ufs_print(void) {
	struct file *curr = file_list;
	struct block *curr_block = NULL;
	int max_print = 20;
	int i = 0;
	printf("------UserFS------\\\n");
	printf("FILES:\n");
	while (curr != NULL) {
		curr_block = curr->block_list;
		printf("file_%d:\n", i + 1);
		printf("\tname: %s\n\trefs: %d\n", curr->name, curr->refs);
		while (curr_block != NULL) {
			printf("\t[%d]\n", curr_block->occupied);
			write(1, "\t", 1);
			write(1, curr_block->memory, curr_block->occupied);
			write(1, "\n", 1);
			curr_block = curr_block->next;
		}
		curr = curr->next;
		i++;
	}
	if (file_descriptors != NULL) {
		printf("FD: %d %d\n", file_descriptor_count, \
					file_descriptor_capacity);
		for (i = 0; i < file_descriptor_capacity; i++) {
			if (file_descriptors[i] == NULL) {
				printf("\t- : -\n");
			} else {
				printf("\t%d : %s\n", i, \
				file_descriptors[i]->file->name);
			}
			if (i == max_print) break;
		}
		if (i < file_descriptor_capacity)
			printf("...\n");
	}
	printf("__________________/\n\n");
}
