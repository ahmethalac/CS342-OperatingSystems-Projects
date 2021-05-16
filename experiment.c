#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "simplefs.h"
#include <sys/time.h>

int main(int argc, char **argv)
{
    int ret;
    int fd1, fd2, fd; 
    int i;
    char c; 
    char buffer[16384];
    char buffer2[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    int size;
    char vdiskname[200];
    char buffer3[16384];
    struct timeval tv1;
    struct timeval tv2;
    double elapsedTime;

    for (i = 0; i < 16384; ++i) {
        buffer3[i] = (char) 65;
    }

    printf ("started\n");

    if (argc != 2) {
        printf ("usage: app  <vdiskname>\n");
        exit(0);
    }
    
    strcpy (vdiskname, argv[1]); 

    //MOUNT    
    ret = sfs_mount (vdiskname);
    if (ret != 0) {
        printf ("could not mount \n");
        exit (1);
    }

    //CREATE
    gettimeofday(&tv1, NULL);

    sfs_create ("file1.bin");

    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Creating time for file1: %f milliseconds\n", elapsedTime);

    //CREATE 2
    gettimeofday(&tv1, NULL);

    sfs_create ("file2.bin");

    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Creating time for file2: %f milliseconds\n", elapsedTime);

    //CREATE 3
    gettimeofday(&tv1, NULL);

    sfs_create ("file3.bin");

    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Creating time for file3: %f milliseconds\n", elapsedTime);


    // OPEN
    gettimeofday(&tv1, NULL);

    fd = sfs_open("file1.bin", MODE_APPEND);

    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Opening time: %f milliseconds\n", elapsedTime); 

    // APPEND 1
    gettimeofday(&tv1, NULL);

    for (i = 0; i < 16384; ++i) {
        sfs_append(fd, (void *) buffer2 + (i%8), 1);
    }

    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Appending 1: %f milliseconds\n", elapsedTime); 

    // APPEND 8
    gettimeofday(&tv1, NULL);

    for (i = 0; i < 16384/8; ++i) {
        sfs_append(fd, (void *) buffer2, 8);
    }

    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Appending 8: %f milliseconds\n", elapsedTime); 

    // APPEND 1024
    gettimeofday(&tv1, NULL);

    for (i = 0; i < 16384/1024; ++i) {
        sfs_append(fd, (void *) buffer3, 1024);
    }
    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Appending 1024: %f milliseconds\n", elapsedTime); 

    // APPEND 4096
    gettimeofday(&tv1, NULL);

    for (i = 0; i < 16384/4096; ++i) {
        sfs_append(fd, (void *) buffer3, 4096);
    }
    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Appending 4096: %f milliseconds\n", elapsedTime); 

    // APPEND 8192
    gettimeofday(&tv1, NULL);

    for (i = 0; i < 16384/8192; ++i) {
        sfs_append(fd, (void *) buffer3, 8192);
    }

    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Appending 8192: %f milliseconds\n", elapsedTime); 

    // APPEND ALL
    gettimeofday(&tv1, NULL);

    sfs_append(fd, (void *) buffer3, 16384);

    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Appending 16348: %f milliseconds\n", elapsedTime); 

    // CLOSE
    sfs_close (fd);

    // OPEN TO READ 
    fd = sfs_open("file1.bin", MODE_READ);

    // READ 1
    size = 16384;

    gettimeofday(&tv1, NULL);

    for (i = 0; i < size; ++i) {
        sfs_read (fd, (void *) buffer, 1);
    }

    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Reading 1: %f milliseconds\n", elapsedTime); 

    // READ 8
    gettimeofday(&tv1, NULL);

    for (i = 0; i < size/8; ++i) {
        sfs_read (fd, (void *) buffer, 8);
    }

    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Reading 8: %f milliseconds\n", elapsedTime); 

    // READ 1024
    gettimeofday(&tv1, NULL);

    for (i = 0; i < size/1024; ++i) {
        sfs_read (fd, (void *) buffer, 1024);
    }

    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Reading 1024: %f milliseconds\n", elapsedTime); 

    // READ 4096
    gettimeofday(&tv1, NULL);

    for (i = 0; i < size/1024; ++i) {
        sfs_read (fd, (void *) buffer, 1024);
    }

    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Reading 4096: %f milliseconds\n", elapsedTime); 

    // READ 8192
    gettimeofday(&tv1, NULL);

    for (i = 0; i < size/8192; ++i) {
        sfs_read (fd, (void *) buffer, 8192);
    }

    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Reading 8192: %f milliseconds\n", elapsedTime); 

    // READ 16384
    gettimeofday(&tv1, NULL);

    sfs_read (fd, (void *) buffer, 16384);

    gettimeofday(&tv2, NULL);
    elapsedTime = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // us to ms
    printf ("Reading 16384: %f milliseconds\n", elapsedTime); 

    //CLOSE
    sfs_close(fd);

    // DELETE
    sfs_delete("file1.bin");

    // UNMOUNT
    ret = sfs_umount();
}
