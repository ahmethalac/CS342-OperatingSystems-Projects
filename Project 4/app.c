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
    
    //CREATE
    printf ("creating files\n");
    sfs_create ("file1.bin");
    sfs_create ("file2.bin");
    sfs_create ("file3.bin");


    //OPEN
    fd1 = sfs_open ("file1.bin", MODE_APPEND);
    fd2 = sfs_open ("file2.bin", MODE_APPEND);

    //APPEND
    printf("Start writing\n");
    for (i = 0; i < 500; ++i) {
        buffer = "denemetest";
        sfs_append(fd1, (void *) buffer, 10);
    }

    printf("End writing. Updated file size: %d\n", sfs_getsize(fd1));

    //CLOSE
    sfs_close(fd1);

    //APPEND
    printf("Start writing\n");
    sfs_open("file1.bin", MODE_APPEND);
    for (i = 0; i < 500; ++i) {
        buffer = "testdeneme";
        if(sfs_append(fd1, (void *) buffer, 10)){
            printf("There is not enough space to append\n");
            break;
        }
    }

    printf("End writing. Updated file size: %d\n", sfs_getsize(fd1));

    //CLOSE
    sfs_close(fd1);
    
    //OPEN
    sfs_open("file1.bin", MODE_READ);

    //READ
    char x[10];
    for (i = 0; i < 1001; ++i) {
        if (sfs_read(fd1, x, 10) == -1) {
            printf("Cannot read\n");
            break;
        }
    }

    //CLOSE
    sfs_close(fd1);
    //DELETE
    sfs_delete("file1.bin");

    //CREATE
    sfs_create("file4.bin");

    //OPEN
    fd2 = sfs_open("file4.bin", MODE_APPEND );

    //APPEND
    for (i = 0; i < 500; ++i) {
        buffer = "testdeneme";
        sfs_append (fd2, (void *) buffer, 10);
    }

    //OPEN
    fd = sfs_open("file3.bin", MODE_READ);
    //GETSIZE
    size = sfs_getsize (fd);

    //READ
    for (i = 0; i < size; ++i) {
        sfs_read (fd, (void *) buffer, 1);
        c = (char) buffer[0];
        c = c + 1;
    }

    sfs_close (fd);
    ret = sfs_umount();
}
