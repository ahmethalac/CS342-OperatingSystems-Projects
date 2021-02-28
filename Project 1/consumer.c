#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    char c;
    for (int i = 0; i < atoi(argv[1]); i++) {
        read(STDIN_FILENO, &c, 1);
    }
}
