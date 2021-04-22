#include "sbmem.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <semaphore.h>
#include <math.h>

#define NAME "/sbmem"
#define SEM_NAME "/SEM"
#define MAX_PROCESS_COUNT 10
#define MAX_ORDER 19

sem_t *sem;

struct process {
    pid_t pid;
    void* offset;
};

struct spaceInformation {
    int nextFreeLocation;
    int order;
    int is_allocated;
};

struct sbmemInformation {
    struct spaceInformation dummyInformation; // To ease buddy algorithm
    int size;
    struct process processes[MAX_PROCESS_COUNT];
    int freeSpaces[MAX_ORDER];
};

int getOrderFromSize(int segmentsize) {
    return (int)ceil(log2((double)segmentsize));
}

int sbmem_init(int segmentsize)
{
    int sbmem_fd = shm_open(NAME, O_CREAT | O_RDWR | O_EXCL, 0666);

    if (sbmem_fd == -1) {
        // There is already a shared segment
        sbmem_remove();
        sbmem_fd = shm_open(NAME, O_CREAT | O_RDWR | O_EXCL, 0666);
    }

    if (sbmem_fd == -1) {
        // Unsuccessful allocation
        return -1;
    }

    ftruncate(sbmem_fd, segmentsize);
    void *ptr = mmap(NULL, sizeof(struct sbmemInformation), PROT_WRITE | PROT_READ, MAP_SHARED, sbmem_fd, 0);
    struct sbmemInformation* info = (struct sbmemInformation*)ptr;
    info->size = segmentsize;
    for (int i = 0; i < MAX_PROCESS_COUNT; ++i) {
        struct process p;
        p.pid = -1;
        info->processes[i] = p;
    }

    for (int i = 0; i < MAX_ORDER; ++i) {
        if (i == getOrderFromSize(segmentsize)) {
            info->freeSpaces[i] = 0;
            continue;
        }
        info->freeSpaces[i] = -1;
    }

    info->dummyInformation.nextFreeLocation = -1;
    info->dummyInformation.order = (int)log2(segmentsize);
    info->dummyInformation.is_allocated = 1;

    printf("sbmem init called\n"); // remove all printfs when you are submitting to us.

    //Initializing semaphore
    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if(sem != SEM_FAILED){
        printf("Semaphore is created succesfully!\n");
    }else{
        printf("Semaphore cannot be created succesfully!\n");
        exit(1);
    }

    sbmem_alloc(sizeof(struct sbmemInformation));
    return (0);
}

int sbmem_remove()
{
    //Initializing semaphore
    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if(sem != SEM_FAILED){
        printf("Semaphore is created succesfully!\n");
    }else{
        printf("Semaphore cannot be created succesfully!\n");
        exit(1);
    }

    shm_unlink(NAME);
    sem_unlink(SEM_NAME);
    return (0);
}

void *getMMapForSegment() {
    int sbmem_fd = shm_open(NAME, O_RDWR, 0666);
    void *ptr = mmap(NULL, sizeof(struct sbmemInformation), PROT_WRITE | PROT_READ, MAP_SHARED, sbmem_fd, 0);
    struct sbmemInformation* info = (struct sbmemInformation*)ptr;
    int size = info->size;
    munmap(ptr, sizeof(struct sbmemInformation));
    ftruncate(sbmem_fd, size);
    return mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, sbmem_fd, 0);
}

int sbmem_open()
{
    sem = sem_open(SEM_NAME, 0); //Open the shared semaphore to be used by processes that will use the library
    sem_wait(sem);

    void* ptr = getMMapForSegment();
    struct sbmemInformation* info = (struct sbmemInformation*)ptr;

    // Find empty slot in process array
    int newProcessIndex = -1;
    for (int i = 0; i < MAX_PROCESS_COUNT; ++i) {
        if (info->processes[i].pid == -1) {
            newProcessIndex = i;
            break;
        }
    }

    if (newProcessIndex == -1) {
        // There is already 10 process
        return -1;
    }

    info->processes[newProcessIndex].pid = getpid();
    info->processes[newProcessIndex].offset = ptr;

    //unmap the ptr
    int success = munmap(ptr, info->size);
    if(success == -1){
        printf("sbmem_open: unmapping cannot be done successfully!\n");
    }

    sem_post(sem);
    return 0;
}

int extractFreeSpace(void* sbmemPtr, struct sbmemInformation* info, int order) {
    int result = info->freeSpaces[order];
    struct spaceInformation* allocatedSpace = (struct spaceInformation*)(sbmemPtr + info->freeSpaces[order]);
    info->freeSpaces[order] = allocatedSpace->nextFreeLocation;
    allocatedSpace->is_allocated = 1;
    return result;
}

void addSpace(void *sbmemPtr, struct sbmemInformation* info, int offset, int order) {
    int foundLocation = info->freeSpaces[order];
    if (foundLocation == -1 || foundLocation > offset) {
        struct spaceInformation *overhead = (struct spaceInformation *)(sbmemPtr + offset);
        overhead->nextFreeLocation = foundLocation;
        overhead->order = order;
        overhead->is_allocated = 0;
        info->freeSpaces[order] = offset;
    } else {
        while (1) {
            struct spaceInformation* spaceInfo = (struct spaceInformation *) (sbmemPtr + foundLocation);
            if ( spaceInfo->nextFreeLocation == -1 || spaceInfo->nextFreeLocation > offset) {
                struct spaceInformation *overhead = sbmemPtr + offset;
                overhead->nextFreeLocation = spaceInfo->nextFreeLocation;
                overhead->order = order;
                overhead->is_allocated = 0;
                spaceInfo->nextFreeLocation = offset;
                break;
            } else {
                foundLocation = spaceInfo->nextFreeLocation;
            }
        }
    }
}

void removeSpace(void *sbmemPtr, struct sbmemInformation* info, int offset, int order) {
    int foundLocation = info->freeSpaces[order];
    if (foundLocation == offset) {
        struct spaceInformation* removedSpaceInformation = (struct spaceInformation *) (sbmemPtr + offset);
        info->freeSpaces[order] = removedSpaceInformation->nextFreeLocation;
    } else if (foundLocation != -1){
        while (1) {
            struct spaceInformation* spaceInfo = (struct spaceInformation *) (sbmemPtr + foundLocation);
            if (spaceInfo->nextFreeLocation == offset) {
                struct spaceInformation* removedSpaceInformation = (struct spaceInformation *) (sbmemPtr + offset);
                spaceInfo->nextFreeLocation = removedSpaceInformation->nextFreeLocation;
                break;
            } else {
                foundLocation = spaceInfo->nextFreeLocation;
            }
        }
    }
}

void divideSpace(void* sbmemPtr, struct sbmemInformation* info, int offset, int order) {
    removeSpace(sbmemPtr, info, offset, order);
    addSpace(sbmemPtr, info, offset, order - 1);
    addSpace(sbmemPtr, info, offset + (int)pow(2, order - 1), order - 1);
}

int mergeSpace(void* sbmemPtr, struct sbmemInformation* info, int selfOffset, int buddyOffset,
        int selfAllocated, int buddyAllocated, int order) {
    if (selfAllocated == 0) {
        removeSpace(sbmemPtr, info, selfOffset, order);
    }
    if (buddyAllocated == 0) {
        removeSpace(sbmemPtr, info, buddyOffset, order);
    }
    if (selfOffset < buddyOffset) {
        addSpace(sbmemPtr, info, selfOffset, order + 1);
        return selfOffset;
    } else {
        addSpace(sbmemPtr, info, buddyOffset, order + 1);
        return buddyOffset;
    }
}

void *sbmem_alloc (int size)
{
    sem_wait(sem);

    void *ptr = getMMapForSegment();
    struct sbmemInformation* info = (struct sbmemInformation*)ptr;

    void* offset = NULL;
    for (int i = 0; i < MAX_PROCESS_COUNT; ++i) {
        if (info->processes[i].pid == getpid()) {
            offset = info->processes[i].offset;
            break;
        }
    }

    int requiredSize = size + sizeof(struct spaceInformation);
    int found = 0;
    int offsetOfSpace = -1;
    int order = getOrderFromSize(requiredSize);

    //Experiment
    FILE *output = fopen("output.txt", "a");
    fprintf(output, "Internal fragmentation: %d\n", (int)pow(2, order) - size);

    while (found == 0) {
        if (info->freeSpaces[order] != -1) {
            offsetOfSpace = extractFreeSpace(ptr, info, order);
            found = 1;
        } else if (order < MAX_ORDER - 1) {
            order++;
            if (info->freeSpaces[order] != -1) {
                divideSpace(ptr, info, info->freeSpaces[order], order);
                order = getOrderFromSize(requiredSize);
            }
        } else {
            break;
        }
    }

    sem_post(sem);
    return (offset + offsetOfSpace + (int)sizeof(struct spaceInformation));
}

int find_buddy(int offset, int order){
    int mod = offset % (int)pow(2,order+1);
    if(mod == 0){
        return offset + (int)pow(2,order);
    }else{
        return offset - (int)pow(2,order);
    }
}

void sbmem_free(void *p)
{
    sem_wait(sem);

    void *ptr = getMMapForSegment();
    struct sbmemInformation* info = (struct sbmemInformation*)ptr;

    void* offset = NULL;
    for (int i = 0; i < MAX_PROCESS_COUNT; ++i) {
        if (info->processes[i].pid == getpid()) {
            offset = info->processes[i].offset;
            break;
        }
    }

    struct spaceInformation* spaceInfo = (struct spaceInformation*)(p - sizeof(struct spaceInformation));

    int buddyOffset = find_buddy(p - offset - sizeof(struct spaceInformation), spaceInfo->order);

    struct spaceInformation* buddyInfo = (struct spaceInformation*)(offset + buddyOffset);

    if (buddyInfo->is_allocated == 1) {
        addSpace(ptr, info, p - offset - sizeof(struct spaceInformation), spaceInfo->order);
    } else {
        int buddyIsAllocated = buddyInfo->is_allocated;
        int order = spaceInfo->order;
        while (buddyIsAllocated == 0) {
            order++;
            int mergedOffset = mergeSpace(ptr, info, p - offset - sizeof(struct spaceInformation), buddyOffset,
                                        spaceInfo->is_allocated, buddyInfo->is_allocated,spaceInfo->order);
            buddyOffset = find_buddy(mergedOffset, order);
            struct spaceInformation* buddyInfo = (struct spaceInformation*)(offset + buddyOffset);
            buddyIsAllocated = buddyInfo->is_allocated;
        }
    }
    sem_post(sem);
}

int sbmem_close()
{
    sem_wait(sem);
    pid_t closedProcessID = getpid();

    void* ptr = getMMapForSegment();
    struct sbmemInformation* info = (struct sbmemInformation*)ptr;

    // Find empty slot in process array
    int closedProcessIndex = -1;
    for (int i = 0; i < MAX_PROCESS_COUNT; ++i) {
        if (info->processes[i].pid == closedProcessID) {
            closedProcessIndex = i;
            break;
        }
    }

    if (closedProcessIndex == -1) {
        // There is no procedd with the given pid
        printf("The process is not already using the library!\n");
        return -1;
    }

    info->processes[closedProcessIndex].pid = -1;
    int ret = munmap(info->processes[closedProcessIndex].offset, info->size);
    if(ret == -1){
        printf("sbmem_close: unmapping for specified process cannot be done successfully!\n");
    }

    //unmap the ptr
    int success = munmap(ptr, info->size);
    if(success == -1){
        printf("sbmem_close: ptr unmapping cannot be done successfully!\n");
    }
    sem_post(sem);
    sem_close(sem);
    return (0);
}