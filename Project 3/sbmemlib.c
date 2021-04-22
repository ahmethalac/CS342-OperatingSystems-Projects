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

// TODO Define semaphore
sem_t *sem;

// Define your stuctures and variables.
struct process {
    pid_t pid;
    void* offset;
};

struct spaceInformation {
    int nextFreeLocation;
    int size;
    int is_allocated;
};

struct sbmemInformation {
    struct spaceInformation dummyInformation; // To ease buddy algorithm
    int size;
    struct process processes[MAX_PROCESS_COUNT];
    // TODO Define buddy algorithm structures
    int freeSpaces[MAX_ORDER];
};

int getOrderFromSize(int segmentsize) {
    return ceil((int)(log2((double)segmentsize)));
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

    // TODO Initialize buddy algorithm structures
    printf("%d\n", getOrderFromSize(segmentsize));
    for (int i = 0; i < MAX_ORDER; ++i) {
        if (i == getOrderFromSize(segmentsize)) {
            info->freeSpaces[i] = 0;
            continue;
        }
        info->freeSpaces[i] = -1;
    }

    sbmem_alloc(sizeof(struct sbmemInformation));
    printf("sbmem init called\n"); // remove all printfs when you are submitting to us.

    //Initializing semaphore
    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if(sem != SEM_FAILED){
        printf("Semaphore is created succesfully!\n");
    }else{
        printf("Semaphore cannot be created succesfully!\n");
        exit(1);
    }
    return (0); 
}

int sbmem_remove()
{
    sem_wait(sem);
    shm_unlink(NAME);

    sem_post(sem);
    // TODO Remove semaphore
    sem_unlink(sem);
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
    // TODO Wait semaphore
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

    printf("%d, %d, %ld\n", newProcessIndex,info->processes[newProcessIndex].pid, info->processes[newProcessIndex].offset);


    //unmap the ptr 
    int success = munmap(ptr, info->size); 
    if(success == -1){
        printf("sbmem_open: unmapping cannot be done successfully!\n");
    }
    // TODO Signal semaphore
    sem_post(sem);
    return 0;
}

int extractFreeSpace(void* sbmemPtr, struct sbmemInformation* info, int size, int order, void* offset) {
    int result = info->freeSpaces[order];
    info->freeSpaces[order] = *(sbmemPtr + info->freeSpaces[order]);
    return offset == NULL ? result : result + sizeof(struct spaceInformation);
}

void addFreeSpace(void* sbmemPtr, struct sbmemInformation* info, int size, int offset, int order) {

}

void *sbmem_alloc (int size)
{
    // TODO Wait semaphore
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

    if (offset != NULL) {
        printf("%p %p", ptr, offset);
    }
    int requiredSize = offset == NULL ? size : size + sizeof(struct spaceInformation);
    int found = 0;
    int offsetOfSpace = -1;
    int order = getOrderFromSize(requiredSize);
    while (found == 0) {
        if (info->freeSpaces[order]) {
            offsetOfSpace = extractFreeSpace(ptr, info, requiredSize, order, offset);
            found = 1;
        } else if (order < MAX_ORDER) {
            order++;
            if (info->freeSpaces[order]) {
                addFreeSpace(ptr, info, requiredSize, );
            }
        } else {
            break;
        }
    }


    if (offset == NULL) {
        // Since we assume that alloc is not called before calling open, this must be process that calls init
        printf("%d %d\n", size,getOrderFromSize(size));

    }
    // TODO Find a location with buddy algorithm (For example (32,63))

    //unmap the ptr 
    int success = munmap(ptr, info->size); 
    if(success == -1){
        printf("sbmem_alloc: unmapping cannot be done successfully!\n");
    }

    // TODO Signal semaphore
    sem_post(sem);
    return offset; // + 32
}


void sbmem_free(void *p)
{
    // TODO Wait semaphore
    sem_wait(sem);

    //Post Semaphore
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


int find_buddy(int offset, int level ){
    int mod = offset % (int)pow(2,level+1);
    if(mod == 0){
        return offset + (int)pow(2,level);
    }else{
        return offset - (int)pow(2,level);
    }
}