#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

//GLobal variables
int count = 0;

//Struct definitions
struct args {
    int threadID;
    int readFromFile; //boolean (0 for random burst, 1 for read from file)
    char* inprefix;
    int Bcount;
    int minB, avgB, minA, avgA;
    char* ALG;
} typedef threadArgs;

struct burst {
    int threadIndex;
    int burstIndex;
    int length;
    int generationTime;
} typedef burst;

//Helper functions
double expRandom(double mean) {
    return -mean * log(rand() / (RAND_MAX + 1.0));
}

//Thread functions
void* W_thread(void* args) {
    int ID = ((threadArgs*)args)->threadID;
    int readFromFile = ((threadArgs*)args)->readFromFile;
    char* inprefix = ((threadArgs*)args)->inprefix;
    int Bcount = ((threadArgs*)args)->Bcount;
    int minB = ((threadArgs*)args)->minB;
    int avgB = ((threadArgs*)args)->avgB;
    int minA = ((threadArgs*)args)->minA;
    int avgA = ((threadArgs*)args)->avgA;
    char* ALG = ((threadArgs*)args)->ALG;

    if (readFromFile == 1) {
        FILE *fp;
        char* line = NULL;
        size_t len = 0;
        ssize_t read;

        char fileName[strlen(inprefix) + 7];
        char format[strlen(inprefix)];
        strcpy(format, inprefix);
        sprintf(fileName, strcat(format, "-%d.txt"), ID);

        fp = fopen(fileName, "r");
        while ((read = getline(&line, &len, fp)) != -1) {
            int sleepAmount, burstAmount;
            sscanf(line, "%d %d", &sleepAmount, &burstAmount);

            printf("ThreadID: %d, Sleep Amount: %d\n", ID, sleepAmount);
            usleep(sleepAmount * 1000);

            printf("ThreadID: %d, Burst Amount: %d\n", ID, burstAmount);
        }
        free(line);

    } else if (readFromFile == 0){
        srand(ID * time(NULL));
        for (int i = 1; i < Bcount + 1; i++) {
            int sleepAmount;
            do {
                sleepAmount = (int)expRandom((double)avgA);
            } while (sleepAmount < minA);
            printf("ThreadID: %d, Sleep Amount: %d\n", ID, sleepAmount);
            usleep(sleepAmount * 1000);

            int burstAmount;
            do {
                burstAmount = (int)expRandom((double)avgB);
            } while (burstAmount < minB);
            printf("ThreadID: %d, Burst Amount: %d\n", ID, burstAmount);
        }
    }

    pthread_exit(0);
}

int main(int argc, char *argv[]) {
    int N = atoi(argv[1]);
    char* ALG;
    pthread_t tid[N];
    if (argc == 5) { //Get bursts from file
        ALG = argv[2];
        char* inprefix = argv[4];

        for (int i = 1; i < N + 1; i++) {
            threadArgs* args = (threadArgs*)malloc(sizeof(threadArgs));
            args->readFromFile = 1;
            args->inprefix = inprefix;
            args->ALG = ALG;
            args->threadID = i;
            pthread_create(&tid[i - 1], NULL, W_thread, (void*)args);
        }
        for (int i = 1; i < N + 1; ++i) {
            pthread_join(tid[i - 1], NULL);
        }
    } else {
        int Bcount = atoi(argv[2]), minB = atoi(argv[3]),
            avgB = atoi(argv[4]), minA = atoi(argv[5]),
            avgA = atoi(argv[6]);
        ALG = argv[7];

        for (int i = 1; i < N + 1; i++) {
            threadArgs* args = (threadArgs*)malloc(sizeof(threadArgs));
            args->readFromFile = 0;
            args->Bcount = Bcount;
            args->minB = minB;
            args->avgB = avgB;
            args->minA = minA;
            args->avgA = avgA;
            args->ALG = ALG;
            args->threadID = i;
            pthread_create(&tid[i - 1], NULL, W_thread, (void*)args);
        }
        for (int i = 1; i < N + 1; ++i) {
            pthread_join(tid[i - 1], NULL);
        }
    }
    return 0;
}