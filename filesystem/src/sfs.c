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

#define DIRECTORY 0
#define FILE 1
#define NUM_DIRECT_POINTERS 49
#define FS_SIZE 16777216
#define MAGIC 4511000 //magic number that indicates whether the fs is ours
#define MAX_FNAME 24

typedef struct {
  int imap;
  int iList;
  int dmap;
  int dList;
  int magicnum; //will tell us if the current fs is what we expect
} superblock;

typedef struct inode {
  //First, the fields required to exist for getattr to work:
  mode_t st_mode; //basically doubles as type?
  ino_t st_ino;
  uid_t st_uid;
  gid_t st_gid;
  time_t st_atime;
  time_t st_mtime;
  time_t st_ctime;
  //char type;
  char opened;
  char used;
  int directp[NUM_DIRECT_POINTERS];
  int indirectp;
} inode;

typedef struct mydir{
  //each dirblock can hold 16 files, given that max name length is 28.
  ino_t inums[16];
  char fnames[16][MAX_FNAME];
} mydir;

int reserveFreeData(){
  //returns a blocknum
  log_msg("\nreserveFreeData running.\n");

  char spbuff[512];
  if( 0 > block_read(0, (void *)spbuff) ){
    log_msg("\nreserveFreeData: block_read(0) failed.\n");
  }
  superblock * s = (superblock *)spbuff;
  int bnum = s->dmap;
  char dmap[512];

  while(bnum < s->iList){
    if(block_read(bnum, (void *)dmap) < 0){
      log_msg("\nreserveFreeData: block_read(%d) failed.\n", bnum);
    }
    int j;
    for(j = 0; j < 512; j++){
      if(dmap[j] == 0){
        dmap[j] = 1;
        if(block_write(bnum, (const void *)dmap) != 512){
          log_msg("\nrserveFreeData: block_write(%d) failed.\n",bnum);
          return -1;
        }
        return (512 * (bnum - s->dmap)) + j + s->dList;
      }
    }
    bnum++;
  }
  return -1;
}

void initDir(int isroot, int blocknum, int selfinum, int parentinum){
  log_msg("\ninitDir running.\n");
  mydir d;
  block_read(blocknum, &d);
  d.inums[0] = selfinum;
  d.fnames[0][0] = '.';
  d.fnames[0][1] = '\0';
  if(isroot == 0){
    d.inums[1] = parentinum;
    d.fnames[1][0] = '.';
    d.fnames[1][1] = '.';
    d.fnames[1][2] = '\0';
  }
  else{
    d.inums[1] = -1;
  }
  int i;
  for(i = 2; i < 16; i++){
    d.inums[i] = -1;
  }

  block_write(blocknum, (const void *)&d);

  return;
}

int navPath(inode *par, inode *targ, const char *path){
  //The parent directory's inode will by par, and the target directory or file will be targ.
  log_msg("\nnavPath running.\n");
  char sbbuff[512];
  block_read(0, (void *)sbbuff);
  superblock *s = (superblock *)sbbuff;
  char paribuff[512];
  char targibuff[512];
  block_read(s->iList, (void *)paribuff);
  block_read(s->iList, (void *)targibuff);
  inode *p = (inode *)paribuff;
  inode *t = (inode *)targibuff;
  log_msg("\nnavPath: s->iList is %d. p->st_ino = %d.\n", s->iList, p->st_ino);


  mydir m;
  char currname[MAX_FNAME];
  char flag = 0;
  if(strlen(path) == 1 || strlen(path) == 0){
    *par = *p;
    *targ = *t;
    return 0;
  }
  int i = 1;
  while(flag == 0){
    int j = 0;
    *p = *t;
    while(path[i] != '/' && path[i] != '\0'){ //make sure path won't every end in a '/'
      currname[j] = path[i];
      i++;
      j++;
    }
    currname[j] = '\0';

    int q;
    int success = 0;
    for(q = 0; q < NUM_DIRECT_POINTERS; q++){
      int k;
      if(t->directp[q] == 0){
        continue;
      }
      block_read(t->directp[q], (void *)&m);
      for(k = 0; k < 16; k++){
        if(m.inums[k] != -1 && strcmp(m.fnames[k], currname) == 0){
          success = 1;
          //update t
          block_read( (m.inums[k]/2)+s->iList , (void *)targibuff);
          if(m.inums[k]%2 == 0){
            p = (inode *)targibuff;
          } else {
            p = (inode *)(targibuff + 256);
          }
          break;
        }
      }
      if(success == 1){break;}
    }
    if(path[i] == '\0' || (path[i] == '/' && path[i+1] == '\0') ){
      if(success == 1){flag = 2;}
      else{flag = 1;}
    }
  }

  //Now, we have to write the current inode info to the input pointers and return, even if the search failed.
  *par = *p;
  *targ = *t;
  if(flag == 1){return -1;}
  return 0;
}

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//

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

  /*At 16777216 bytes, with a block size of 512, we have 32768 blocks. Each inode has a size of 1/2 block, or 256 bytes exactly.
    The imap and data map are going to be arrays of chars. We'll have 2048 inodes, which will require 1024 blocks to store their
    data, and 4 blocks to store the imap. One block will be reserved for the superblock. That leaves 31739 blocks for data blocks
    and the data map (dmap). We'll use 60 blocks for the dmap (This means we'll have a few inaccessible data blocks, but we'll
    never call block_write for them, so we'll effectively have a slightly smaller filesystem).
  */

  fprintf(stderr, "in bb-init\n");
  log_msg("\nsfs_init()\n");
  log_msg("\nSize of inode currently: %d\n", sizeof(inode));
  void *p;
  log_msg("\nSize of int: %d. Size of void pointer: %d. Size of mydir: %d.\n", sizeof(int), sizeof(p), sizeof(mydir));
  log_conn(conn);
  log_fuse_context(fuse_get_context());

  disk_open(SFS_DATA->diskfile);

  int ret;
  char sbbuff[512];
  superblock * sbp = (superblock *)sbbuff;
  block_read(0, (void*)sbbuff);

  struct stat root_stat;
  lstat(SFS_DATA->diskfile, &root_stat);
  if(root_stat.st_size == 0 || sbp->magicnum != MAGIC){
    log_msg("\nDoing init stuffs...\n");
    //Do init stuff
    sbp->magicnum = MAGIC;
    sbp->imap = 1; //imap starts at block 1
    sbp->dmap = 5; //dmap starts at block 5
    sbp->iList = 65; //inodes start at block 65
    sbp->dList = 1089; //data blocks start at block 1089
    ret = block_write(0, (void *)sbbuff);
    if(512 != ret){
      log_msg("\nInit: writing block 0 error. Ret was %d.\n", ret);
    }

    int i;
    //Setting up freelists. That's about the only initialization I think is necessary, aside from setting up root.
    char temp[512] = {0};
    for(i = 1; i < sbp->iList; i++){
      ret = block_write(i, (const void *)temp);
      if(ret < 0){
	log_msg("\nInit: block_write at block %d failed. Return value: %d\n", i, ret);
      }
    }

    //root initialization:
    char ibuff[512];
    //block_read(i, (void *)ibuff);
    inode * rooti = (inode *)ibuff;
    rooti->st_mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH; //can be read/written by user, groups, and other
    rooti->st_ino = 0;
    rooti->st_uid = geteuid(); //Real or effective uid usage? I.e., getuid or geteuid?
    rooti->st_gid = getegid();
    rooti->st_atime = time(NULL);
    rooti->st_mtime = rooti->st_atime;
    rooti->st_ctime = rooti->st_atime;
    rooti->opened = 0;
    rooti->used = 0; //Just matches up with the imap
    int k;
    for(k = 1; k < NUM_DIRECT_POINTERS; k++){
      rooti->directp[k] = 0;
    }
    rooti->directp[0] = reserveFreeData();
    if(rooti->directp[0] == -1){
      log_msg("\nInit: reservefreeData returned -1.\n");
    }

    initDir(1, rooti->directp[0], rooti->st_ino, rooti->st_ino);
    rooti->indirectp = 0;
    block_write(i, (const void *)ibuff);
  }
  else{
    if(sbp->magicnum != MAGIC){
      log_msg("\nUnexpected value at magicnum. Wrong fs?\n");
    }
  }

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
  int retstat = 0;

  inode par;
  inode targ;
  if(navPath(&par, &targ, path) == -1){
    log_msg("\ngetattr: navPath returned -1.\n");
  }

  statbuf->st_mode = targ.st_mode;
  statbuf->st_ino = targ.st_ino;
  statbuf->st_uid = targ.st_uid;
  statbuf->st_gid = targ.st_gid;
  statbuf->st_atime = targ.st_atime;
  statbuf->st_mtime = targ.st_mtime;
  statbuf->st_ctime = targ.st_ctime;
  log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
          path, statbuf);
  log_msg("\ngetattr: inum of targ: %d. Uid of targ: %d. Mode of targ: %d.\n", targ.st_ino, targ.st_uid, targ.st_mode);

    return retstat;
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
  int retstat = 0;

  //const char direntbuff[1000];
  struct stat currstat;
  //struct dirent *dirp = (dirent *)dbuff;
  inode targ;
  inode par;
  if(navPath(&par, &targ, path) == -1){
    log_msg("\ngetattr: navPath returned -1.\n");
  }
  if(S_ISDIR(targ.st_mode) == 0){
    log_msg("\nreaddir: Given input is not a directory.\n");
    return -1;
  }

  char mydirbuff[512];
  mydir *m = (mydir *)mydirbuff;
  int i;
  for(i = 0; i < NUM_DIRECT_POINTERS; i++){
    if(targ.directp[i] == 0){
      continue;
    }
    block_read(targ.directp[i], mydirbuff);
    int j;
    for(j = 0; j < 16; j++){
      if(m->inums[j] != -1){
	//dirp->d_ino = m->inums[j];
	//strcpy(dirp->d_name, m->fname);
	if(0 != filler(buf, m->fnames[j], &currstat, 0)){
	  return 0;
	}
      }
    }
  }

  return retstat;
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
