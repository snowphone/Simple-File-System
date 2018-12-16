//
// Simple FIle System
// Student Name : 문준오
// Student Number : B611062
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h> //to use uint32_t
#include <stdbool.h>

/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
/***********/

#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"

void dump_directory();

/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1<<(b)))
#define BIT_CHECK(a,b) ((a) & (1<<(b)))

static struct sfs_super spb;	// superblock
static struct sfs_dir sd_cwd = { SFS_NOINO }; // current working directory

/* my macro */

#define debug

#define MAX_NUM_OF_DIR (SFS_DENTRYPERBLOCK * SFS_NDIRECT)
#ifdef debug
 #define Log printf("%s, %d\n", __func__, __LINE__)
 #define check(comment, option) do{\
		assert(comment && (option));\
		fprintf(stderr, "%dline \'%s\': Success\n", __LINE__, comment);\
		}while(0);
#else
 #define Log 
 #define check(x,y)
#endif

enum ERROR{
	CD_NOT_DIR = -2,
	CD_NOT_EXISTS = -1,

	LS_NOT_EXISTS = -1,

	TOUCH_BLOCK_UNAVAILABLE = -4,
	TOUCH_DIRECTORY_FULL = -3,
	TOUCH_ALREADY_EXISTS = -6,

	RMDIR_INVALID_ARG = -8,
	RMDIR_DIR_NOT_EMPTY = -7,
	RMDIR_NOT_DIR = -5,

	MKDIR_ALREADY_EXISTS = -6,
	MKDIR_DIRECTORY_FULL = -3,
	MKDIR_BLOCK_UNAVAILABLE = -4,

	MV_NOT_EXISTS = -1,
	MV_ALREADY_EXISTS = -6,
};

void error_message(const char *message, const char *path, int error_code) {
	switch (error_code) {
	case -1:
		printf("%s: %s: No such file or directory\n",message, path); return;
	case -2:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -3:
		printf("%s: %s: Directory full\n",message, path); return;
	case -4:
		printf("%s: %s: No block available\n",message, path); return;
	case -5:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -6:
		printf("%s: %s: Already exists\n",message, path); return;
	case -7:
		printf("%s: %s: Directory not empty\n",message, path); return;
	case -8:
		printf("%s: %s: Invalid argument\n",message, path); return;
	case -9:
		printf("%s: %s: Is a directory\n",message, path); return;
	case -10:
		printf("%s: %s: Is not a file\n",message, path); return;
	default:
		printf("unknown error code\n");
		return;
	}
}

void sfs_mount(const char* path)
{
	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}

	printf("Disk image: %s\n", path);

	disk_open(path);
	disk_read( &spb, SFS_SB_LOCATION );

	printf("Superblock magic: %x\n", spb.sp_magic);

	assert( spb.sp_magic == SFS_MAGIC );
	
	printf("Number of blocks: %d\n", spb.sp_nblocks);
	printf("Volume name: %s\n", spb.sp_volname);
	printf("%s, mounted\n", spb.sp_volname);
	
	sd_cwd.sfd_ino = 1;		//init at root
	sd_cwd.sfd_name[0] = '/';
	sd_cwd.sfd_name[1] = '\0';
}

void sfs_umount() {
	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}
}

struct BlockAddr{
	uint32_t blocksIdx,
			 bytesIdx,
			 bitsIdx;
};

struct BlockAddr findUnusedBlock()
{
	struct BlockAddr ret = {SFS_NOINO, };
	const int end = SFS_MAP_LOCATION + SFS_BITBLOCKS(spb.sp_nblocks);
	int i,
		block_num;
	for(block_num = SFS_MAP_LOCATION; block_num < end; ++block_num)
	{
		uint8_t buf[SFS_BLOCKSIZE] = { 0, };
		disk_read(buf, block_num);

		for(i = 0; i != SFS_BLOCKSIZE; ++i)
		{
			//Read block as a byte stream for speed
			if(buf[i] == ( 1u << CHAR_BIT ) - 1 )
				continue;
			int j;
			for(j = 0; j < CHAR_BIT; ++j)
			{
				if( BIT_CHECK(buf[i], j) == 0 ) {
					ret.blocksIdx = block_num;
					ret.bytesIdx = i;
					ret.bitsIdx = j;
					return ret;
				}
			}
		}
	}
	ret.blocksIdx = SFS_NOINO;
	return ret;
}

void setBlock(const struct BlockAddr entity)
{
	u_int8_t buf[SFS_BLOCKSIZE] = { 0, };
	disk_read(buf, entity.blocksIdx);
	
	BIT_SET(buf[entity.bytesIdx], entity.bitsIdx);

	disk_write(buf, entity.blocksIdx);
}

struct BlockAddr decomposite(const uint32_t inodeNum){
	struct BlockAddr ret;
	ret.blocksIdx = SFS_MAP_LOCATION + inodeNum / SFS_BLOCKBITS;
	ret.bitsIdx = inodeNum % CHAR_BIT;
	ret.bytesIdx = (inodeNum - (ret.blocksIdx - SFS_MAP_LOCATION) * SFS_BLOCKBITS) / CHAR_BIT;

	return ret;
}

uint32_t composite(const struct BlockAddr blockNum){
	return (blockNum.blocksIdx - SFS_MAP_LOCATION) * SFS_BLOCKBITS 
		+ blockNum.bytesIdx * CHAR_BIT
		+ blockNum.bitsIdx;
}

void releaseBlock(uint32_t inodeNum)
{
	if(inodeNum == SFS_NOINO)
		return;

	struct BlockAddr blkNum = decomposite(inodeNum);

	u_int8_t buf[SFS_BLOCKSIZE];
	disk_read(buf, blkNum.blocksIdx );
	
	BIT_CLEAR(buf[blkNum.bytesIdx], blkNum.bitsIdx );

	disk_write(buf, blkNum.blocksIdx);
}



int64_t getUnusedBlock()
{
	struct BlockAddr blockNum = findUnusedBlock();
	if(blockNum.blocksIdx == SFS_NOINO)
		return -1;
	setBlock(blockNum);

	return composite(blockNum);
}

uint32_t getNumOfFilesInDirectory(const struct sfs_dir* dirPtr)
{
	struct sfs_inode dirInode;
	disk_read(&dirInode, dirPtr->sfd_ino);
	assert(dirInode.sfi_type == SFS_TYPE_DIR);

	return dirInode.sfi_size / sizeof(struct sfs_dir);
}

_Bool isSameString(const char* lhs, const char* rhs){
	return strncmp(lhs, rhs, SFS_NAMELEN) == 0;
}

void* findStrInDirEntries(struct sfs_dir* entry, const char* str){
	struct sfs_dir* ptr;
	for(ptr = entry; ptr != entry + SFS_DENTRYPERBLOCK; ++ptr) {
		if(isSameString(ptr->sfd_name, str))
			return ptr;
	}
	return NULL;
}

void sfs_touch(const char* path)
{
	uint32_t numOfFiles = getNumOfFilesInDirectory(&sd_cwd);
	uint32_t i;
	// cwd inode
	struct sfs_inode inode;
	disk_read( &inode, sd_cwd.sfd_ino );

	//for consistency
	assert( inode.sfi_type == SFS_TYPE_DIR );

	//we assume that cwd is the root directory and root directory is empty which has . and .. only
	//unused DISK2.img satisfy these assumption
	//for new directory entry(for new file), we use cwd.sfi_direct[0] and offset 2
	//becasue cwd.sfi_directory[0] is already allocated, by .(offset 0) and ..(offset 1)
	//for new inode, we use block 6 
	// block 0: superblock,	block 1:root, 	block 2:bitmap 
	// block 3:bitmap,  	block 4:bitmap 	block 5:root.sfi_direct[0] 	block 6:unused
	//
	
	//check for empty entry in directoy inode's direct block pointer
	if(inode.sfi_size / sizeof(struct sfs_dir) == MAX_NUM_OF_DIR ) {
		error_message("touch", path, TOUCH_DIRECTORY_FULL);
		goto exit;
	}

	//Check for filename duplication!
	for(i = 0; i < numOfFiles / SFS_DENTRYPERBLOCK + 1; ++i)
	{
		struct sfs_dir entry_buf[SFS_DENTRYPERBLOCK];
		disk_read(entry_buf, inode.sfi_direct[i]);

		if(findStrInDirEntries(entry_buf, path)) {
			error_message("touch", path, TOUCH_ALREADY_EXISTS);
			goto exit;
		}
	}

	//allocate new block
	int newbie_ino = getUnusedBlock();
	if(newbie_ino == -1) {
		error_message("touch", path, TOUCH_BLOCK_UNAVAILABLE);
		goto exit;
	}

	/* --------- No more exceptions! ----------- */

	//buffer for disk read
	struct sfs_dir entries[SFS_DENTRYPERBLOCK];

	/* A direct block pointer can hold SFS_DENTRYPERBLOCK entries 
	 * There's SFS_NDIRECT direct pointers
	 * Therefore, when the number of files n is given then
	 * read inode's (n / SFS_DENTRYPERBLOCK)th direct pointer and 
	 * access entry array's (n % SFS_DENTRYPERBLOCK)th entry!
	 */

	//block access
	disk_read( entries, inode.sfi_direct[numOfFiles / SFS_DENTRYPERBLOCK] );

	//find an empty direct pointer
	struct sfs_dir* newbie_entry = entries + numOfFiles % SFS_DENTRYPERBLOCK;
	newbie_entry->sfd_ino = newbie_ino;
	strncpy( newbie_entry->sfd_name, path, SFS_NAMELEN );

	disk_write( entries, inode.sfi_direct[numOfFiles / SFS_DENTRYPERBLOCK] );

	inode.sfi_size += sizeof(struct sfs_dir);
	disk_write( &inode, sd_cwd.sfd_ino );

	struct sfs_inode newbie;

	bzero(&newbie,SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbie.sfi_size = 0;
	newbie.sfi_type = SFS_TYPE_FILE;

	disk_write( &newbie, newbie_ino );
exit:
	return;
}

_Bool isDirectory(const uint32_t inode_num) {
	struct sfs_inode inode;
	disk_read(&inode, inode_num);

	return inode.sfi_type == SFS_TYPE_DIR;
}



uint32_t getBlocksToInvestigate(const u_int32_t bytes){
	return bytes / SFS_BLOCKSIZE + 1;
}

void* findInCwd(const char* path)
{
	struct sfs_inode inode;
	disk_read(&inode, sd_cwd.sfd_ino);

	uint32_t i;
	for(i = 0; i < getBlocksToInvestigate(inode.sfi_size); ++i)
	{
		struct sfs_dir entries[SFS_DENTRYPERBLOCK] = {0, };
		assert(sizeof(entries) >= SFS_BLOCKSIZE);
		disk_read(entries, inode.sfi_direct[i]);

		struct sfs_dir* target = findStrInDirEntries(entries, path);
		if(target)
			return target;
	}
	return NULL;
}

void sfs_cd(const char* path)
{
	if(!path) {
		sd_cwd.sfd_ino = 1;
		strncpy(sd_cwd.sfd_name, "/", SFS_NAMELEN );
		goto exit;
	}

	struct sfs_inode inode;
	disk_read(&inode, sd_cwd.sfd_ino);

	struct sfs_dir* target = findInCwd(path);

	if(target){
		if(isDirectory(target->sfd_ino)) {
			sd_cwd = *target;
			goto exit;
		}
		else {
			/* not a directory */
			error_message("cd", path, CD_NOT_DIR);
			goto exit;
		}
	} else {
		/* not exist -> raise error */
		error_message("cd", path, CD_NOT_EXISTS);
		goto exit;
	}
exit:
	return;
}

void sfs_ls(const char* path)
{
	if(!path)
		path = ".";

	struct sfs_dir* entry = findInCwd(path);

	if(!entry) {
		error_message("ls", path, LS_NOT_EXISTS);
		goto immediate_exit;
	}

	if(isDirectory(entry->sfd_ino))
	{
		/* list every files in 'directory' */
		struct sfs_inode inode;
		disk_read(&inode, entry->sfd_ino);

		uint32_t i, 
				 totalFiles = inode.sfi_size / sizeof(struct sfs_dir),
				 cnt = 0;
		for(i = 0; i < getBlocksToInvestigate(inode.sfi_size); ++i)
		{
			struct sfs_dir entries[SFS_DENTRYPERBLOCK];
			disk_read(entries, inode.sfi_direct[i]);

			uint32_t j;
			for(j = 0; j < SFS_DENTRYPERBLOCK; ++j)
			{
				++cnt;
				_Bool isDir = isDirectory(entries[j].sfd_ino);
				/* display each file */
				printf("%s%s\t", entries[j].sfd_name, isDir? "/" : "");

				if(cnt == totalFiles)
					goto exit;
			}
		}
	} else {
		/* list specific file */
		printf("%s", path);
		goto exit;
	}
exit:
	puts("");	//newline
immediate_exit:
	return;
}

void sfs_mkdir(const char* org_path) 
{
	struct sfs_inode dirInode;
	disk_read(&dirInode, sd_cwd.sfd_ino);

	int i;

	if(findInCwd(org_path)) {
		error_message("mkdir", org_path, MKDIR_ALREADY_EXISTS);
		goto exit;
	} else if(dirInode.sfi_size / sizeof(struct sfs_dir) == MAX_NUM_OF_DIR) {
		error_message("mkdir", org_path, MKDIR_DIRECTORY_FULL);
		goto exit;
	}

	int64_t newBlockNOs[2];
	newBlockNOs[0] = getUnusedBlock();
	if(newBlockNOs[0] == -1) {
		error_message("mkdir", org_path, MKDIR_BLOCK_UNAVAILABLE);
		goto exit;
	}
	newBlockNOs[1] = getUnusedBlock();
	if(newBlockNOs[1] == -1) {
		puts("HI HELLO 안녕");
		releaseBlock(newBlockNOs[0]);
		error_message("mkdir", org_path, MKDIR_BLOCK_UNAVAILABLE);
		goto exit;
	}
	/* A direct block pointer can hold SFS_DENTRYPERBLOCK entries 
	 * There's SFS_NDIRECT direct pointers
	 * Therefore, when the number of files n is given then
	 * read inode's (n / SFS_DENTRYPERBLOCK)th direct pointer and 
	 * access entry array's (n % SFS_DENTRYPERBLOCK)th entry!
	 */

	struct sfs_dir entries[SFS_DENTRYPERBLOCK];
	uint32_t numOfFiles = getNumOfFilesInDirectory(&sd_cwd);
	disk_read(entries, dirInode.sfi_direct[numOfFiles / SFS_DENTRYPERBLOCK]);
	struct sfs_dir* newbieEntry = &entries[numOfFiles % SFS_DENTRYPERBLOCK];

	/* Succeed to enter the usused entry! */

	newbieEntry->sfd_ino = newBlockNOs[0];
	strncpy(newbieEntry->sfd_name, org_path, SFS_NAMELEN);

	struct sfs_inode newbieInode;
	disk_read(&newbieInode, newbieEntry->sfd_ino);
	bzero(&newbieInode, SFS_BLOCKSIZE);
	newbieInode.sfi_type = SFS_TYPE_DIR;
	newbieInode.sfi_direct[0] = newBlockNOs[1];

	struct sfs_dir newbieEntries[SFS_DENTRYPERBLOCK];
	disk_read(newbieEntries, newbieInode.sfi_direct[0]);

	for(i = 0; i < SFS_DENTRYPERBLOCK; ++i) {
		newbieEntries[i].sfd_ino = SFS_NOINO;
	}

	newbieEntries[0].sfd_ino = newbieEntry->sfd_ino;
	strncpy(newbieEntries[0].sfd_name, ".", SFS_NAMELEN);

	newbieEntries[1].sfd_ino = sd_cwd.sfd_ino;
	strncpy(newbieEntries[1].sfd_name, "..", SFS_NAMELEN);
	
	newbieInode.sfi_size = sizeof(struct sfs_dir) * 2;
	dirInode.sfi_size += sizeof(struct sfs_dir);

	disk_write(newbieEntries, newbieInode.sfi_direct[0]);
	disk_write(&newbieInode, newbieEntry->sfd_ino);
	disk_write(entries, dirInode.sfi_direct[numOfFiles / SFS_DENTRYPERBLOCK]);
	disk_write(&dirInode, sd_cwd.sfd_ino);


exit:
	return;
}

void sfs_rmdir(const char* org_path) 
{
	if(isSameString(org_path, ".")) {
		goto invalid_arg;
	}

	struct sfs_inode dirInode;
	disk_read(&dirInode, sd_cwd.sfd_ino);

	int i, numOfBlocks = getBlocksToInvestigate(dirInode.sfi_size);
	struct sfs_dir entries[SFS_NDIRECT][SFS_DENTRYPERBLOCK] = {0, };
	struct sfs_dir* target = NULL, 
		*it,
		*it_begin = *entries,
		*it_end = it_begin + MAX_NUM_OF_DIR;

	for(i = 0; i < numOfBlocks; ++i) {
		disk_read(entries[i], dirInode.sfi_direct[i]);
	}

	for(it = it_begin; it != it_begin + MAX_NUM_OF_DIR; ++it)
	{
		if(isSameString(org_path, it->sfd_name)) {
			target = it;
			break;
		}
	}

	if(!target){
		goto invalid_arg;
	} else if(!isDirectory(target->sfd_ino)) {
		error_message("rmdir", org_path, RMDIR_NOT_DIR);
		goto exit;
	} else if(getNumOfFilesInDirectory(target)  != 2) {
		/* 2 means "." and ".." 
		 * It means the directory is not empty
		 */
		error_message("rmdir", org_path, RMDIR_DIR_NOT_EMPTY);
		goto exit;
	} else {

		/* 현재의 코드는 블럭의 내용을 순서대로 채워 쓴다고 가정함.
		 * 따라서 파일 또는 디렉토리 삭제시, 뒤에 위치한 블럭들을 전부 당겨주어야 함
		 * TODO: 현재 출력은 잘 되나, 비트맵에서 문제가 발생하고 있음.
		 */
		struct sfs_inode targetInode;
		disk_read(&targetInode, target->sfd_ino);
		releaseBlock(targetInode.sfi_direct[0]);
		
		releaseBlock(target->sfd_ino);
		memmove(target, target + 1, (it_end - (target + 1) ) * sizeof(struct sfs_dir) );
		dirInode.sfi_size -= sizeof(struct sfs_dir);

		for(i = 0; i < numOfBlocks; ++i) {
			disk_write(entries[i], dirInode.sfi_direct[i]);
		}

		disk_write(&dirInode, sd_cwd.sfd_ino);
		goto exit;
	} 

invalid_arg:
	error_message("rmdir", org_path, RMDIR_INVALID_ARG);
exit:
	return;
}

void sfs_mv(const char* src_name, const char* dst_name) 
{
	struct sfs_inode dirInode;
	disk_read(&dirInode, sd_cwd.sfd_ino);
	int dirIdx;
	struct sfs_dir* target = NULL;
	struct sfs_dir entries[SFS_DENTRYPERBLOCK];

	for(dirIdx = 0; dirIdx < getBlocksToInvestigate(dirInode.sfi_size); ++dirIdx)
	{
		disk_read(entries, dirInode.sfi_direct[dirIdx]);
		target = findStrInDirEntries(entries, src_name);
		if(target)
			break;
	}
	
	if(!target) {
		error_message("mv", src_name, MV_NOT_EXISTS);
		goto exit;
	}
	if(findInCwd(dst_name)) {
		error_message("mv", dst_name, MV_ALREADY_EXISTS);
		goto exit;
	}

	/* Enter exception-free zone! */

	strncpy(target->sfd_name, dst_name, SFS_NAMELEN);
	disk_write(entries, dirInode.sfi_direct[dirIdx]);

exit:
	return;
}

void sfs_rm(const char* path) 
{
	printf("Not Implemented\n");
}

void sfs_cpin(const char* local_path, const char* path) 
{
	printf("Not Implemented\n");
}

void sfs_cpout(const char* local_path, const char* path) 
{
	printf("Not Implemented\n");
}

void dump_inode(struct sfs_inode inode) {
	int i;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

	printf("size %d type %d direct ", inode.sfi_size, inode.sfi_type);
	for(i=0; i < SFS_NDIRECT; i++) {
		printf(" %d ", inode.sfi_direct[i]);
	}
	printf(" indirect %d",inode.sfi_indirect);
	printf("\n");

	if (inode.sfi_type == SFS_TYPE_DIR) {
		for(i=0; i < SFS_NDIRECT; i++) {
			if (inode.sfi_direct[i] == 0) break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			dump_directory(dir_entry);
		}
	}

}

void dump_directory(struct sfs_dir dir_entry[]) {
	int i;
	struct sfs_inode inode;
	for(i=0; i < SFS_DENTRYPERBLOCK;i++) {
		printf("%d %s\n",dir_entry[i].sfd_ino, dir_entry[i].sfd_name);
		disk_read(&inode,dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE) {
			printf("\t");
			dump_inode(inode);
		}
	}
}

void sfs_dump() {
	// dump the current directory structure
	struct sfs_inode c_inode;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	printf("cwd inode %d name %s\n",sd_cwd.sfd_ino,sd_cwd.sfd_name);
	dump_inode(c_inode);
	printf("\n");

}
