#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <string.h>
#include <errno.h>

static const char* hello_str = "Hello, FUSE!\n";
static const char* hello_path = "/hello";

/* Получение атрибутов файлов */
static int hello_getattr(const char* path, struct stat* stbuf,
	struct fuse_file_info* fi)
{
	memset(stbuf, 0, sizeof(struct stat));

	if (strcmp(path, "/") == 0)
	{
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}
	else if (strcmp(path, hello_path) == 0)
	{
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(hello_str);
		return 0;
	}

	return -ENOENT;
}

/* Чтение каталога "/" */
static int hello_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info* fi,
	enum fuse_readdir_flags flags)
{
	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0, static_cast<fuse_fill_dir_flags>(0));
	filler(buf, "..", NULL, 0, static_cast<fuse_fill_dir_flags>(0));
	filler(buf, "hello", NULL, 0, static_cast<fuse_fill_dir_flags>(0));

	return 0;
}

/* Чтение файла */
static int hello_read(const char* path, char* buf, size_t size,
	off_t offset, struct fuse_file_info* fi)
{
	size_t len;

	if (strcmp(path, hello_path) != 0)
		return -ENOENT;

	len = strlen(hello_str);
	if (offset >= len)
		return 0;

	if (offset + size > len)
		size = len - offset;

	memcpy(buf, hello_str + offset, size);
	return size;
}

static struct fuse_operations hello_ops = {
	.getattr = hello_getattr,
	.read = hello_read,
	.readdir = hello_readdir,
};

int main(int argc, char* argv[])
{
	return fuse_main(argc, argv, &hello_ops, NULL);
}
