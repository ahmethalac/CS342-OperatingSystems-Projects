#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "simplefs.h"

int main(int argc, char **argv)
{
    int ret;
    int fd1, fd2, fd; 
    int i;
    char c; 
    char* buffer;
    char buffer2[8] = {50, 50, 50, 50, 50, 50, 50, 50};
    int size;
    char vdiskname[200]; 

    printf ("started\n");

    if (argc != 2) {
        printf ("usage: app  <vdiskname>\n");
        exit(0);
    }
    strcpy (vdiskname, argv[1]); 
    
    ret = sfs_mount (vdiskname);
    if (ret != 0) {
        printf ("could not mount \n");
        exit (1);
    }

    printf ("creating files\n"); 
    sfs_create ("file1.bin");
    sfs_create ("file2.bin");
    sfs_create ("file3.bin");


    fd1 = sfs_open ("file1.bin", MODE_APPEND);
    fd2 = sfs_open ("file2.bin", MODE_APPEND);

    for (i = 0; i < 500; ++i) {
        buffer = "denemetest";
        sfs_append(fd1, (void *) buffer, 10);
    }

    sfs_close(fd1);
    sfs_open("file1.bin", MODE_APPEND);
    for (i = 0; i < 500; ++i) {
        buffer = "testdeneme";
        sfs_append (fd1, (void *) buffer, 10);
    }

    sfs_close(fd1);
    sfs_open("file1.bin", MODE_READ);

    char x[10];
    for (i = 0; i < 1000; ++i) {
        if (sfs_read(fd1, x, 10) == -1) {
            printf("Cannot read\n");
            break;
        }
        printf("%s\n", x);
    }

    sfs_close(fd1);
    sfs_delete("file1.bin");

    sfs_create("file4.bin");

    fd2 = sfs_open("file4.bin", MODE_APPEND );

    for (i = 0; i < 500; ++i) {
        buffer = "testdeneme";
        sfs_append (fd2, (void *) buffer, 10);
    }
    return 0;

    fd = sfs_open("file3.bin", MODE_APPEND);
    for (i = 0; i < 10000; ++i) {
        memcpy (buffer, buffer2, 8); // just to show memcpy
        sfs_append(fd, (void *) buffer, 8);
    }
    sfs_close (fd);

    fd = sfs_open("file3.bin", MODE_READ);
    size = sfs_getsize (fd);
    for (i = 0; i < size; ++i) {
        sfs_read (fd, (void *) buffer, 1);
        c = (char) buffer[0];
        c = c + 1;
    }
    sfs_close (fd);
    ret = sfs_umount();
}
