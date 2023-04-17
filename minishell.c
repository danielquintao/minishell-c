#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>  // TODO remove me

int main() {

    while(1) {
        // read cmd
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

        // parse cmd
        prog = strtok(cmd, delim);
        if(prog) {
            printf("comando: %s\n", prog);
        }
        
        while(tmp = strtok(NULL, " ")) {
            argv = realloc(argv, (argc + 1) * sizeof *argv);
            // argv[argc] = malloc(sizeof tmp);
            argv[argc++] = tmp;
        }

        for(int i = 0; i < argc; i++) printf("arg: %s\n", argv[i]);

        // check if program file exists
        struct stat sb;
        int res = stat(prog, &sb);
        if(res == -1 && errno == 2) {
            printf("No such file\n");
            continue;
        } else if(res == -1) {
            printf("Some error occurred while checking for file %s\n", prog);
            continue;
        } else if((sb.st_mode & S_IFMT) != S_IFREG) {
            printf("%s is not a regular file\n", prog);
            continue;
        }

        // execute program
        pid_t pid = fork();

        if(pid != 0) {
            // we are in parent
            int * wstatus;
            wait(wstatus);
        } else {
            char * const nullargv[] = {NULL};
            char * const nullenvp[] = {NULL};
            execve(prog, nullargv, nullenvp); // TODO use argv
        }

        // free memory
        free(cmd); // free(prog) is unnecessary, since prog points to an adress "mallocated" by cmd
        //            Similarly for the arguments (argv[i], i < argc)
        free(argv);
        free(tmp);
        // // break;
    }


}