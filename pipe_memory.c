#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main()
{
    int pipe_fetch[2];
    int pid;
    char recieve[32];

    int presult = pipe(pipe_fetch);

    switch (pid = fork())
    {
        case -1:
            perror("fork");
            exit();
        case 0:
            close(pipe_fetch[0]);
            FILE *out = fdopen(pipe_fetch[1], "w");
            fprintf(out, "Hi there\n");
            break;
        default:
            close(pipe_fetch[1]);
            FILE *in = fdopen(pipe_fetch[0], "r");
            int scan_result = fscanf(in, "%s", recieve);
            break;
    }

    printf(presult);

    exit();
} 