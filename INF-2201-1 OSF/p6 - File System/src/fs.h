#ifndef FS_H
#define FS_H

#include "block.h"
#include "fstypes.h"
#include "inode.h"

/*
 * type for inode number. This is just the offset from the start of
 * the inode area on disk.
 */
#ifndef LINUX_SIM

#endif /* LINUX_SIM */

/* Number of file system blocks.
 *
 * If you decide to change this, you must probably change the number
 * of blocks reserved for the filesystem in createimage.c as well.*/
#define FS_BLOCKS (512 + 2)

#define MASK(v) (1 << (v))

/* fs_open mode flags */

/* This mode is used to mark a file descriptor table as unused */
#define MODE_UNUSED 0

/* Exactly one of these modes must be specified in fs_open */

#define MODE_RDONLY_BIT 1 /* Open for reading only */
#define MODE_RDONLY MASK(MODE_RDONLY_BIT)

#define MODE_WRONLY_BIT 2 /* Open for writing only */
#define MODE_WRONLY MASK(MODE_WRONLY_BIT)

#define MODE_RDWR_BIT 3 /* Open for reading and writing */
#define MODE_RDWR MASK(MODE_RDWR_BIT)

#define MODE_RWFLAGS_MASK (MODE_RDONLY | MODE_WRONLY | MODE_RDWR)

/* Any combination of the modes below are legal */

/* Makes no sense to have MODE_RDONLY together with this one */
#define MODE_CREAT_BIT 4 /* Create the file if it doesn't exist */
#define MODE_CREAT MASK(MODE_CREAT_BIT)

/* Makes no sense to have MODE_RDONLY together with this one */
#define MODE_TRUNC_BIT 5 /* Set file size to 0 */
#define MODE_TRUNC MASK(MODE_TRUNC_BIT)

enum
{
	MAX_FILENAME_LEN = 14,
	MAX_PATH_LEN = 256, /* Total length of a path */
	STAT_SIZE = 6,      /* Size of the information returned by fs_stat */
};

/* A directory entry */
struct dirent {
	inode_t inode;
	char name[MAX_FILENAME_LEN];
};

#define DIRENTS_PER_BLK (BLOCK_SIZE / sizeof(struct dirent))

#ifndef SEEK_SET
enum
{
	SEEK_SET,
	SEEK_CUR,
	SEEK_END
};
#endif

/**
 * @brief Check if given filename is valid to be used as directory entry.
 * @param filename Filename that should be checked.
 * @returns Return 1 if valid, else -1.
 */
static int valid_name(char* filename);

/**
 * @brief Write superblock to disk.
 * @returns Return 0 if successfully writes to disk, else -1.
 */
static int write_superblock(void);

/**
 * @brief Read superblock from disk.
 * @returns Return 0 if successfully reads from disk, else -1.
 */
static int read_superblock(void);

/**
 * @brief Write inode-bitmap and datablock-bitmap to disk.
 * @returns Return 0 if successfully writes to disk, else -1.
 */
static int write_bitmap(void);

/**
 * @brief Read inode-bitmap and datablock-bitmap from disk.
 * @returns Return 0 if successfully reads from disk, else -1.
 */
static int read_bitmap(void);

/**
 * @brief Write inode into inode-table on disk.
 * @param disk_inode Inode which should be written onto disk.
 * @param inode_num Inode-index into inode table on disk.
 * @returns Return 0 if successfully writes to disk, else -1.
 */
static int write_inode(struct disk_inode *disk_inode, inode_t inode_num);

/**
 * @brief Read inode from disk, and put data into given disk_inode*.
 * @param disk_inode Inode buffer, where the read data should be put.
 * @param inode_num Inode index which should be read from disk.
 * @returns Return 0 if successfully reads from disk, else -1.
 */
static int read_inode(struct disk_inode *disk_inode, inode_t inode_num);

/**
 * @brief Write a directory entry into given memory inode. (Will update inode size)
 * @param disk_inode Inode which the directory entry should be written to.
 * @param inode Inode which the directory entry should point to (usually parent).
 * @param name Name for directory entry.
 * @returns Return 1 if successfully writes to disk, else -1.
 */
static int write_dirent(struct disk_inode *disk_inode, inode_t inode, char *name);

/**
 * @brief Remove a directory entry. Will move last entry into slot
 		  where the removed entry resided. (Will update inode size)
 * @param disk_inode Inode which the directory entry should be removed from.
 * @param name Name for directory entry that should be removed.
 * @returns Return 1 if successfully removes, else -1.
 */
static int remove_dirent(struct disk_inode *disk_inode, char *remove_name);

/**
 * @brief Open an inode and put it into global memory inode table
 * @param inode_num Inode index which should be open.
 * @returns Return index into global memory inode table if successfully opens, else -1.
 */
static int open_inode(inode_t inode_num);

/**
 * @brief Close an inode and remove it from the global memory inode table
 * @param entry Index into global memory inode table.
 * @returns Return 0 if successfully closes, else -1.
 */
static int close_inode(int entry);

/**
 * @brief Read/write data from/to a datablock no disk.
 	Handle reading/writing to unspecified number of datablocks,
	but datablocks must be acquired.
 * @param operation Operation that should be done, SHOULD ONLY BE block_modify or block_read_part
 * @param disk_inode Inode which should be read/written.
 * @param offset Offset, where to start read/write.
 * @param size Size of data that should be read/written.
 * @param buffer Data which should be written to datablock.
 * @returns Return 1 if successfully read/write, else -1.
 */
static int helper_read_write(int (*operation)(int, int, int, void*), struct disk_inode *disk_inode, int offset, int size, char *buffer);

/**
 * @brief Check if inode need new datablock to write given size. Will acquire new datablock.
 * @param disk_inode Inode which need new datablock.
 * @param inode Inode index whicn need new datablock.
 * @param size Size of data that is being written.
 * @returns Return 0 if there was no need to acquire new block.
 		If successfully acquire new block return 1.
		Else -1.
 */
static int acquire_datablock(struct disk_inode *disk_inode, inode_t inode, int size);

/**
 * @brief Create and write a direcotry/file to disk.
 * @param type Type that should be created. (directory/file)
 * @returns Return inode-entry index for created direcotry/file, else -1.
 */
static inode_t create_type(int type);

void fs_init(void);
void fs_mkfs(void);
int fs_open(const char *filename, int mode);
int fs_close(int fd);
int fs_read(int fd, char *buffer, int size);
int fs_write(int fd, char *buffer, int size);
int fs_lseek(int fd, int offset, int whence);
int fs_link(char *linkname, char *filename);
int fs_unlink(char *linkname);
int fs_stat(int fd, char *buffer);

int fs_mkdir(char *dir_name);
int fs_chdir(char *path);
int fs_rmdir(char *path);

#endif
