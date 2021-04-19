#include "sbmem.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

#define NAME "/sbmem"
#define MAX_PROCESS_COUNT 10

// TODO Define semaphore

// Define your stuctures and variables.
struct process {
    pid_t pid;
    void* offset;
};

struct sbmemInformation {
    int size;
    struct process processes[MAX_PROCESS_COUNT];
    // TODO Define buddy algorithm structures
};

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

    sbmem_alloc(sizeof(struct sbmemInformation));
    printf("sbmem init called\n"); // remove all printfs when you are submitting to us.
    return (0); 
}

int sbmem_remove()
{
    shm_unlink(NAME);
    // TODO Remove semaphore
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
    // TODO Signal semaphore
    return 0;
}


void *sbmem_alloc (int size)
{
    // TODO Wait semaphore
    void *ptr = getMMapForSegment();
    struct sbmemInformation* info = (struct sbmemInformation*)ptr;

    void* offset = NULL;
    for (int i = 0; i < MAX_PROCESS_COUNT; ++i) {
        if (info->processes[i].pid == getpid()) {
            offset = info->processes[i].offset;
            break;
        }
    }

    // TODO Find a location with buddy algorithm (For example (32,63))

    // TODO Signal semaphore
    return 0; // + 32
}


void sbmem_free(void *p)
{
    // TODO Wait semaphore
 
}

int sbmem_close()
{
    
    return (0); 
}
