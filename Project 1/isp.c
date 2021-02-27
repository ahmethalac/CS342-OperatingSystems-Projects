#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

#define WRITE_END 1
#define READ_END 0
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
        printf("isp$ ");

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
            if (argc == 3 && strcmp(argv[2], "1") == 0) { //Normal mode
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
                    }
                }

            }
        }


    }
    return 0;
}
