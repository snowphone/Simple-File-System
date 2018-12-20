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
#ifdef debug
#define Log fprintf(stderr, "%s: %d\n", __func__, __LINE__)
#else
#define Log
#endif

#define MAX_FILES_IN_FOLDER (SFS_DENTRYPERBLOCK * SFS_NDIRECT)
#define MAX_NUM_OF_BLK (SFS_NDIRECT + SFS_DBPERIDB)
#define MAX_FILE_SIZE (MAX_NUM_OF_BLK * SFS_BLOCKSIZE)

#define min(x, y) ((x) < (y) ? (x) : (y))

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

	RM_NOT_EXISTS = -1,
	RM_IS_DIR = -9,

	CPIN_REAL_PATH_NOT_EXIST = -11,
	CPIN_ALREADY_EXISTS = -6,
	CPIN_FILE_SIZE_EXCEED = -12,
	CPIN_BLOCK_UNAVAILABLE = -4,
	CPIN_DIRECTORY_FULL = -3,

	CPOUT_ALREADY_EXISTS = -6,
	CPOUT_NOT_EXISTS = -1,
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
	case -11:
		printf("%s: can't open %s input file\n", message, path); return;
	case -12:
		printf("%s: input file size exceeds the max file size\n", message); return;
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
	uint32_t blockIdx,
			 byteIdx,
			 bitIdx;
};

struct BlockAddr findUnusedBlock()
{
	struct BlockAddr ret = { SFS_NOINO, };
	uint32_t byteIdx,
		blockIdx;
	for(blockIdx = SFS_MAP_LOCATION; blockIdx < SFS_MAP_LOCATION + SFS_BITBLOCKS(spb.sp_nblocks); ++blockIdx)
	{
		uint8_t buf[SFS_BLOCKSIZE] = { 0, };
		const uint8_t fullByte = ( 1ul << CHAR_BIT ) - 1;
		disk_read(buf, blockIdx);

		for(byteIdx = 0; byteIdx < SFS_BLOCKSIZE; ++byteIdx)
		{
			//Read block as a byte stream for speed
			if(buf[byteIdx] ==  fullByte)
				continue;
			uint32_t bitIdx;
			for (bitIdx = 0; bitIdx < CHAR_BIT; bitIdx++)
			{
				if( BIT_CHECK(buf[byteIdx], bitIdx) == 0 ) {
					ret.blockIdx = blockIdx;
					ret.byteIdx = byteIdx;
					ret.bitIdx = bitIdx;
					return ret;
				}
			}
		}
	}
	//Failed to find an unused block
	ret.blockIdx = SFS_NOINO;
	return ret;
}

void setBlock(const struct BlockAddr entity)
{
	uint8_t buf[SFS_BLOCKSIZE];
	disk_read(buf, entity.blockIdx);
	
	assert(!BIT_CHECK(buf[entity.byteIdx], entity.bitIdx));
	BIT_SET(buf[entity.byteIdx], entity.bitIdx);

	assert(entity.blockIdx != SFS_SB_LOCATION);
	disk_write(buf, entity.blockIdx);
}

struct BlockAddr decomposite(const uint32_t inodeNum){
	struct BlockAddr ret;
	ret.blockIdx = SFS_MAP_LOCATION + inodeNum / SFS_BLOCKBITS;
	ret.bitIdx = inodeNum % CHAR_BIT;
	ret.byteIdx = (inodeNum - (ret.blockIdx - SFS_MAP_LOCATION) * SFS_BLOCKBITS) / CHAR_BIT;

	return ret;
}

uint32_t composite(const struct BlockAddr blockNum){
	return (blockNum.blockIdx - SFS_MAP_LOCATION) * SFS_BLOCKBITS 
		+ blockNum.byteIdx * CHAR_BIT
		+ blockNum.bitIdx;
}

void releaseBlock(uint32_t inodeNum)
{
	if(inodeNum == SFS_NOINO)
		return;

	struct BlockAddr blkNum = decomposite(inodeNum);

	uint8_t buf[SFS_BLOCKSIZE];
	disk_read(buf, blkNum.blockIdx );
	
	assert( BIT_CHECK(buf[blkNum.byteIdx], blkNum.bitIdx ) );
	BIT_CLEAR(buf[blkNum.byteIdx], blkNum.bitIdx );

	assert(blkNum.blockIdx != SFS_SB_LOCATION);
	disk_write(buf, blkNum.blockIdx);
}



uint32_t getUnusedBlock()
{
	struct BlockAddr blockNum = findUnusedBlock();
	if(blockNum.blockIdx == SFS_NOINO)
		return SFS_NOINO;
	setBlock(blockNum);

	uint32_t ret = composite(blockNum);
	struct BlockAddr tmp = decomposite(ret);
	assert(memcmp(&tmp, &blockNum, sizeof tmp) == 0);
	return ret;
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

void* findStrInDirEntries(struct sfs_dir* entry, const uint32_t length, const char* str){
	struct sfs_dir* ptr;
	for(ptr = entry; ptr != entry + length; ++ptr) {
		if(isSameString(ptr->sfd_name, str))
			return ptr;
	}
	return NULL;
}

uint32_t divideAndCeil(const uint32_t lhs, const uint32_t rhs){
	const uint32_t tmp = lhs / rhs;
	if(tmp * rhs == lhs)
		return tmp;
	else
		return tmp + 1;

}

void sfs_touch(const char* path)
{
	uint32_t numOfFiles = getNumOfFilesInDirectory(&sd_cwd);
	uint32_t i;
	// cwd inode
	struct sfs_inode dirInode;
	disk_read( &dirInode, sd_cwd.sfd_ino );

	//for consistency
	assert( dirInode.sfi_type == SFS_TYPE_DIR );

	//check for empty entry in directoy inode's direct block pointer
	if(getNumOfFilesInDirectory(&sd_cwd) == MAX_FILES_IN_FOLDER ) {
		error_message("touch", path, TOUCH_DIRECTORY_FULL);
		goto exit;
	}

	//Check for filename duplication!
	for(i = 0; i < divideAndCeil(numOfFiles, SFS_DENTRYPERBLOCK); ++i)
	{
		struct sfs_dir entry_buf[SFS_DENTRYPERBLOCK];
		disk_read(entry_buf, dirInode.sfi_direct[i]);

		if(findStrInDirEntries(entry_buf, SFS_DENTRYPERBLOCK, path)) {
			error_message("touch", path, TOUCH_ALREADY_EXISTS);
			goto exit;
		}
	}

	//allocate new block
	uint32_t newbie_ino = getUnusedBlock();
	if(newbie_ino == SFS_NOINO) {
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
	const uint32_t directIdx = numOfFiles / SFS_DENTRYPERBLOCK,
		  entryIdx = numOfFiles % SFS_DENTRYPERBLOCK;

	if(entryIdx == 0){
		dirInode.sfi_direct[directIdx]  = getUnusedBlock();
		if(dirInode.sfi_direct[directIdx] == SFS_NOINO) {
			error_message("touch", path, TOUCH_BLOCK_UNAVAILABLE);
			goto exit;
		}
	}

	//block access
	disk_read( entries, dirInode.sfi_direct[directIdx] );

	//find an empty direct pointer
	struct sfs_dir* newbie = entries + entryIdx;
	newbie->sfd_ino = newbie_ino;
	strncpy( newbie->sfd_name, path, SFS_NAMELEN );

	assert(dirInode.sfi_direct[directIdx] != SFS_SB_LOCATION);
	disk_write( entries, dirInode.sfi_direct[directIdx] );

	dirInode.sfi_size += sizeof(struct sfs_dir);
	assert(sd_cwd.sfd_ino != SFS_SB_LOCATION);
	disk_write( &dirInode, sd_cwd.sfd_ino );

	struct sfs_inode newbieInode;

	bzero(&newbieInode,SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbieInode.sfi_size = 0;
	newbieInode.sfi_type = SFS_TYPE_FILE;

	assert(newbie->sfd_ino != SFS_SB_LOCATION);
	disk_write( &newbieInode, newbie->sfd_ino);
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

		struct sfs_dir* target = findStrInDirEntries(entries, SFS_DENTRYPERBLOCK, path);
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
	int i;
	struct sfs_inode dirInode;
	disk_read(&dirInode, sd_cwd.sfd_ino);

	if(findInCwd(org_path)) {
		error_message("mkdir", org_path, MKDIR_ALREADY_EXISTS);
		goto exit;
	} else if(dirInode.sfi_size / sizeof(struct sfs_dir) == MAX_FILES_IN_FOLDER) {
		error_message("mkdir", org_path, MKDIR_DIRECTORY_FULL);
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

	const uint32_t directIdx = numOfFiles / SFS_DENTRYPERBLOCK,
			 entryIdx = numOfFiles % SFS_DENTRYPERBLOCK;

	if(entryIdx == 0) {
		dirInode.sfi_direct[directIdx] = getUnusedBlock();
		if(dirInode.sfi_direct[directIdx] == SFS_NOINO) {
			error_message("mkdir", org_path, MKDIR_BLOCK_UNAVAILABLE);
			goto exit;
		}
	}

	disk_read(entries, dirInode.sfi_direct[directIdx]);
	assert(dirInode.sfi_direct[directIdx] != SFS_SB_LOCATION);
	struct sfs_dir* newbieEntry = &entries[entryIdx];

	/* Succeed to enter the usused entry! */

	newbieEntry->sfd_ino = getUnusedBlock();
	if(newbieEntry->sfd_ino == SFS_NOINO) {
		error_message("mkdir", org_path, MKDIR_BLOCK_UNAVAILABLE);
		goto exit;
	}
	strncpy(newbieEntry->sfd_name, org_path, SFS_NAMELEN);

	struct sfs_inode newbieInode;
	disk_read(&newbieInode, newbieEntry->sfd_ino);
	bzero(&newbieInode, SFS_BLOCKSIZE);
	newbieInode.sfi_type = SFS_TYPE_DIR;
	newbieInode.sfi_direct[0] = getUnusedBlock();
	if(newbieInode.sfi_direct[0] == SFS_NOINO) {
		//Roll back the allocated block
		releaseBlock(newbieEntry->sfd_ino);
		error_message("mkdir", org_path, MKDIR_BLOCK_UNAVAILABLE);
		//Do not write anything and return immediately
		goto exit;
	}

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

	assert(newbieInode.sfi_direct[0] != SFS_SB_LOCATION);
	disk_write(newbieEntries, newbieInode.sfi_direct[0]);

	assert(newbieEntry->sfd_ino != SFS_SB_LOCATION);
	disk_write(&newbieInode, newbieEntry->sfd_ino);

	assert(dirInode.sfi_direct[directIdx] != SFS_SB_LOCATION);
	disk_write(entries, dirInode.sfi_direct[directIdx]);
	assert(sd_cwd.sfd_ino != SFS_SB_LOCATION);
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
		*it_end = it_begin + MAX_FILES_IN_FOLDER;

	for(i = 0; i < numOfBlocks; ++i) {
		disk_read(entries[i], dirInode.sfi_direct[i]);
	}

	target = findStrInDirEntries(it_begin, MAX_FILES_IN_FOLDER, org_path);

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
		target = findStrInDirEntries(entries, SFS_DENTRYPERBLOCK, src_name);
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
	struct sfs_inode dirInode;
	disk_read(&dirInode, sd_cwd.sfd_ino);

	struct sfs_dir entries[SFS_NDIRECT][SFS_DENTRYPERBLOCK] = {0, },
				   *it,
				   *it_begin = *entries,
				   *it_end = it_begin + MAX_FILES_IN_FOLDER,
				   *target = NULL;
	int i,
		numOfBlocks = getBlocksToInvestigate(dirInode.sfi_size);

	for(i = 0; i < numOfBlocks; ++i) {
		disk_read(entries[i], dirInode.sfi_direct[i]);
	}

	target = findStrInDirEntries(it_begin, numOfBlocks * SFS_DENTRYPERBLOCK, path);

	//Exception handling
	if(!target){
		error_message("rm", path, RM_NOT_EXISTS);
		goto exit;
	} else if(isDirectory(target->sfd_ino)) {
		error_message("rm", path, RM_IS_DIR);
		goto exit;
	}

	/* Enter exception-free zone! */
	struct sfs_inode targetInode;
	disk_read(&targetInode, target->sfd_ino);

	//Release blocks
	int64_t unReleasedBytes = targetInode.sfi_size;
	//Release direct raw-data blocks
	for(i = 0; i != SFS_NDIRECT; ++i) {
		releaseBlock(targetInode.sfi_direct[i]);
		unReleasedBytes -= SFS_BLOCKSIZE;
		if(unReleasedBytes <= 0)
			goto array_compaction;
	}

	//This file uses an indirect pointer
	uint32_t indirectEntries[SFS_DBPERIDB];
	disk_read(indirectEntries, dirInode.sfi_indirect);

	for(i = 0; i != SFS_DBPERIDB; ++i){
		//Release second level raw-data blocks
		releaseBlock(indirectEntries[i]);
		unReleasedBytes -= SFS_BLOCKSIZE;
		if(unReleasedBytes <= 0)
			break;
	}
	//Release indirect pointer-array block
	releaseBlock(dirInode.sfi_indirect);

	//Restruct entries
array_compaction:
	//Release target inode block
	releaseBlock(target->sfd_ino);
	//Array compaction
	memmove(target, target + 1, ( it_end - (target + 1) ) * sizeof(struct sfs_dir) );
	for(i = 0; i < numOfBlocks; ++i) {
		disk_write(entries[i], dirInode.sfi_direct[i]);
	}
	dirInode.sfi_size -= sizeof(struct sfs_dir);
	disk_write(&dirInode, sd_cwd.sfd_ino);
exit:
	return;
}


uint32_t getFileSize(const char* path){
	struct stat st;
	stat(path, &st);
	return st.st_size;
}


void sfs_cpin(const char* local_path, const char* path) 
{
	FILE* fp = fopen(path, "rb");
	int64_t fileSize = getFileSize(path);
#define		uncopiedBytes (fileSize - newbieInode.sfi_size)
	if(!fp) {
		error_message("cpin", path, CPIN_REAL_PATH_NOT_EXIST);
		goto exit;
	} 
	if(findInCwd(local_path)){
		error_message("cpin", local_path, CPIN_ALREADY_EXISTS);
		goto exit;
	} 
	if(getNumOfFilesInDirectory(&sd_cwd) == MAX_FILES_IN_FOLDER) {
		error_message("cpin", local_path, CPIN_DIRECTORY_FULL);
		goto exit;
	}
	if(findUnusedBlock().blockIdx == SFS_NOINO){
		error_message("cpin", local_path, CPIN_BLOCK_UNAVAILABLE);
		goto exit;
	}
	if(fileSize > MAX_FILE_SIZE) {
		error_message("cpin", local_path, CPIN_FILE_SIZE_EXCEED);
		goto exit;
	}

	sfs_touch(local_path);

	const struct sfs_dir* const newbie = findInCwd(local_path);

	struct sfs_inode newbieInode;
	assert(newbie->sfd_ino != SFS_SB_LOCATION);
	disk_read(&newbieInode, newbie->sfd_ino);

	int i;
	//Copy raw-data into direct blocks
	for(i = 0; i < SFS_NDIRECT; ++i){

		uint8_t rawData[SFS_BLOCKSIZE] = { 0, };

		int n = fread(rawData, 1, SFS_BLOCKSIZE, fp);
		assert(n != -1);

		newbieInode.sfi_direct[i] = getUnusedBlock();
		if(newbieInode.sfi_direct[i] == SFS_NOINO)
		{
			error_message("cpin", local_path, CPIN_BLOCK_UNAVAILABLE);
			goto cp_done;
		}

		assert(newbieInode.sfi_direct[i] != SFS_SB_LOCATION);
		disk_write(rawData, newbieInode.sfi_direct[i]);
		newbieInode.sfi_size += n;

		if (uncopiedBytes == 0)
			goto cp_done;
	}

	/* Goto statement isn't executed.
	 * So, copy remaining raw-data into indirect block
	 */
start_indirect:
	newbieInode.sfi_indirect = getUnusedBlock();
	if(newbieInode.sfi_indirect == SFS_NOINO) {
		error_message("cpin", local_path, CPIN_BLOCK_UNAVAILABLE);
		goto cp_done;
	}

	uint32_t blockInodeList[SFS_DBPERIDB];
	assert(newbieInode.sfi_indirect != SFS_SB_LOCATION);
	disk_read(blockInodeList, newbieInode.sfi_indirect);
	bzero(&blockInodeList, sizeof blockInodeList);

	for(i = 0; i < SFS_DBPERIDB; ++i) {

		uint8_t rawData[SFS_BLOCKSIZE] = { 0, };
		int n = fread(rawData, 1, SFS_BLOCKSIZE, fp);

		blockInodeList[i] = getUnusedBlock();
		if(blockInodeList[i] == SFS_NOINO) {
			error_message("cpin", local_path, CPIN_BLOCK_UNAVAILABLE);
			goto cp_indirect_done;
		}

		assert(blockInodeList[i] != SFS_SB_LOCATION);
		disk_write(rawData, blockInodeList[i]);
		newbieInode.sfi_size += n;

		if (uncopiedBytes == 0)
			goto cp_indirect_done;
	}


cp_indirect_done:
	assert(newbieInode.sfi_indirect != SFS_SB_LOCATION);
	disk_write(blockInodeList, newbieInode.sfi_indirect);
cp_done:
	assert(newbie->sfd_ino != SFS_SB_LOCATION);
	disk_write(&newbieInode, newbie->sfd_ino);

	fclose(fp);
exit:
#undef uncopiedBytes
	return;
}

_Bool doesFileExist(const char* realPath) {
	return access(realPath, F_OK) != -1;
}

void sfs_cpout(const char* local_path, const char* path) 
{
	const struct sfs_dir* const target = findInCwd(local_path);
	if (!target) {
		error_message("cpout", local_path, CPOUT_NOT_EXISTS);
		return;
	}
	if (doesFileExist(path)) {
		error_message("cpout", path, CPOUT_ALREADY_EXISTS);
		return;
	}
	FILE* fp = fopen(path, "wb");

	struct sfs_inode targetInode;
	disk_read(&targetInode, target->sfd_ino);

	uint32_t uncopiedBytes = targetInode.sfi_size;

	int i;
	for(i = 0; i < SFS_NDIRECT; ++i){
		uint8_t buf[SFS_BLOCKSIZE];
		disk_read(buf, targetInode.sfi_direct[i]);
		uint32_t nwritten = fwrite(buf, 1, SFS_BLOCKSIZE, fp);
		uncopiedBytes -= nwritten;
		if(uncopiedBytes == 0)
			goto exit;
	}

	uint32_t blockInodeList[SFS_DBPERIDB];
	disk_read(blockInodeList, targetInode.sfi_indirect);

	for(i = 0; i < SFS_DBPERIDB; ++i){
		uint8_t buf[SFS_BLOCKSIZE];
		uint32_t writeBytes = min(SFS_BLOCKSIZE, uncopiedBytes);
		disk_read(buf, blockInodeList[i]);
		fwrite(buf, 1, writeBytes, fp);
		uncopiedBytes -= writeBytes;
		if(uncopiedBytes == 0)
			goto exit;
	}


exit:
	fclose(fp);
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
