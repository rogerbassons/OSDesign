/*
   Simple File System

   This code is derived from function prototypes found /usr/include/fuse/fuse.h
   Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
   His code is licensed under the LGPLv2.

*/

#include "params.h"
#include "block.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"

/*
+---------------------------------------------------------------------------------+
|---------------------------------------------------+  +--------------------------|
||             ||            ||            ||3 Block|  |      ||      |  |       ||
||  0 Block    ||   1 Block  ||  2 Block   || first |  | last || first|  | last  ||
||             ||            ||            ||       |  |      ||      |  |       ||
||  Superblock || inodeList  ||  dataList  || inode |..| inode|| data |..| data  ||
||             ||            ||            ||       |  |      ||      |  |       ||
||             ||            ||            ||       |  |      || block|  | block ||
||             ||            ||            ||       |  |      ||      |  |       ||
|---------------------------------------------------+  +--------------------------|
+---------------------------------------------------------------------------------+

Superblock contains information about the FS. Where all info starts.

inodeList is a list that represents free/used inodes. This list doesn't contain
any pointers to actual inodes. Iff attribute free of nth node of this list is 1 
then the nth inode on the actual inode list(starting at 3rd Block) is free.

The same can be said for dataList.

The number of space/blocks used to store actual inodes is calculated on init and
to make it simple we choose the maximum number of blocks that the disk can hold 
minus 3 blocks (the first three blocks used to do the bookkeeping).

*/

void log_fuse_context(struct fuse_context *);

#define DIRECTORY 0
#define FILE      1

#define FS_SIZE    100000000

typedef struct {
	void *inodeList;
	void *dataList;

	void *inodeStart;
	void *dataStart;
} superblock;

typedef struct fsNode {
	unsigned free;
	struct fsNode *next;
} fsNode;

typedef struct inode {
	unsigned type;
	char *name;
	void *data;
	struct inode *nextInode;
	mode_t    st_mode;        /* File type and mode */
	nlink_t   st_nlink;       /* Number of hard links */
	uid_t     st_uid;         /* User ID of owner */
	gid_t     st_gid;         /* Group ID of owner */
	off_t     st_size;        /* Total size, in bytes */
	struct timespec st_atim;  /* Time of last access */
	struct timespec st_mtim;  /* Time of last modification */
} inode;

superblock *getSuperblock()
{
	return (superblock *) SFS_DATA->diskfile;
}

void initializeFreeList(void *list, unsigned size, int maxNumber)
{

	fsNode *n = list;
	unsigned s = sizeof(fsNode);

	unsigned i = 0;
	fsNode *prev = NULL;
	while (s <= size && (maxNumber == -1 || i < maxNumber)) {
		n->free = 1;
		n->next = NULL;

		if (prev != NULL)
			prev->next = n;

		i++;
		s += sizeof(fsNode);
		n += sizeof(fsNode);
	}

	if (s <= size && i < maxNumber)
		log_msg("\nCouldn't create free list for all available inodes\n");

}

int getFree(void *start)
{
	fsNode *n = start;

	int i = 0;
	while (!n->free && n->next != NULL) {
		i++;
		n = n->next;
	}

	if (!n->free)
		return -1;

	return i;
}

inode *getFreeInode()
{
	superblock *s = getSuperblock();

	void *start = s->inodeList;

	unsigned i = getFree(start);

	return s->inodeStart + sizeof(inode) * i;
}

void *getFreeBlock()
{
	superblock *s = getSuperblock();

	void *start = s->dataList;

	unsigned i = getFree(start);

	return s->dataStart + BLOCK_SIZE * i;
}

int newInode(unsigned type, char *name)
{
	inode *i = getFreeInode();

	i->type = type;
	i->name = name;
	i->nextInode = NULL;

	if (type == DIRECTORY) {

		i->st_mode = S_IFDIR;
		i->st_nlink = 2;
		i->data = NULL;

	} else {

		i->data = getFreeBlock();
		i->st_mode = S_IFREG;
		i->st_nlink = 1;
		i->st_size = BLOCK_SIZE;

	}
	i->st_mode = i->st_mode | S_IRWXU | S_IRWXG | S_IRWXO;

	i->st_uid = getuid(); 
	i->st_gid = getgid();        

	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);

	i->st_atim = t;
	i->st_mtim = t;

	return 0;
}


inode *getRootInode()
{
	// root "/" inode is always the first one
	return (inode *) getSuperblock()->inodeList;
}

inode *findNextInode(inode *i, char *s)
{
	if (i->type != DIRECTORY)
		return NULL;

	inode *res = i->nextInode;
	while (res != NULL && strcmp(res->name, s) != 0) 
		res = res->nextInode;

	return res;
}

inode *findPath(char *path)
{
	char *s = strtok(path, "/");
	inode *i = getRootInode();

	while (s != NULL) {
		i = findNextInode(i, s);
		if (i == NULL) 
			return NULL; // not found

		s = strtok(NULL, "/");
	}

	return i;
}

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h


/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
void *sfs_init(struct fuse_conn_info *conn)
{
	fprintf(stderr, "in bb-init\n");
	log_msg("\nsfs_init()\n");

	log_conn(conn);
	log_fuse_context(fuse_get_context());

	char **disk = &(SFS_DATA->diskfile);

	*disk = malloc(sizeof(FS_SIZE));
	if (*disk == NULL) {
		log_msg("\n Error: can't malloc file system\n");
		return NULL;
	}
	
	unsigned nInodes = (FS_SIZE - BLOCK_SIZE * 3) / BLOCK_SIZE;
	unsigned nBlocks = (FS_SIZE - BLOCK_SIZE * (3 + nInodes)) / BLOCK_SIZE;

	superblock s;
	s.inodeList = *disk + BLOCK_SIZE;
	s.dataList = s.inodeList + BLOCK_SIZE;
	s.inodeStart = s.dataList + BLOCK_SIZE;
	s.dataStart = s.inodeStart + nInodes * sizeof(inode);
	memcpy(*disk, &s, sizeof(superblock));

	initializeFreeList(s.inodeList, BLOCK_SIZE, nInodes);
	initializeFreeList(s.dataList, BLOCK_SIZE, nBlocks);

	newInode(DIRECTORY, "/");

	return SFS_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void sfs_destroy(void *userdata)
{
	log_msg("\nsfs_destroy(userdata=0x%08x)\n", userdata);
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int sfs_getattr(const char *path, struct stat *statbuf)
{
	char fpath[PATH_MAX];
	strcpy(fpath, path);
	log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
			path, statbuf);

	inode *i = findPath(fpath);
	if (i == NULL) {
		log_msg("Can't find inode from path\n");
		return 1;
	}
	statbuf->st_mode = i->st_mode;
	statbuf->st_nlink = i->st_nlink;
	statbuf->st_uid = i->st_uid;
	statbuf->st_gid = i->st_gid;
	statbuf->st_size = i->st_size;
	statbuf->st_atime = i->st_atim.tv_sec;
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	i->st_atim = t;
	statbuf->st_mtime = i->st_mtim.tv_sec;
	
	return 0;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int sfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	int retstat = 0;
	log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
			path, mode, fi);


	return retstat;
}

/** Remove a file */
int sfs_unlink(const char *path)
{
	int retstat = 0;
	log_msg("sfs_unlink(path=\"%s\")\n", path);


	return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int sfs_open(const char *path, struct fuse_file_info *fi)
{
	int retstat = 0;
	log_msg("\nsfs_open(path\"%s\", fi=0x%08x)\n",
			path, fi);


	return retstat;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int sfs_release(const char *path, struct fuse_file_info *fi)
{
	int retstat = 0;
	log_msg("\nsfs_release(path=\"%s\", fi=0x%08x)\n",
			path, fi);


	return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
int sfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	int retstat = 0;
	log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
			path, buf, size, offset, fi);

	return retstat; 
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int sfs_write(const char *path, const char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi)
{
	int retstat = 0;
	log_msg("\nsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
			path, buf, size, offset, fi);


	return retstat;
}


/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode)
{
	int retstat = 0;
	log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",
			path, mode);


	return retstat;
}


/** Remove a directory */
int sfs_rmdir(const char *path)
{
	int retstat = 0;
	log_msg("sfs_rmdir(path=\"%s\")\n",
			path);


	return retstat;
}


/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int sfs_opendir(const char *path, struct fuse_file_info *fi)
{
	int retstat = 0;
	log_msg("\nsfs_opendir(path=\"%s\", fi=0x%08x)\n",
			path, fi);


	return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int sfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
		struct fuse_file_info *fi)
{
	char fpath[PATH_MAX];
	strcpy(fpath, path);

	if(filler(buf, ".", NULL, 0))
		log_msg("Filler buffer is full\n"); 
	if (filler(buf, "..", NULL, 0))
		log_msg("Filler buffer is full\n");

	inode *i = findPath(fpath);
	if (i->type != DIRECTORY)
		return 1;

	i = i->nextInode;
	while (i != NULL) {
		if (filler(buf, i->name, NULL, 0))
			log_msg("Filler buffer is full\n");
		i = i->nextInode;
	}

	return 0;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int sfs_releasedir(const char *path, struct fuse_file_info *fi)
{
	int retstat = 0;


	return retstat;
}

struct fuse_operations sfs_oper = {
	.init = sfs_init,
	.destroy = sfs_destroy,

	.getattr = sfs_getattr,
	.create = sfs_create,
	.unlink = sfs_unlink,
	.open = sfs_open,
	.release = sfs_release,
	.read = sfs_read,
	.write = sfs_write,

	.rmdir = sfs_rmdir,
	.mkdir = sfs_mkdir,

	.opendir = sfs_opendir,
	.readdir = sfs_readdir,
	.releasedir = sfs_releasedir
};

void sfs_usage()
{
	fprintf(stderr, "usage:  sfs [FUSE and mount options] diskFile mountPoint\n");
	abort();
}

int main(int argc, char *argv[])
{
	int fuse_stat;
	struct sfs_state *sfs_data;

	// sanity checking on the command line
	if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
		sfs_usage();

	sfs_data = malloc(sizeof(struct sfs_state));
	if (sfs_data == NULL) {
		perror("main calloc");
		abort();
	}

	// Pull the diskfile and save it in internal data
	sfs_data->diskfile = argv[argc-2];
	argv[argc-2] = argv[argc-1];
	argv[argc-1] = NULL;
	argc--;

	sfs_data->logfile = log_open();

	// turn over control to fuse
	fprintf(stderr, "about to call fuse_main, %s \n", sfs_data->diskfile);
	fuse_stat = fuse_main(argc, argv, &sfs_oper, sfs_data);
	fprintf(stderr, "fuse_main returned %d\n", fuse_stat);

	return fuse_stat;
}
