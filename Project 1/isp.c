#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>

#define WRITE_END 1
#define READ_END 0

void printElapsedTime(struct timeval start, struct timeval end) {
    long microsecond = ((end.tv_sec * 1000000 + end.tv_usec) -
                        (start.tv_sec * 1000000 + start.tv_usec));
    printf("\033[1;32m");
    printf("Done in ");
    if (microsecond / 1000000 > 0) {
        printf("%ld seconds, ", microsecond / 1000000);
    }
    if (microsecond / 1000 > 0) {
        printf("%ld milliseconds, ", (microsecond % 1000000) / 1000);
    }
    printf("%ld microseconds!\n", (microsecond % 1000));
    printf("\033[0m");
}

char** getArgs(char* command) {
    int argCount = 1;
    for (int i = 0; i < 256; i++) {
        if (command[i] == ' ') {
            argCount++;
        } else if (command[i] == '\n') {
            command[i] = '\0';
            break;
        }
    }

    char **args = malloc(sizeof(char*) * (argCount + 1));

    char *token = strtok(command, " ");
    for (argCount = 0; token != NULL; argCount++) {
        args[argCount] = token;
        token = strtok(NULL, " ");
    }
    args[argCount] = NULL;

    return args;
}

int main(int argc, char *argv[]) {
    while (1) {
        printf("\033[;34m");
        printf("isp$ ");
        printf("\033[0m");

        char command[256];
        fgets(command, 256, stdin);

        if (strcmp(command, "exit\n") == 0) {
            break;
        }

        char *firstCommand = strtok(command, "|");
        char *secondCommand = strtok(NULL, "|");

        if (secondCommand == NULL) { //Classic mode
            if (fork() == 0) {
                char **args = getArgs(firstCommand);
                execvp(args[0], args);
            } else {
                wait(NULL);
            }
        } else { //Composition mode
            if (argc == 3) { //Arguments are given correctly
                //struct timeval start, end;
                //gettimeofday(&start, NULL);

                if (strcmp(argv[2], "1") == 0) { //Normal mode
                    int fd[2];
                    pipe(fd);

                    if (fork() == 0) {
                        dup2(fd[WRITE_END], STDOUT_FILENO);
                        close(fd[READ_END]);
                        close(fd[WRITE_END]);

                        char **args = getArgs(firstCommand);
                        execvp(args[0], args);
                    } else {
                        if (fork() == 0) {
                            dup2(fd[READ_END], STDIN_FILENO);
                            close(fd[READ_END]);
                            close(fd[WRITE_END]);

                            char **args = getArgs(secondCommand);
                            execvp(args[0], args);
                        } else {
                            close(fd[READ_END]);
                            close(fd[WRITE_END]);

                            wait(NULL);
                            wait(NULL);
                        }
                    }
                } else { //Tapped mode
                    int fd1[2];
                    int fd2[2];
                    const int MSGSIZE = atoi(argv[1]);
                    pipe(fd1);
                    pipe(fd2);

                    if (fork() == 0) {
                        dup2(fd1[WRITE_END], STDOUT_FILENO);
                        close(fd2[READ_END]);
                        close(fd2[WRITE_END]);
                        close(fd1[READ_END]);
                        close(fd1[WRITE_END]);

                        char **args = getArgs(firstCommand);
                        execvp(args[0], args);
                    } else {
                        if (fork() == 0) {
                            dup2(fd2[READ_END], STDIN_FILENO);
                            close(fd1[READ_END]);
                            close(fd1[WRITE_END]);
                            close(fd2[READ_END]);
                            close(fd2[WRITE_END]);

                            char **args = getArgs(secondCommand);
                            execvp(args[0], args);
                        } else {
                            char buffer[MSGSIZE];
                            close(fd1[WRITE_END]);
                            close(fd2[READ_END]);

                            int operationCount = 0;
                            int totalBytes = 0;
                            int bytes;

                            while ((bytes = read(fd1[READ_END], buffer, MSGSIZE)) > 0) {
                                if (write(fd2[WRITE_END], buffer, bytes) == -1) {
                                    break;
                                }
                                operationCount++;
                                totalBytes += bytes;
                            }

                            close(fd2[WRITE_END]);
                            close(fd1[READ_END]);

                            wait(NULL);
                            wait(NULL);

                            printf("\033[1;33m");
                            printf("character-count: %d\n", totalBytes);
                            printf("read-call-count: %d\n", operationCount);
                            printf("write-call-count: %d\n", operationCount);
                        }
                    }
                }

                //gettimeofday(&end, NULL);
                //printElapsedTime(start, end);
            } else {
                printf("Arguments are not given correctly!\n"
                       "Please run the program with two arguments "
                       "in the format that is specified in project description!\n");
                break;
            }
        }
    }
    return 0;
}
