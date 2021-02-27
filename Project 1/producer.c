#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>

char randChar() {
    return "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"[rand() % 62];
}

int main(int argc, char *argv[])
{
    srand(time(NULL));
    char c;
    for (int i = 0; i < atoi(argv[1]); i++) {
        c = randChar();
        printf("%c", c);
    }
}
