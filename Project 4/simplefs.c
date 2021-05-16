#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "simplefs.h"
#include <string.h>

#define MAX_FILE_SIZE 4194304
// Global Variables =======================================
int vdisk_fd; // Global virtual disk file descriptor. Global within the library.
              // Will be assigned with the vsfs_mount call.
              // Any function in this file can use this.
              // Applications will not use  this directly.

struct superblockStruct {
    int noOfBlocks;
    int freeBlockCount;
    char emptySpace[4088];
} typedef superblock;


struct directoryEntryStruct {
    char filename[124];
    int fcbIndex;
} typedef directoryEntry;

struct rootDirectoryStruct {
    directoryEntry entries[32];
} typedef rootDirectory;


struct FCBStruct {
    int used;
    int inodeBlockNumber;
    int fileSize; // in bytes
    char emptySpace[116];
} typedef FCB;

struct FCBTableStruct {
    FCB fcbs[32];
} typedef FCBTable;


struct inodeStruct {
    int blockNumbers[1024];
} typedef inode;


struct openFileEntryStruct {
    int fcbIndex;
    int currentPointer;
    int mode;
    int fileSize;
    int inodeBlockNumber;
} typedef openFileEntry;

struct openFileTableStruct {
    openFileEntry entries[16];
} typedef openFileTable;

openFileTable* openFiles;
// ========================================================


// read block k from disk (virtual disk) into buffer block.
// size of the block is BLOCKSIZE.
// space for block must be allocated outside of this function.
// block numbers start from 0 in the virtual disk.
int read_block (void *block, int k)
{
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = read (vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
	printf ("read error\n");
	return -1;
    }
    return (0);
}

// write block k into the virtual disk.
int write_block (void *block, int k)
{
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = write (vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
	printf ("write error\n");
	return (-1);
    }
    return 0;
}

/*********************
   Helper functions
*********************/
void setBit(char* bitmapBlock, int offset, int value) {
    int charIndex = offset / 8;
    int bitOffset = offset % 8;

    bitmapBlock[charIndex] ^= (-value ^ bitmapBlock[charIndex]) & (1UL << bitOffset);
}

int getBit(char* bitmapBlock, int offset) {
    int charIndex = offset / 8;
    int bitOffset = offset % 8;

    return (bitmapBlock[charIndex] >> bitOffset)&1U;
}

int occupyEmptyBlock() {
    char bitmapBlock[4096];
    for (int k = 1; k < 5; ++k) {
        read_block(bitmapBlock, k);

        for (int l = 0; l < 32768; ++l) {
            if (getBit(bitmapBlock, l) == 1) {
                setBit(bitmapBlock, l, 0);
                write_block(bitmapBlock, k);

                superblock* metadata = (superblock*) malloc(BLOCKSIZE);
                read_block(metadata, 0);
                metadata->freeBlockCount -= 1;
                write_block(metadata, 0);

                printf("%dth block is occupied! New free block count is: %d\n",
                       (k - 1) * 32768 + l, metadata->freeBlockCount);

                free(metadata);
                return (k - 1) * 32768 + l;
            }
        }
    }
    return -1;
}

void freeBlock(int blockNumber){
    // TODO: Add one line information prompt şu şu bloklar freelendi
    int bitmapBlockNo = blockNumber / 32768 + 1;
    int bitmapBlockOffset = blockNumber % 32768;
    char bitmapBlock[4096];
    read_block(bitmapBlock, bitmapBlockNo);
    setBit(bitmapBlock, bitmapBlockOffset, 1); //inode block is free
    write_block(bitmapBlock, bitmapBlockNo);
    printf("%dth block is freed!", bitmapBlockNo);
    superblock* metadata = (superblock*)malloc(BLOCKSIZE);
    read_block(metadata, 0);
    metadata->freeBlockCount += 1;
    write_block(metadata, 0);
    free(metadata);
}

/**********************************************************************
   The following functions are to be called by applications directly.
***********************************************************************/

// this function is partially implemented.
int create_format_vdisk (char *vdiskname, unsigned int m)
{
    char command[1000];
    int size;
    int num = 1;
    int count;
    size  = num << m;
    count = size / BLOCKSIZE;
    sprintf(command, "dd if=/dev/zero of=%s bs=%d count=%d",
             vdiskname, BLOCKSIZE, count);
    system(command);

    sfs_mount(vdiskname);

    // Initialize superblock
    superblock* info = (superblock*)malloc(sizeof(superblock));
    info->noOfBlocks = count;
    info->freeBlockCount = count - 13;
    write_block(info, 0);
    free(info);

    // Initialize all bitmap blocks to all 1s
    char bitmap[4096];
    for (int i = 0; i < 4096; ++i) {
        bitmap[i] = -1;
    }
    write_block(bitmap, 2);
    write_block(bitmap, 3);
    write_block(bitmap, 4);

    // Set first 13 bit to 0 since we use them
    for (int i = 0; i < 13; ++i) {
        setBit(bitmap, i, 0);
    }
    write_block(bitmap, 1);

    // Initialize root directory
    rootDirectory* rootDirectory = malloc(sizeof(rootDirectory));
    for (int i = 0; i < 32; ++i) {
        strcpy(rootDirectory->entries[i].filename, "");
        rootDirectory->entries[i].fcbIndex = -1;
    }
    write_block(rootDirectory, 5);
    write_block(rootDirectory, 6);
    write_block(rootDirectory, 7);
    write_block(rootDirectory, 8);
    free(rootDirectory);

    // Initialize FCB Table
    FCBTable* fcbTable = malloc(sizeof(FCBTable));
    for (int i = 0; i < 32; ++i) {
        fcbTable->fcbs[i].fileSize = 0;
        fcbTable->fcbs[i].inodeBlockNumber = 0;
        fcbTable->fcbs[i].used = 0;
    }
    write_block(fcbTable, 9);
    write_block(fcbTable, 10);
    write_block(fcbTable, 11);
    write_block(fcbTable, 12);
    free(fcbTable);

    sfs_umount();
    return (0);
}


// already implemented
int sfs_mount (char *vdiskname)
{
    // simply open the Linux file vdiskname and in this
    // way make it ready to be used for other operations.
    // vdisk_fd is global; hence other function can use it.
    vdisk_fd = open(vdiskname, O_RDWR);
    //Initalize the open file table
    openFiles = malloc(sizeof(openFileTable));
    for(int i = 0; i < 16; ++i){
        openFiles->entries[i].currentPointer = 0;
        openFiles->entries[i].fcbIndex = -1;
        openFiles->entries[i].mode = -1;
        openFiles->entries[i].fileSize = 0;
        openFiles->entries[i].inodeBlockNumber = -1;
    }
    return(0);
}


// already implemented
int sfs_umount ()
{
    fsync (vdisk_fd); // copy everything in memory to disk
    close (vdisk_fd);
    return (0);
}


int sfs_create(char *filename)
{
    printf("Create %s\n", filename);
    superblock* metadata = (superblock*) malloc(BLOCKSIZE);
    read_block(metadata, 0);
    int noOfBlocks = metadata->noOfBlocks;

    int foundBlockNumber = -1; // Block number of found directory entry
    int foundOffset = -1; // Block offset of found directory entry
    int alreadyAllocated = 0;

    // Search empty directory table
    rootDirectory* block;
    for (int i = 5; i < 9; ++i) {
        //Search for ith block
        block = (rootDirectory*) malloc(BLOCKSIZE);
        read_block(block, i);

        for (int j = 0; j < 32; ++j) {
            if (block->entries[j].fcbIndex == -1
                && foundBlockNumber == -1 && foundOffset == -1) {
                // First empty directory entry is found
                foundBlockNumber = i;
                foundOffset = j;
            }
            if (strcmp(block->entries[j].filename, filename) == 0) {
                // Filename already exists in root directory
                alreadyAllocated = 1;
                printf("%s already exists in root directory\n", filename);
                break;
            }
        }
        free(block);
        if (alreadyAllocated) {
            break;
        }
    }

    if (alreadyAllocated == 0 && foundBlockNumber != -1 && foundOffset != -1) {
        // There is no file with specified name and empty directory entry is found

        // Search for empty FCB
        FCBTable* fcbBlock;
        int foundFCBIndex = -1;
        for (int i = 9; i < 13; ++i) {
            fcbBlock = (FCBTable*) malloc(BLOCKSIZE);
            read_block(fcbBlock, i);

            for (int j = 0; j < 32; ++j) {
                if (fcbBlock->fcbs[j].used == 0) {
                    // Empty FCB is found

                    // Search for empty block in bitmap
                    char bitmapBlock[4096];
                    int currentBlockIndex = 0;
                    for (int k = 1; k < 5; ++k) {
                        read_block(bitmapBlock, k);

                        for (int l = 0; l < 32768; ++l) {
                            if (getBit(bitmapBlock, l) == 1) {
                                foundFCBIndex = 32 * (i - 9) + j;

                                printf("%dth FCB entry is used\n", foundFCBIndex);
                                fcbBlock->fcbs[j].used = 1;
                                fcbBlock->fcbs[j].inodeBlockNumber = 32768 * (k - 1) + l;
                                fcbBlock->fcbs[j].fileSize = 0;
                                write_block(fcbBlock, i);

                                printf("%dth block is occupied for inode\n", 32768 * (k - 1) + l);
                                inode* inodeTable = (inode*) malloc(BLOCKSIZE);
                                for (int m = 0; m < 1024; ++m) {
                                    inodeTable->blockNumbers[m] = 0;
                                }
                                write_block(inodeTable, fcbBlock->fcbs[j].inodeBlockNumber);
                                free(inodeTable);

                                setBit(bitmapBlock, l, 0);
                                write_block(bitmapBlock, k);

                                metadata->freeBlockCount = metadata->freeBlockCount - 1;
                                write_block(metadata, 0);
                                printf("New free block count: %d\n", metadata->freeBlockCount);

                                break;
                            }
                            currentBlockIndex++;
                            if (currentBlockIndex == noOfBlocks) {
                                printf("There is not enough block!\n");
                                return -1;
                            }
                        }
                        if (foundFCBIndex != -1) {
                            break;
                        }
                    }
                    if (foundFCBIndex != -1) {
                        break;
                    }
                }
            }

            free(fcbBlock);
            if (foundFCBIndex != -1) {
                break;
            }
        }

        read_block(metadata, 0);
        free(metadata);

        if (foundFCBIndex != -1) {
            block = (rootDirectory*) malloc(sizeof(rootDirectory));
            read_block(block, foundBlockNumber);

            strcpy(block->entries[foundOffset].filename, filename);
            block->entries[foundOffset].fcbIndex = foundFCBIndex;
            printf("%dth directory entry is used\n", 32 * (foundBlockNumber - 5) + foundOffset);

            write_block(block, foundBlockNumber);
            free(block);

            return 0;
        } else {
            return -1;
        }

    }
    printf("There is no empty directory entry in root directory\n");
    return -1;
}


int sfs_open(char *file, int mode)
{
    //Finding the empty entry in openFile Table
    int openFileIndex = -1;
    for (int i = 0; i < 16; i++)
    {
        if(openFiles->entries[i].fcbIndex == -1){
            openFileIndex = i;
            break;
        }
    }

    //If there is an empty location in open file table
    if(openFileIndex != -1){
        int isFound = 0;
        //Specifying the FCBindex for a requested file
        rootDirectory* block;
        for(int i = 5; i < 9; ++i){
            block = (rootDirectory*) malloc(BLOCKSIZE);
            read_block(block, i);

            for(int j = 0; j < 32; ++j){
                //If the file is found in the directory
                if(strcmp(block->entries[j].filename, file) == 0){
                    isFound = 1;
                    //Getting the file size from the fcb
                    int blockNo = block->entries[j].fcbIndex / 32;
                    int offsetInBlock = block->entries[j].fcbIndex % 32;

                    FCBTable* fcbBlock = (FCBTable*)malloc(BLOCKSIZE);
                    read_block(fcbBlock, blockNo + 9);
                    openFiles->entries[openFileIndex].fileSize = fcbBlock->fcbs[offsetInBlock].fileSize;
                    openFiles->entries[openFileIndex].inodeBlockNumber = fcbBlock->fcbs[offsetInBlock].inodeBlockNumber;
                    //Assign the attributes for opened file into the open file table
                    openFiles->entries[openFileIndex].fcbIndex = block->entries[j].fcbIndex;
                    openFiles->entries[openFileIndex].mode = mode;

                    if (mode == MODE_APPEND) {
                        openFiles->entries[openFileIndex].currentPointer = fcbBlock->fcbs[offsetInBlock].fileSize;
                    } else {
                        openFiles->entries[openFileIndex].currentPointer = 0;
                    }

                    printf("%s is opened. Mode is %d. Current pointer is: %d. Related FCB index is: %d. fd: %d\n",
                           file, mode, openFiles->entries[openFileIndex].currentPointer,
                           openFiles->entries[openFileIndex].fcbIndex, openFileIndex);
                    free(fcbBlock);
                    break;
                }
            }

            free(block);
            if (isFound == 1) {
                break;
            }
        }
        if(isFound == 0){
            printf("There are no file with this name!");
            return -1;
        }
    }else{
        printf("There are already 16 files in the system!");
        return -1;
    }

    //Return the index on the open allocation table as a file descriptor
    return openFileIndex;
}

int sfs_close(int fd){
    if(openFiles->entries[fd].fcbIndex != -1){
        int blockNo = openFiles->entries[fd].fcbIndex / 32;
        int offsetInBlock = openFiles->entries[fd].fcbIndex % 32;

        //Writing fileSize into the FCB before it is closed
        FCBTable* fcbBlock = (FCBTable*)malloc(BLOCKSIZE);
        read_block(fcbBlock, blockNo + 9);
        fcbBlock->fcbs[offsetInBlock].fileSize = openFiles->entries[fd].fileSize;
        write_block(fcbBlock, blockNo + 9);

        //Indicates the specified entry in the open file table is free
        openFiles->entries[fd].fcbIndex = -1;
        free(fcbBlock);
        return 0;

    }else{
        printf("File is not opened yet!\n");
        return -1;
    }

}

int sfs_getsize (int  fd)
{
    if(openFiles->entries[fd].fcbIndex != -1){
        return openFiles->entries[fd].fileSize;
    }else{
        return -1;
    }
}

int sfs_read(int fd, void *buf, int n){
    if (openFiles->entries[fd].fcbIndex == -1) {
        printf("There is no open file with this fd\n");
        return -1;
    }

    if (openFiles->entries[fd].mode == MODE_APPEND) {
        printf("Mode is append! You cannot read\n");
        return -1;
    }

    if (openFiles->entries[fd].currentPointer >= openFiles->entries[fd].fileSize) {
        printf("End of file\n");
        return -1;
    }

    int readCount = n;
    if (openFiles->entries[fd].currentPointer + n > openFiles->entries[fd].fileSize) { // Overflow
        readCount = openFiles->entries[fd].fileSize - openFiles->entries[fd].currentPointer;
    }

    inode* inodeTable = (inode*) malloc(BLOCKSIZE);
    read_block(inodeTable,openFiles->entries[fd].inodeBlockNumber);

    int blockIndex = openFiles->entries[fd].currentPointer / BLOCKSIZE;
    int blockOffset = openFiles->entries[fd].currentPointer % BLOCKSIZE;


    char blockContent[4096];
    read_block(blockContent, inodeTable->blockNumbers[blockIndex]);

    for (int i = 0; i < readCount; ++i) {
        sprintf(&(((char*)buf)[i]), "%c", blockContent[blockOffset]);
        blockOffset++;

        if (blockOffset == BLOCKSIZE) {
            blockOffset = 0;
            blockIndex++;
            read_block(blockContent, inodeTable->blockNumbers[blockIndex]);
        }
    }
    openFiles->entries[fd].currentPointer = openFiles->entries[fd].currentPointer + readCount;
    return readCount;
}

int sfs_append(int fd, void *buf, int n)
{
    if (openFiles->entries[fd].fcbIndex == -1) {
        printf("There is no open file with this fd\n");
        return -1;
    }
    if (openFiles->entries[fd].mode == MODE_READ) {
        printf("Mode is read! You cannot append\n");
        return -1;
    }
    superblock* metadata = (superblock*) malloc(BLOCKSIZE);
    read_block(metadata, 0);

    if (metadata->freeBlockCount * BLOCKSIZE < n) {
        printf("There is not enough blocks to write %d bytes\n", n);
        free(metadata);
        return -1;
    }
    free(metadata);

    if (openFiles->entries[fd].currentPointer + n >= MAX_FILE_SIZE) {
        printf("Max file size is reached\n");
        return -1;
    }

    inode* inodeTable = (inode*) malloc(BLOCKSIZE);
    read_block(inodeTable,openFiles->entries[fd].inodeBlockNumber);

    if (openFiles->entries[fd].fileSize == 0) {
        int blockNumber = occupyEmptyBlock();
        inodeTable->blockNumbers[0] = blockNumber;
        write_block(inodeTable, openFiles->entries[fd].inodeBlockNumber);
    }

    int blockIndex = openFiles->entries[fd].currentPointer / BLOCKSIZE;
    int blockOffset = openFiles->entries[fd].currentPointer % BLOCKSIZE;

    char blockContent[4096];
    read_block(blockContent, inodeTable->blockNumbers[blockIndex]);

    for (int i = 0; i < n; ++i) {
        strcpy(&(blockContent[blockOffset]), &(((char*)buf)[i]));
        blockOffset++;

        if (blockOffset == BLOCKSIZE) {
            write_block(blockContent, inodeTable->blockNumbers[blockIndex]);
            blockOffset = 0;
            blockIndex++;

            int blockNumber = occupyEmptyBlock();
            inodeTable->blockNumbers[blockIndex] = blockNumber;
            write_block(inodeTable, openFiles->entries[fd].inodeBlockNumber);

            read_block(blockContent, inodeTable->blockNumbers[blockIndex]);
        }
    }
    write_block(blockContent, inodeTable->blockNumbers[blockIndex]);

    openFiles->entries[fd].currentPointer += n;
    openFiles->entries[fd].fileSize += n;

    return n;
}

int sfs_delete(char *filename)
{
    int deletedFCBIndex = -1;

    //Finding the directory entry
    rootDirectory* block;
    for(int i = 5; i < 9; ++i){
        //Search fir ith block
        block = (rootDirectory*) malloc(BLOCKSIZE);
        read_block(block, i);

        for(int j = 0; j < 32; ++j){
            //If the filename is found in the directory entry
            if(strcmp(block->entries[j].filename, filename) == 0){
                deletedFCBIndex = block->entries[j].fcbIndex;
                block->entries[j].fcbIndex = -1;
                strcpy(block->entries[j].filename, "");
                printf("The file %s is deleted from root directory!", filename);
                printf("The FCB %d is freed!", deletedFCBIndex);
                write_block(block, i);
                break;
            }
        }
        free(block);
    }

    if(deletedFCBIndex != -1){ //There is a file with this name in the root directory
        //Finding block number and block offset for given FCB index
        int fcbBlockNo = deletedFCBIndex / 32;
        int fcbBlockOffset = deletedFCBIndex % 32;
        FCBTable* fcbBlock = (FCBTable*)malloc(BLOCKSIZE);
        read_block(fcbBlock, fcbBlockNo + 9);
        int inodeBlockNumber = fcbBlock->fcbs[fcbBlockOffset].inodeBlockNumber;

        freeBlock(inodeBlockNumber);        

        if(fcbBlock->fcbs[fcbBlockOffset].fileSize != 0){
            inode* inodeTable = (inode*)malloc(BLOCKSIZE);
            read_block(inodeTable, inodeBlockNumber);
            for(int i = 0; i < 1024; ++i){
                if(inodeTable->blockNumbers[i] == 0){
                    break;
                }
                freeBlock(inodeTable->blockNumbers[i]);
            }
            free(inodeTable);
        }
        fcbBlock->fcbs[fcbBlockOffset].used = 0;
        fcbBlock->fcbs[fcbBlockOffset].fileSize = 0;
        write_block(fcbBlock, fcbBlockNo + 9);

        free(fcbBlock);
    }else{ //There is no file with this name in the directory
        printf("There is no file with this name in the directory!");
        return -1;
    }

    return (0);
}


