#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <limits.h>

// Struct definitions
struct wThreadArgs {
    int threadID;
    int readFromFile; // boolean (0 for random burst, 1 for read from file)
    char* inprefix; // Filename prefix
    int Bcount; // Burst count
    int minB, avgB, minA, avgA;
} typedef wThreadArgs;

struct sThreadArgs {
    int N;
    char* ALG;
} typedef sThreadArgs;

struct burst {
    int threadIndex;
    int burstIndex;
    int length;
    long generationTime;
    SLIST_ENTRY(burst) next;
} typedef burstNode;

// Global variables
int count = 0;
int done = 0;
long timeOffset; // Initial time of main function
struct rqHead head; // head of runqueue
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t rqEmpty = PTHREAD_COND_INITIALIZER;

SLIST_HEAD(rqHead, burst);

// Helper functions
double expRandom(unsigned int* seed, double mean) {
    return -mean * log(rand_r(seed) / (RAND_MAX + 1.0)); // Using inverse method
}

long getCurrentTime() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return (currentTime.tv_sec * 1000000 + currentTime.tv_usec) / 1000;
}

void createBurst(int threadIndex, int burstIndex, int length, int sleepAmount) {
    //printf("[%ld] %dth thread sleeps for %d\n",
    //       getCurrentTime() - timeOffset, threadIndex, sleepAmount);

    usleep(sleepAmount * 1000);

    burstNode* temp = malloc(sizeof(burstNode));
    temp->threadIndex = threadIndex;
    temp->burstIndex = burstIndex;
    temp->length = length;

    pthread_mutex_lock(&lock);

    temp->generationTime = getCurrentTime();
    //printf("[%ld] %d.%d(%d) is created\n",
    //       getCurrentTime() - timeOffset, threadIndex, burstIndex, length);

    // Add burst to the end of runqueue
    if (!SLIST_EMPTY(&head)) {
        burstNode* last = SLIST_FIRST(&head);
        while (SLIST_NEXT(last, next)) {
            last = SLIST_NEXT(last, next);
        }
        SLIST_INSERT_AFTER(last, temp, next);
    } else {
        SLIST_INSERT_HEAD(&head, temp, next);
    }

    pthread_cond_signal(&rqEmpty); // Signal S_thread about burst creation
    pthread_mutex_unlock(&lock);
}

void createBurstsFromFile(char* inprefix, int ID) {
    FILE *fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;

    char fileName[strlen(inprefix) + 7];
    char format[strlen(inprefix)];
    strcpy(format, inprefix);
    sprintf(fileName, strcat(format, "-%d.txt"), ID);

    fp = fopen(fileName, "r");
    int burstIndex = 1;
    while ((read = getline(&line, &len, fp)) != -1) {
        int sleepAmount, burstAmount;
        sscanf(line, "%d %d", &sleepAmount, &burstAmount);

        createBurst(ID, burstIndex, burstAmount, sleepAmount);
        burstIndex++;
    }
    free(line);
}

burstNode* sjfAlgorithm(int N) {
    burstNode* chosenBurst;

    //For each thread, keep the information that whether earliest burst of this thread is found (1 if found)
    int foundEarliestBurst[N];
    for (int i = 0; i < N; ++i) {
        foundEarliestBurst[i] = 0;
    }

    burstNode* foundBurst;
    int minBurst = INT_MAX;
    SLIST_FOREACH(foundBurst, &head, next) {
        if (foundEarliestBurst[foundBurst->threadIndex - 1] == 1) {
            continue;
        }
        foundEarliestBurst[foundBurst->threadIndex - 1] = 1;
        if (foundBurst->length < minBurst) {
            chosenBurst = foundBurst;
            minBurst = foundBurst->length;
        }
    }

    return chosenBurst;
}

burstNode* prioAlgorithm(int N) {
    burstNode* chosenBurst;

    //For each thread, keep the information that whether earliest burst of this thread is found (1 if found)
    int foundEarliestBurst[N];
    for (int i = 0; i < N; ++i) {
        foundEarliestBurst[i] = 0;
    }

    burstNode* foundBurst;
    int highestPriority = N + 1;
    SLIST_FOREACH(foundBurst, &head, next) {
        if (foundEarliestBurst[foundBurst->threadIndex - 1] == 1) {
            continue;
        }
        foundEarliestBurst[foundBurst->threadIndex - 1] = 1;
        if (foundBurst->threadIndex < highestPriority) {
            chosenBurst = foundBurst;
            highestPriority = foundBurst->threadIndex;
        }
    }

    return chosenBurst;
}

burstNode* vruntimeAlgorithm(int* vruntime, int N) {
    burstNode* chosenBurst;

    //For each thread, keep the information that whether earliest burst of this thread is found (1 if found)
    int foundEarliestBurst[N];
    for (int i = 0; i < N; ++i) {
        foundEarliestBurst[i] = 0;
    }

    burstNode* foundBurst;
    int minVruntime = INT_MAX;
    SLIST_FOREACH(foundBurst, &head, next) {
        if (foundEarliestBurst[foundBurst->threadIndex - 1] == 1) {
            continue;
        }
        foundEarliestBurst[foundBurst->threadIndex - 1] = 1;
        if (vruntime[foundBurst->threadIndex - 1] < minVruntime) {
            chosenBurst = foundBurst;
            minVruntime = vruntime[foundBurst->threadIndex - 1];
        }
    }
    vruntime[chosenBurst->threadIndex - 1] += chosenBurst->length * (0.7 + 0.3 * chosenBurst->threadIndex);
    return chosenBurst;
}

// Thread runners
void* W_thread(void* args) {
    // Get args
    int ID = ((wThreadArgs*)args)->threadID;
    int readFromFile = ((wThreadArgs*)args)->readFromFile;
    char* inprefix = ((wThreadArgs*)args)->inprefix;
    int Bcount = ((wThreadArgs*)args)->Bcount;
    int minB = ((wThreadArgs*)args)->minB;
    int avgB = ((wThreadArgs*)args)->avgB;
    int minA = ((wThreadArgs*)args)->minA;
    int avgA = ((wThreadArgs*)args)->avgA;

    // Set different seed for each thread
    unsigned int seed = ID;

    if (readFromFile == 1) { //Read bursts from file
        createBurstsFromFile(inprefix, ID);
    } else if (readFromFile == 0){
        for (int i = 1; i < Bcount + 1; i++) {
            int sleepAmount;
            do {
                sleepAmount = (int)expRandom(&seed, (double)avgA);
            } while (sleepAmount < minA);

            int burstAmount;
            do {
                burstAmount = (int)expRandom(&seed, (double)avgB);
            } while (burstAmount < minB);

            createBurst(ID, i, burstAmount, sleepAmount);
        }
    }
    return 0;
}

void* S_thread(void* args) {
    // Get args
    int N = ((sThreadArgs*)args)->N;
    char* ALG = ((sThreadArgs*)args)->ALG;

    int vruntime[N], burstCount[N], totalWaitingTime[N];
    for (int i = 0; i < N; ++i) {
        vruntime[i] = 0;
        burstCount[i] = 0;
        totalWaitingTime[i] = 0;
    }

    while (done == 0 || !SLIST_EMPTY(&head)) {
        pthread_mutex_lock(&lock);
        if (SLIST_EMPTY(&head)) {
            //printf("[%ld] Waiting for a burst in S_thread\n", getCurrentTime() - timeOffset);
            pthread_cond_wait(&rqEmpty, &lock);
            //printf("[%ld] New burst arrived, finish waiting in S_thread\n", getCurrentTime() - timeOffset);
        }

        burstNode* chosenBurst;
        if (strcmp(ALG, "FCFS") == 0) {
            chosenBurst = SLIST_FIRST(&head);
        } else if (strcmp(ALG, "SJF") == 0) {
            chosenBurst = sjfAlgorithm(N);
        } else if (strcmp(ALG, "PRIO") == 0) {
            chosenBurst = prioAlgorithm(N);
        } else if (strcmp(ALG, "VRUNTIME") == 0) {
            chosenBurst = vruntimeAlgorithm(vruntime, N);
        }

        burstCount[chosenBurst->threadIndex - 1]++;
        totalWaitingTime[chosenBurst->threadIndex - 1] += getCurrentTime() - chosenBurst->generationTime;
        SLIST_REMOVE(&head, chosenBurst, burst, next);

        //printf("[%ld] Running %d.%d(%d) -- ",
        //       getCurrentTime() - timeOffset, chosenBurst->threadIndex, chosenBurst->burstIndex, chosenBurst->length);
        //printf("Waiting time is: %ld\n", getCurrentTime() - chosenBurst->generationTime);

        pthread_mutex_unlock(&lock);
        usleep(chosenBurst->length * 1000);
        free(chosenBurst);
    }

    /*
    printf("Average waiting times:\n");
    int totalSum = 0;
    int totalBurstCount = 0;
    for (int i = 0; i < N; ++i) {
        printf("Thread %d: %.2f\n", i + 1, totalWaitingTime[i] / (double) burstCount[i]);
        totalBurstCount += burstCount[i];
        totalSum += totalWaitingTime[i];
    }
    printf("Total average waiting time: %.2f\n", totalSum / (double)totalBurstCount );
    */
    return 0;
}

int main(int argc, char *argv[]) {
    // Initial preparations
    timeOffset = getCurrentTime();
    SLIST_INIT(&head);
    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("Failure of mutex init\n");
        return 0;
    }
    int N = atoi(argv[1]);
    sThreadArgs* sThreadArg = (sThreadArgs*)malloc(sizeof(sThreadArgs));
    sThreadArg->N = N;

    pthread_t tid[N];
    pthread_t s_thread_id;
    wThreadArgs* args[N];
    char* ALG;

    if (argc == 5) { //Get bursts from file
        ALG = argv[2];
        sThreadArg->ALG = ALG;
        pthread_create(&s_thread_id, NULL, S_thread, sThreadArg); // S_Thread

        for (int i = 0; i < N; i++) { // W_Threads
            args[i] = (wThreadArgs*)malloc(sizeof(wThreadArgs));
            args[i]->readFromFile = 1;
            args[i]->inprefix = argv[4];
            args[i]->threadID = i + 1;
            pthread_create(&tid[i], NULL, W_thread, (void*)args[i]);
        }
    } else {
        int Bcount = atoi(argv[2]), minB = atoi(argv[3]),
            avgB = atoi(argv[4]), minA = atoi(argv[5]),
            avgA = atoi(argv[6]);
        ALG = argv[7];

        sThreadArg->ALG = ALG;
        pthread_create(&s_thread_id, NULL, S_thread, sThreadArg); // S_Thread

        for (int i = 0; i < N; i++) { // W_Threads
            args[i] = (wThreadArgs*)malloc(sizeof(wThreadArgs));
            args[i]->readFromFile = 0;
            args[i]->Bcount = Bcount;
            args[i]->minB = minB;
            args[i]->avgB = avgB;
            args[i]->minA = minA;
            args[i]->avgA = avgA;
            args[i]->threadID = i + 1;
            pthread_create(&tid[i], NULL, W_thread, (void*)args[i]);
        }
    }
    // Join W_Threads and free memory allocations
    for (int i = 0; i < N; ++i) {
        pthread_join(tid[i], NULL);
    }
    for (int i = 0; i < N; ++i) {
        free(args[i]);
    }
    done = 1;

    // Join S_Thread and free memory allocation
    pthread_join(s_thread_id, NULL);
    free(sThreadArg);

    pthread_mutex_destroy(&lock);

    return 0;
}