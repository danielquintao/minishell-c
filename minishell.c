#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>  // TODO remove me

int main() {

    while(1) {
        // 1. read and parse cmd
        char *cmd;
        cmd = malloc(256);
        char *prog, **argv, *tmp;
        argv = malloc(sizeof *argv);
        const char *delim = " ";
        short argc = 0;

        ssize_t trash = write(1, "$> ", 3);
        ssize_t nchars_cmd = read(0, cmd, 255);
        cmd[nchars_cmd - 1] = '\0';
        printf("echo %s\n", cmd);
        printf("%ld caracteres lidos\n", nchars_cmd);

        // special case: exit
        if(strcmp(cmd, "exit") == 0) {
            free(cmd);
            free(argv);
            break;
        }

        prog = strtok(cmd, delim);
        if(prog) {
            printf("comando: %s\n", prog);
        }
        
        while(tmp = strtok(NULL, " ")) {
            argv = realloc(argv, (argc + 1) * sizeof *argv);
            argv[argc] = malloc(sizeof tmp);
            argv[argc++] = tmp;
        }

        for(int i = 0; i < argc; i++) printf("arg: %s\n", argv[i]);

        // free memory
        free(cmd); // free(prog) is unnecessary, since prog points to an adress "mallocated" by cmd
        //            Similarly for the arguments (argv[i], i < argc)
        free(argv);
        free(tmp);
        // // break;
    }


}