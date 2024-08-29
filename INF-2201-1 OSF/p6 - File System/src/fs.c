#include "fs.h"
#include "fstypes.h"

#ifdef LINUX_SIM
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#endif /* LINUX_SIM */

#include "block.h"
#include "common.h"
#include "fs_error.h"
#include "inode.h"
#include "kernel.h"
#include "superblock.h"
#include "thread.h"
#include "util.h"

#define INODE_TABLE_ENTRIES 20
#define BITMAP_ENTRIES 256
#define MAGIC_NUM 0x420
#define FREE_BLK -1
#define SIZEX 50

/* Block index of superblock */
int superblock_blk;
/* Global superblock */
static struct mem_superblock mem_superblock;

/* Memory inode table */
static char mem_inode_bmap[INODE_TABLE_ENTRIES];
static struct mem_inode mem_inode_table[INODE_TABLE_ENTRIES];

/* Char array with 0, used to clean */
static char zero_block[BLOCK_SIZE];
static char zero_dirent[sizeof(struct dirent)];
static char zero_inode[sizeof(struct disk_inode)];

/* Bitmaps */
static char inode_bmap[BITMAP_ENTRIES];
static char dblk_bmap[BITMAP_ENTRIES];

int	printf (const char *__restrict, ...);
static int get_free_entry(unsigned char *bitmap);
static int free_bitmap_entry(int entry, unsigned char *bitmap);
static inode_t path2inode(char *name);
static blknum_t ino2blk(inode_t ino);
static blknum_t idx2blk(int index);

/* Extract every directory name out of a path. This consist of replacing every '/' with '\0'.
 * Return number of directories in path. */
static int parse_path(char *path, char *argv[SIZEX]) {

	char *s = path;
	int argc = 0;

	/* if absolute path */
	if(*s == '/') {
		s++;
		argv[argc++] = "/";
	}

	if(*s != '\0')
		argv[argc++] = s;

	while (*s != '\0') {
		if (*s == '/') {
			*s = '\0';
			argv[argc++] = (s + 1);
		}

		s++;
	}

	return argc;
}

static int valid_name(char* filename) {

	/* Check if filename is to long */
	if(strlen(filename) > MAX_FILENAME_LEN)
		return FSE_NAMETOLONG;

	/* Check if filename contains '/' */
	char *tmp = (char*)filename;
	while (*tmp != '\0') {
		if (*tmp == '/')
			return FSE_INVALIDNAME;
		tmp++;
	}

	/* Check if filename is "." or ".." */
	char self[] = ".";
	char parent[] = "..";
	if( (same_string(filename, self) == TRUE) || (same_string(filename, parent) == TRUE) )
		return FSE_INVALIDNAME;
	

	return 1;
}

static int write_superblock(void) {
	return block_modify((os_size + 2) + superblock_blk, sizeof(struct disk_superblock) * 0, sizeof(struct disk_superblock), &mem_superblock.d_super);
}

static int read_superblock(void) {
	return block_read_part((os_size + 2) + superblock_blk, sizeof(struct disk_superblock) * 0, sizeof(struct disk_superblock), &mem_superblock.d_super);
}

static int write_bitmap(void) {
	int ret = block_modify((os_size + 2) + mem_superblock.d_super.root_bmap, BITMAP_ENTRIES * 0, BITMAP_ENTRIES, mem_superblock.ibmap);
	if(ret < 0)
		return ret;
	return block_modify((os_size + 2) + mem_superblock.d_super.root_bmap, BITMAP_ENTRIES * 1, BITMAP_ENTRIES, mem_superblock.dbmap);
}

static int read_bitmap(void) {
	int ret = block_read_part((os_size + 2) +  mem_superblock.d_super.root_bmap, BITMAP_ENTRIES * 0, BITMAP_ENTRIES, mem_superblock.ibmap);
	if(ret < 0)
		return ret;
	return block_read_part((os_size + 2) +  mem_superblock.d_super.root_bmap, BITMAP_ENTRIES * 1, BITMAP_ENTRIES, mem_superblock.dbmap);
}

static int write_inode(struct disk_inode *disk_inode, inode_t inode_num) {

	int block = inode_num / (BLOCK_SIZE / sizeof(struct disk_inode)); 	// Block to write
	int offset = inode_num % (BLOCK_SIZE / sizeof(struct disk_inode));	// Offset to start write

	return block_modify((os_size + 2) + mem_superblock.d_super.root_inode + block,  sizeof(struct disk_inode) * offset, sizeof(struct disk_inode), disk_inode);
}

static int read_inode(struct disk_inode *disk_inode, inode_t inode_num) {

	int block = inode_num / (BLOCK_SIZE / sizeof(struct disk_inode));	// Block to read
	int offset = inode_num % (BLOCK_SIZE / sizeof(struct disk_inode));	// Offset to start read

	return block_read_part((os_size + 2) + mem_superblock.d_super.root_inode + block, sizeof(struct disk_inode) * offset, sizeof(struct disk_inode), disk_inode);
}

static int write_dirent(struct disk_inode *disk_inode, inode_t inode, char *name) {

	/* Declare directory entry */
	struct dirent dirent;
	strcpy(dirent.name, name);
	dirent.inode = inode;

	/* Write directory entry into datablock */
	int ret = helper_read_write(block_modify, disk_inode, disk_inode->size, sizeof(struct dirent), (char*)&dirent);
	if(ret < 0)
		return FSE_ADDDIR;

	/* Update size in disk inode */
	disk_inode->size += sizeof(struct dirent);

	return 1;
}

static int remove_dirent(struct disk_inode *disk_inode, char *remove_name) {

	/* Move last directory entry into slot where the removed directory entry resided.
	 * Only necessary if there is more than 3 directory entries. */
	if( disk_inode->size > (int)(sizeof(struct dirent) * 3) ) {

		int offset = 0;
		struct dirent dirent;
		/* Find offset for removed directory entry */
		while (1) {
			/* Get directory entry */
			helper_read_write(block_read_part, disk_inode, offset, sizeof(struct dirent), (char*)&dirent);

			if(same_string(dirent.name, remove_name))
				break;
			offset += sizeof(struct dirent);
		}

		/* Get last direcotry entry */
		helper_read_write(block_read_part, disk_inode, (disk_inode->size - sizeof(struct dirent)), sizeof(struct dirent), (char*)&dirent);

		/* Move last directory entry */
		helper_read_write(block_modify, disk_inode, offset, sizeof(struct dirent), (char*)&dirent);
	}
			
	/* Clean last directory entry slot */
	helper_read_write(block_modify, disk_inode, (disk_inode->size - sizeof(struct dirent)), sizeof(struct dirent), zero_dirent);

	/* Check if parent inode can free datablock */
	int cur_blk = disk_inode->size / BLOCK_SIZE;
	int new_blk = (disk_inode->size - sizeof(struct dirent)) / BLOCK_SIZE;
	if(cur_blk > new_blk) {
		free_bitmap_entry(disk_inode->direct[cur_blk], mem_superblock.dbmap);
		disk_inode->direct[cur_blk] = FREE_BLK;

		/* Update superblock */
		mem_superblock.d_super.ndata_blks--;
		write_superblock();
		write_bitmap();
	}

	/* Update size for parent inode */
	disk_inode->size -= sizeof(struct dirent);

	return 1;
}

static int open_inode(inode_t inode_num) {

	/* Check if inode is open */
	for(int i = 0; i < INODE_TABLE_ENTRIES; i++)
		if(mem_inode_table[i].inode_num == inode_num) {
			mem_inode_table[i].open_count++;
			return i;
		}

	/* Get free slot in memory inode table */
	int entry = get_free_entry((unsigned char *)mem_inode_bmap);
	int ret = read_inode(&mem_inode_table[entry].d_inode, inode_num);
	if(ret < 0)
		return -1;

	/* Init memory inode values */
	mem_inode_table[entry].inode_num = inode_num;
	mem_inode_table[entry].open_count = 1;
	mem_inode_table[entry].dirty = 0;
	mem_inode_table[entry].pos = 0;

	return entry;
}

static int close_inode(int entry) {

	mem_inode_table[entry].open_count--;

	/* Check if inode is truly closed */
	if(mem_inode_table[entry].open_count == 0) {

		/* Update inode if it is dirty */
		if(mem_inode_table[entry].dirty == 1)
			write_inode(&mem_inode_table[entry].d_inode, mem_inode_table[entry].inode_num);

		mem_inode_table[entry].inode_num = FREE_BLK;
		return free_bitmap_entry(entry, (unsigned char *)mem_inode_bmap);
	}

	return FSE_COUNT;
}

static int helper_read_write(int (*operation)(int, int, int, void*), struct disk_inode *disk_inode, int offset, int size, char *buffer) {
	
	int new_blk = (offset + size) / (BLOCK_SIZE + 1);

	/* Check If read/write will span over to not acquired block */
	if(disk_inode->direct[new_blk] == FREE_BLK)
		return FSE_INVALIDBLOCK;

	/* Calculate block to start read/write */
	int cur_blk = 0;
	while(offset >= BLOCK_SIZE) {
		offset -= BLOCK_SIZE;
		cur_blk++;
	}

	/* Calculate multiple read/write offsets */
	int rw_offset[ (new_blk - cur_blk) + 1 ];
	rw_offset[0] = offset;
	for(int i = 1; i < (new_blk - cur_blk) + 1; i++)
		rw_offset[i] = 0;

	/* Calculate multiple read/write sizes */
	int rw_size[ (new_blk - cur_blk) + 1 ];
	for(int i = 0; i < (new_blk - cur_blk) + 1; i++) {
		if( (rw_offset[i] + size) > BLOCK_SIZE )
			rw_size[i] = BLOCK_SIZE - rw_offset[i];
		else
			rw_size[i] = size;
		size -= rw_size[i];
	}

	/* Read/write part of block(s) */
	int ret;
	ret = operation((os_size + 2) + disk_inode->direct[cur_blk], rw_offset[0], rw_size[0], buffer);
	if(ret < 0)
		return FSE_INVALIDBLOCK;
	for(int i = 1; i < (new_blk - cur_blk) + 1; i++) {
		ret = operation((os_size + 2) + disk_inode->direct[cur_blk + i], rw_offset[i], rw_size[i], &buffer[rw_size[i-1]]);
		if(ret < 0)
			return FSE_INVALIDBLOCK;
	}

	return 1;
}

static int acquire_datablock(struct disk_inode *disk_inode, inode_t inode, int size) {
	int idx;

	/* Check if inode need new datablock */
	int cur_blk = disk_inode->size / BLOCK_SIZE;
	int new_blk = (disk_inode->size + size) / BLOCK_SIZE;
	if(cur_blk == new_blk)
		return 0;

	/* Search for free space in inode */
	for(idx = 0; idx < INODE_NDIRECT; idx++)
		if(disk_inode->direct[idx] == FREE_BLK)
			break;

	/* If there is no free space in inode */
	if(idx == INODE_NDIRECT)
		return FSE_FULL;

	/* Insert block index into inode */
	disk_inode->direct[idx] = get_free_entry((unsigned char *)mem_superblock.dbmap);
	block_write((os_size + 2) + disk_inode->direct[idx], zero_block);

	/* Increament number of datablocks and write */
	mem_superblock.d_super.ndata_blks++;
	write_inode(disk_inode, inode);
	write_superblock();
	write_bitmap();

	return 1;
}

static inode_t create_type(int type) {
	int ret;

	/* Get inode-entry index for directory.
	 * If there is no free entry, return  */
	inode_t inode_entry = (inode_t)get_free_entry((unsigned char *)mem_superblock.ibmap);
	if(inode_entry < 0)
		return inode_entry;
	write_inode((struct disk_inode*)zero_inode, inode_entry);	// Clear space on disk
	mem_superblock.d_super.ninodes++;

	/* Get datablock index for directory.
	 * If there is no free entry, retrun */
	int dblk_blk = get_free_entry((unsigned char *)mem_superblock.dbmap);
	if(dblk_blk < 0)
		return (inode_t)dblk_blk;
	block_write((os_size + 2) + dblk_blk, zero_block);			// Clear space on disk
	mem_superblock.d_super.ndata_blks++;

	/* Init disk inode for directory */
	struct disk_inode disk_inode;
	for(int i = 0; i < INODE_NDIRECT; i++)
		disk_inode.direct[i] = FREE_BLK;
	disk_inode.direct[0] = dblk_blk;
	disk_inode.nlinks = 0;
	disk_inode.size = 0;
	disk_inode.type = type;

	/* Check for type */
	if(type == INTYPE_DIR) {
		/* Write "." into datablock for inode */
		char self[] = ".";
		ret = write_dirent(&disk_inode, inode_entry, self);
		if(ret < 0)
			return (inode_t)-1;

		/* Write ".." into datablock for inode */
		char parent[] = "..";
		ret = write_dirent(&disk_inode, current_running->cwd, parent);
		if(ret < 0)
			return (inode_t)-1;
	}

	/* Write inode onto disk */
	ret = write_inode(&disk_inode, inode_entry);
	if(ret < 0)
		return (inode_t)-1;

	/* Write superblock onto disk */
	ret = write_superblock();
	if(ret < 0)
		return (inode_t)-1;

	 /* Write datablock bitmap and inode bitmap onto disk */
	ret = write_bitmap();
	if(ret < 0)
		return (inode_t)-1;

	return inode_entry;
}

/*
 * Exported functions.
 */
void fs_init(void) {

	block_init();

	/* Init char arrays with 0 */
	bzero(zero_block, sizeof(zero_block));
	bzero(zero_dirent, sizeof(zero_dirent));
	bzero(zero_inode, sizeof(zero_inode));

	/* Block index of superblock, should always be first block in filesystem */
	superblock_blk = 0;

	/* Current working direcotry as root */
	current_running->cwd = 0;

	/* Init superblock on memory */
	mem_superblock.dbmap = &dblk_bmap;
	mem_superblock.ibmap = &inode_bmap;
	mem_superblock.dirty = 0;
	
	/* Mark file descriptor table as "unused" */
	for (int i = 0; i < MAX_OPEN_FILES; i++)
		current_running->filedes[i].mode = MODE_UNUSED;

	/* Mark memory inodes as "free" */
	for(int i = 0; i < INODE_TABLE_ENTRIES; i++) {
		mem_inode_bmap[i] = 0;
		mem_inode_table[i].inode_num = -1;
	}

	read_superblock();
	
	/* Check if filesystem exists */
	if(mem_superblock.d_super.magic != MAGIC_NUM)
		fs_mkfs();
	else
		read_bitmap();

}

/*
 * Make a new file system.
 * Argument: kernel size
 */
void fs_mkfs(void) {
	int count = 0;	// number of datablocks acquired.

	/* Mark inodes and data blocks as "free" */
	for(int i = 0; i < BITMAP_ENTRIES; i++){
		inode_bmap[i] = 0;
		dblk_bmap[i] = 0;
	}

	/* Get block-index for superblock */
	superblock_blk = get_free_entry((unsigned char *)mem_superblock.dbmap);
	block_write((os_size + 2) + superblock_blk, zero_block);
	count++;

	/* Number of blocks for whole inode table */
	int num = BLOCK_SIZE / sizeof(struct disk_inode);	// Number of inodes per block.
	int blocks = (BITMAP_ENTRIES / num) + ((BITMAP_ENTRIES % num) != 0);	// Number of blocks for entire inode table, rounded up

	/* Get block-index for root of inode table */
	int inode_table_blk = get_free_entry((unsigned char *)mem_superblock.dbmap);
	block_write((os_size + 2) + inode_table_blk, zero_block);
	count++;

	/* Fill rest of inode table */
	for(int i = 0; i < (blocks - 1); i++) {
		int ent = get_free_entry((unsigned char *)mem_superblock.dbmap);
		block_write((os_size + 2) + ent, zero_block);
		count++;
	}

	/* Get block-index for block allocation bitmap */
	int block_alloc_blk = get_free_entry((unsigned char *)mem_superblock.dbmap);
	block_write((os_size + 2) + block_alloc_blk, zero_block);
	count++;

	/* Init superblock on disk */
	mem_superblock.d_super.magic = MAGIC_NUM;
	mem_superblock.d_super.max_filesize = INODE_NDIRECT * BLOCK_SIZE;
	mem_superblock.d_super.ndata_blks = count;
	mem_superblock.d_super.ninodes = 0;
	mem_superblock.d_super.root_inode = (blknum_t)inode_table_blk;
	mem_superblock.d_super.root_bmap = (blknum_t)block_alloc_blk;

	/* Create root directory */
	create_type(INTYPE_DIR);
}

/* Return index into file descriptor, update file descriptor */
int fs_open(const char *path, int mode) {

	int fd_idx, mode_bit, mem_entry_idx = 0;

	/* Search for unused file descriptor */
	for(fd_idx = 0; fd_idx < MAX_OPEN_FILES; fd_idx++)
		if( current_running->filedes[fd_idx].mode == MODE_UNUSED )
			break;

	/* Out of file descriptor table entries */
	if(fd_idx == MAX_OPEN_FILES)
		return FSE_NOMOREFDTE;

	/* Get inode for last name in path */
	inode_t inode = path2inode((char*)path);

	/* If mode read only, check if filename exists */
	mode_bit = mode & MODE_RDONLY;
	if(mode_bit == MODE_RDONLY) {
		if(inode < 0)
			return FSE_NOTEXIST;
		mem_entry_idx = open_inode(inode);
	}

	/* If mode create, create new file if name does not exist.
	 * If name exists, check if name is of type file. */
	mode_bit = mode & MODE_CREAT;
	if(mode_bit == MODE_CREAT)
	{
		/* File does not exist */
		if(inode < 0)
		{
			int ret;

			/* Check if filename is valid */
			ret = valid_name((char*)path);
			if(ret < 0)
				return ret;

			/* Get current working inode */
			struct disk_inode disk_inode;
			read_inode(&disk_inode, current_running->cwd);

			/* Check if current working directory need a new block for the new directory entry */
			ret = acquire_datablock(&disk_inode, current_running->cwd, (int)sizeof(struct dirent));
			if(ret < 0)
				return FSE_FULL;

			/* Init new file */
			inode_t file_inode = create_type(INTYPE_FILE);

			/* Write new directory entry, into current working directory */
			write_dirent(&disk_inode, file_inode, (char*)path);

			/* Update current working inode, since the new entry increases size */
			write_inode(&disk_inode, current_running->cwd);

			/* Open newly created file */
			mem_entry_idx = open_inode(file_inode);
		} 
		/* File exist */
		else {
			
			/* Get inode for found name, and check if inode is file */
			struct disk_inode disk_inode;
			read_inode(&disk_inode, inode);
			if(disk_inode.type != INTYPE_FILE)
				return FSE_INVALIDNAME;

			/* Open previously created file */
			mem_entry_idx = open_inode(inode);

			/* Update position to where file last left off */
			mem_inode_table[mem_entry_idx].pos = mem_inode_table[mem_entry_idx].d_inode.size;
		}
	}

	/* Init file descriptor */
	current_running->filedes[fd_idx].idx = mem_entry_idx;
	current_running->filedes[fd_idx].mode = mode;

	return fd_idx;
}

int fs_close(int fd) {

	/* Close inode in memory inode table */
	close_inode(current_running->filedes[fd].idx);
	current_running->filedes[fd].mode = MODE_UNUSED;

	return 0;
}

int fs_read(int fd, char *buffer, int size) {
	int read_size = 0;

	/* Index into global memory inode table */
	int idx = current_running->filedes[fd].idx;

	/* Check if caller have permission to read */
	int mode_bit = current_running->filedes[fd].mode & MODE_RDONLY;
	if(mode_bit != MODE_RDONLY)
		return 0;

	/* Read values for different types */
	switch(mem_inode_table[idx].d_inode.type) {
		case INTYPE_FILE:
			/* Check how much to read */
			if( ( mem_inode_table[idx].d_inode.size - (mem_inode_table[idx].pos + BLOCK_SIZE) ) > 0 )
				read_size = size;															// read block size
			else
				read_size = mem_inode_table[idx].d_inode.size - mem_inode_table[idx].pos;	// read remaining
		break;

		case INTYPE_DIR:
			/* Not allowed to read more than 1 directory entry at a time, so "more" cant be used on directories */
			if(size > (int)sizeof(struct dirent))
				return 0;
			read_size = size;
		break;

		return 0;	// Invalid type
	}

	/* Check if read will read out of file */
	if( (mem_inode_table[idx].pos + read_size) > mem_inode_table[idx].d_inode.size )
		return 0;

	/* Read given size of datablock(s) */
	helper_read_write(block_read_part, &mem_inode_table[idx].d_inode, mem_inode_table[idx].pos, read_size, buffer);
	mem_inode_table[idx].pos += read_size;	// Update position

	return read_size;
}

int fs_write(int fd, char *buffer, int size) {

	/* Index into global memory inode table */
	int idx = current_running->filedes[fd].idx;

	/* Check if caller have permission to write */
	int mode_bit = current_running->filedes[fd].mode & MODE_WRONLY;
	if(mode_bit != MODE_WRONLY)
		return FSE_INVALIDMODE;

	/* Check if inode is of type file */
	if(mem_inode_table[idx].d_inode.type != INTYPE_FILE)
		return FSE_INVALIDMODE;

	/* Check if write will write out of file */
	int ret = acquire_datablock(&mem_inode_table[idx].d_inode, mem_inode_table[idx].inode_num, strlen(buffer));
	if(ret < 0)
		return FSE_FULL;

	/* Write buffer, update inode size and position */
	helper_read_write(block_modify, &mem_inode_table[idx].d_inode, mem_inode_table[idx].pos, strlen(buffer), buffer);
	mem_inode_table[idx].d_inode.size += strlen(buffer);
	mem_inode_table[idx].pos += strlen(buffer);
	mem_inode_table[idx].dirty = 1;		// set dirty bit since size is changed.

	return 1;
}

/*
 * fs_lseek:
 * This function is really incorrectly named, since neither its offset
 * argument or its return value are longs (or off_t's).
 */
int fs_lseek(int fd, int offset, int whence) {

	/* Index into global memory inode table */
	int idx = current_running->filedes[fd].idx;

	switch(whence) {
		case SEEK_SET:	// Beginning of file.
		mem_inode_table[idx].pos = offset;
		break;

		case SEEK_CUR:	// Current position of the file pointer.
		mem_inode_table[idx].pos += offset;
		break;

		case SEEK_END:	// End of file.
		mem_inode_table[idx].pos = mem_inode_table[idx].d_inode.size - offset;
		break;
	}

	return 1;
}

int fs_mkdir(char *dirname) {
	int ret;

	/* Check if filename is valid */
	ret = valid_name(dirname);
	if(ret < 0)
		return ret;

	/* Get current working inode */
	struct disk_inode disk_inode;
	read_inode(&disk_inode, current_running->cwd);

	/* Check if current working directory need a new block for the new directory entry */
	ret = acquire_datablock(&disk_inode, current_running->cwd, (int)sizeof(struct dirent));
	if(ret < 0)
		return FSE_FULL;

	/* Check if dirname exists */
	if(path2inode(dirname) >= 0)
		return FSE_EXIST;

	/* Init new directory */
	inode_t inode = create_type(INTYPE_DIR);

	/* Write new directory entry, into current working directory */
	write_dirent(&disk_inode, inode,  dirname);

	/* Update current working inode, since the new entry increases size */
	write_inode(&disk_inode, current_running->cwd);

	return 0;
}

int fs_chdir(char *path) {

	/* Get inode of last name in path */
	int inum = path2inode(path);
	if(inum < 0)
		return FSE_DENOTFOUND;

	/* Check if path is directory */
	struct disk_inode disk_inode;
	read_inode(&disk_inode, inum);
	if(disk_inode.type != INTYPE_DIR)
		return FSE_DIRISFILE;

	current_running->cwd = inum;

	return 0;
}

int fs_rmdir(char *dirname) {
	int ret;

	/* Check if filename is valid */
	ret = valid_name(dirname);
	if(ret < 0)
		return ret;

	/* Check if dirname exists */
	inode_t remove_inum = path2inode(dirname);
	if(remove_inum < 0)
		return FSE_INVALIDNAME;

	/* Get inode from disk */
	struct disk_inode remove_inode;
	read_inode(&remove_inode, remove_inum);

	/* Check if inode is of type direcotry */
	if(remove_inode.type != INTYPE_DIR)
		return FSE_INVALIDMODE;

	/* Check if inode contains data */
	if( remove_inode.size != (int)(sizeof(struct dirent) * 2) ) 
		return FSE_DNOTEMPTY;

	/* Remove directory */
	free_bitmap_entry(remove_inode.direct[0], (unsigned char*)mem_superblock.dbmap);
	free_bitmap_entry(remove_inum, (unsigned char*)mem_superblock.ibmap);

	/* Update superblock */
	mem_superblock.d_super.ndata_blks--;
	mem_superblock.d_super.ninodes--;
	write_superblock();
	write_bitmap();

	/* ----- Clean up in current working directory ----- */
	
	/* Get current working inode */
	struct disk_inode cur_inode;
	read_inode(&cur_inode, current_running->cwd);

	/* Remove directory entry from current working directory */
	remove_dirent(&cur_inode, dirname);

	/* Write updated current working inode */
	write_inode(&cur_inode, current_running->cwd);

	return 1;
}

int fs_link(char *linkname, char *filename) {
	int ret;

	/* Check if filename is valid */
	ret = valid_name(filename);
	if(ret < 0)
		return ret;

	/* Get inode for link path */
	int file_inum = path2inode(linkname);
	if(file_inum < 0)
		return FSE_NOTEXIST;

	/* Check if end in path is of type file */
	struct disk_inode file_inode;
	read_inode(&file_inode, file_inum);
	if(file_inode.type != INTYPE_FILE)
		return FSE_INVALIDNAME;

	/* Get inode for current working direcotry */
	struct disk_inode dir_inode;
	read_inode(&dir_inode, current_running->cwd);

	/* Check if current working direcotry need a new block for the new directory entry */
	ret = acquire_datablock(&dir_inode, current_running->cwd, sizeof(struct dirent));
	if(ret < 0)
		return ret;

	/* Write link/copy entry into current working direcotry */
	write_dirent(&dir_inode, file_inum, filename);

	/* Increment number of links */
	file_inode.nlinks++;

	/* Write updated directory inode and file inode */
	write_inode(&dir_inode, current_running->cwd);
	write_inode(&file_inode, file_inum);

	return 1;
}

int fs_unlink(char *linkname) {
	int ret;

	/* Check if filename is valid */
	ret = valid_name(linkname);
	if(ret < 0)
		return ret;

	/* Get inode for filename */
	int file_inum = path2inode(linkname);
	if(file_inum < 0)
		return FSE_NOTEXIST;

	/* Check if filename is of type file */
	struct disk_inode file_inode;
	read_inode(&file_inode, file_inum);
	if(file_inode.type != INTYPE_FILE)
		return FSE_INVALIDNAME;

	/* Get current working directory */
	struct disk_inode dir_inode;
	read_inode(&dir_inode, current_running->cwd);

	/* Remove file entry from current working direcotry */
	remove_dirent(&dir_inode, linkname);

	file_inode.nlinks--;
	/* Check if file can be removed. */
	if(file_inode.nlinks < 0) {

		/* Remove datablocks */
		for(int i = 0; i < INODE_NDIRECT; i++)
			if(file_inode.direct[i] != FREE_BLK) {
				free_bitmap_entry(file_inode.direct[i], (unsigned char*)mem_superblock.dbmap);
				mem_superblock.d_super.ndata_blks--;
			}

		/* Remove inode */
		free_bitmap_entry(file_inum, (unsigned char*)mem_superblock.ibmap);
		mem_superblock.d_super.ninodes--;

		/* Update superblock */
		write_superblock();
		write_bitmap();
	} else {
		/* Write updated file inode if it is not removed */
		write_inode(&file_inode, file_inum);
	}

	/* Write updated directory inode */
	write_inode(&dir_inode, current_running->cwd);

	return 1;
}

int fs_stat(int fd, char *buffer) {

	/* Index into global memory inode table */
	int idx = current_running->filedes[fd].idx;

	/* Check if caller have permission to read */
	int mode_bit = current_running->filedes[fd].mode & MODE_RDONLY;
	if(mode_bit != MODE_RDONLY)
		return 0;

	/* Copy meta data into given buffer */
	bcopy((char*)&mem_inode_table[idx].d_inode.type, &buffer[0], (int)sizeof(mem_inode_table[idx].d_inode.type));		// Type	
	bcopy((char*)&mem_inode_table[idx].d_inode.nlinks, &buffer[1], (int)sizeof(mem_inode_table[idx].d_inode.nlinks));	// Links
	bcopy((char*)&mem_inode_table[idx].d_inode.size, &buffer[2], (int)sizeof(mem_inode_table[idx].d_inode.size));		// Size

	return 1;
}

/*
 * Helper functions for the system calls
 */

/*
 * get_free_entry:
 *
 * Search the given bitmap for the first zero bit.  If an entry is
 * found it is set to one and the entry number is returned.  Returns
 * -1 if all entrys in the bitmap are set.
 */
static int get_free_entry(unsigned char *bitmap) {
	int i;

	/* Seach for a free entry */
	for (i = 0; i < BITMAP_ENTRIES / 8; i++) {
		if (bitmap[i] == 0xff) /* All taken */
			continue;
		if ((bitmap[i] & 0x80) == 0) { /* msb */
			bitmap[i] |= 0x80;
			return i * 8;
		}
		else if ((bitmap[i] & 0x40) == 0) {
			bitmap[i] |= 0x40;
			return i * 8 + 1;
		}
		else if ((bitmap[i] & 0x20) == 0) {
			bitmap[i] |= 0x20;
			return i * 8 + 2;
		}
		else if ((bitmap[i] & 0x10) == 0) {
			bitmap[i] |= 0x10;
			return i * 8 + 3;
		}
		else if ((bitmap[i] & 0x08) == 0) {
			bitmap[i] |= 0x08;
			return i * 8 + 4;
		}
		else if ((bitmap[i] & 0x04) == 0) {
			bitmap[i] |= 0x04;
			return i * 8 + 5;
		}
		else if ((bitmap[i] & 0x02) == 0) {
			bitmap[i] |= 0x02;
			return i * 8 + 6;
		}
		else if ((bitmap[i] & 0x01) == 0) { /* lsb */
			bitmap[i] |= 0x01;
			return i * 8 + 7;
		}
	}
	return -1;
}

/*
 * free_bitmap_entry:
 *
 * Free a bitmap entry, if the entry is not found -1 is returned, otherwise zero.
 * Note that this function does not check if the bitmap entry was used (freeing
 * an unused entry has no effect).
 */
static int free_bitmap_entry(int entry, unsigned char *bitmap) {
	unsigned char *bme;

	if (entry >= BITMAP_ENTRIES)
		return -1;

	bme = &bitmap[entry / 8];

	switch (entry % 8) {
	case 0:
		*bme &= ~0x80;
		break;
	case 1:
		*bme &= ~0x40;
		break;
	case 2:
		*bme &= ~0x20;
		break;
	case 3:
		*bme &= ~0x10;
		break;
	case 4:
		*bme &= ~0x08;
		break;
	case 5:
		*bme &= ~0x04;
		break;
	case 6:
		*bme &= ~0x02;
		break;
	case 7:
		*bme &= ~0x01;
		break;
	}

	return 0;
}

/*
 * ino2blk:
 * Returns the filesystem block (block number relative to the super
 * block) corresponding to the inode number passed.
 */
static blknum_t ino2blk(inode_t ino) {
	int blk_num = (ino * sizeof(struct disk_inode)) / (BLOCK_SIZE + 1);
	return (blknum_t)blk_num;
}

/*
 * idx2blk:
 * Returns the filesystem block (block number relative to the super
 * block) corresponding to the data block index passed.
 */
static blknum_t idx2blk(int index) {
	return (blknum_t)(index + (os_size + 2));
}

/*
 * path2inode:
 * Parses a file name and returns the corresponding inode number. If
 * the file cannot be found, -1 is returned.
 */
static inode_t path2inode(char *path) {

	/* Copy path */
	char copy[SIZEX];
	strcpy(copy, path);

	/* Parse path */
	char *parsed[SIZEX];
	int num = parse_path(copy, parsed);

	int i = 0;
	struct dirent dirent;
	/* Check if absolute path, so it skips search for "/" */
	if(path[0] == '/') {
		i++;
		dirent.inode = 0;
	} else
		dirent.inode = current_running->cwd;

	struct disk_inode disk_inode;
	/* Iterate through each name in path */
	for(; i < num; i++) {

		int offset = 0;
		read_inode(&disk_inode, dirent.inode);

		/* Iterate through each entry in current inode */
		while (1) {

			/* Get directory entry */
			helper_read_write(block_read_part, &disk_inode, offset, sizeof(struct dirent), (char*)&dirent);
			
			if(same_string(dirent.name, parsed[i]))	// If string is same, HIT!
				break;
			offset += sizeof(struct dirent);
			if(offset > disk_inode.size)			// If offset is larger than size, MISS!
				return FSE_DENOTFOUND;
		}
	}
	
	return dirent.inode;
}
