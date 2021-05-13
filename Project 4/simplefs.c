#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "simplefs.h"
#include <string.h>


// Global Variables =======================================
int vdisk_fd; // Global virtual disk file descriptor. Global within the library.
              // Will be assigned with the vsfs_mount call.
              // Any function in this file can use this.
              // Applications will not use  this directly.

struct superblockStruct {
    int noOfBlocks;
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
} typedef openFileEntry;

struct openFileTableStruct {
    openFileEntry entries[16];
} typedef openFileTable;

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


/**********************************************************************
   The following functions are to be called by applications directly.
***********************************************************************/

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

// this function is partially implemented.
int create_format_vdisk (char *vdiskname, unsigned int m)
{
    char command[1000];
    int size;
    int num = 1;
    int count;
    size  = num << m;
    count = size / BLOCKSIZE;
    //    printf ("%d %d", m, size);
    sprintf (command, "dd if=/dev/zero of=%s bs=%d count=%d",
             vdiskname, BLOCKSIZE, count);
    //printf ("executing command = %s\n", command);
    system (command);

    sfs_mount(vdiskname);

    // Initialize superblock
    superblock* info = (superblock*)malloc(sizeof(superblock));
    info->noOfBlocks = count;
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

    for (int i = 0; i < 13; ++i) {
        setBit(bitmap, i, 0);
    }
    write_block(bitmap, 1);

    /*
    for (int i = 0; i < 4096; ++i) {
        for (int j = 0; j < 8; ++j) {
            printf("%d", selectBit(x[i], j));
        }
    }
     */

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
    /*
    directoryEntry* x = malloc(sizeof(rootDirectory));
    read_block(x, 5);
    for (int i = 0; i < 32; ++i) {
        printf("filename: %s, index: %d\n",
               x->filename,
               x->fcbIndex);
        x++;
    }
     */


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
    /*
    FCB* x = malloc(sizeof(FCBTable));
    read_block(x, 9);
    for (int i = 0; i < 32; ++i) {
        printf("filesize: %d, inode: %d, used: %d\n",
               x->fileSize,
               x->inodeBlockNumber,
               x->used);
        x++;
    }
    */

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
    superblock* metadata = (superblock*) malloc(BLOCKSIZE);
    read_block(metadata, 0);
    int noOfBlocks = metadata->noOfBlocks;
    free(metadata);


    int foundBlockNumber = -1;
    int foundOffset = -1;
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
                printf("First empty directory found! blockNumber: %d, offset: %d\n",
                       i,j);
                foundBlockNumber = i;
                foundOffset = j;
            }
            if (strcmp(block->entries[j].filename, filename) == 0) {
                // Filename already exists in root directory
                printf("Filename is already exists in block %d, offset %d\n", i, j);
                alreadyAllocated = 1;
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

                    printf("Unused FCB found in block %d, offset %d\n", i, j);
                    // Search for empty block in bitmap
                    char bitmapBlock[4096];
                    int currentBlockIndex = 0;
                    for (int k = 1; k < 5; ++k) {
                        read_block(bitmapBlock, k);

                        for (int l = 0; l < 32768; ++l) {
                            if (getBit(bitmapBlock, l) == 1) {
                                foundFCBIndex = 32 * (i - 9) + j;

                                printf("Empty block is found from bitmap with number %d\n", 32768 * (k - 1) + l);
                                fcbBlock->fcbs[j].used = 1;
                                fcbBlock->fcbs[j].inodeBlockNumber = 32768 * (k - 1) + l;

                                write_block(fcbBlock, i);

                                setBit(bitmapBlock, l, 0);
                                write_block(bitmapBlock, k);

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

        if (foundFCBIndex != -1) {
            block = (rootDirectory*) malloc(sizeof(rootDirectory));
            read_block(block, foundBlockNumber);

            strcpy(block->entries[foundOffset].filename, filename);
            block->entries[foundOffset].fcbIndex = foundFCBIndex;

            write_block(block, foundBlockNumber);

            free(block);

            return 0;
        } else {
            return -1;
        }

    }
    return (0);
}


int sfs_open(char *file, int mode)
{
    return (0);
}

int sfs_close(int fd){
    return (0);
}

int sfs_getsize (int  fd)
{
    return (0);
}

int sfs_read(int fd, void *buf, int n){
    return (0);
}


int sfs_append(int fd, void *buf, int n)
{
    return (0);
}

int sfs_delete(char *filename)
{
    return (0);
}

