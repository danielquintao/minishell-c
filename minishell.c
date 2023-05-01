/* Daniel Quintao de Moraes */
/* CSC-33 course */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

int main() {

    // welcome message
    printf(" ____  _____    ___  ____  ___ ___           _____ ___ ___ ______  ___    ___\n");
    printf("| |\\ \\ | |\\ \\   | | | |\\ \\ | | | |  _____   /  __/ | |_| | |  ___  | |    | |\n");
    printf("| | \\ \\| | \\ \\  | | | | \\ \\| | | | |_____| _\\  \\   |  _  | |  ___  | |__  | |__\n");
    printf("|_|  \\___|  \\_\\ |_| |_|  \\___| |_|        /____/   |_| |_| |_____  |____| |____|\n");

    while(1) {
        // read cmd
        char *cmd;
        cmd = malloc(256);
        char *prog, **argv, *tmp, *in_fd, *out_fd;
        int in_fd_flag = 0; // whether we have input redirection (may change to 1 during execution)
        int out_fd_flag = 0; // whether we have output redirection (may change to 1 during execution)
        argv = malloc(sizeof *argv);
        const char *delim = " ";
        short argc = 0;

        ssize_t trash = write(1, "$> ", 3);
        ssize_t nchars_cmd = read(0, cmd, 255);
        cmd[nchars_cmd - 1] = '\0';

        // special case: exit
        if(strcmp(cmd, "exit") == 0) {
            free(cmd);
            free(argv);
            break;
        }

        // parse cmd
        prog = strtok(cmd, delim);
        
        argv[argc++] = prog;  // first argv is always the own command
        while(tmp = strtok(NULL, " ")) {
            // check for I/O redirection requests
            if(strcmp(tmp, "<") == 0) {
                tmp = strtok(NULL, " ");
                if(tmp == NULL) {
                    printf("No string after '<'... Ignoring it.\n");
                    break;
                }
                in_fd = tmp;
                in_fd_flag = 1;
                continue;
            }
            if(strcmp(tmp, ">") == 0) {
                tmp = strtok(NULL, " ");
                if(tmp == NULL) {
                    printf("No string after '>'... Ignoring it.\n");
                    break;
                }
                out_fd = tmp;
                out_fd_flag = 1;
                continue;
            }
            // ----------------------
            // keep processing arguments
            argv = realloc(argv, (argc + 1) * sizeof *argv);
            // argv[argc] = malloc(sizeof tmp);
            argv[argc++] = tmp;
        }
        argv = realloc(argv, (argc + 1) * sizeof *argv);
        argv[argc] = NULL;

        // deal corner case of I/O redirection symbol (>, <) in an argument (i.e. no surrounding spaces)
        int restart = 0;
        for(int i = 0; i < argc; i++) {
            char * str = argv[i];
            for(int j = 0; j < strlen(str); j++) {
                if(str[j] == '<' || str[j] == '>') {
                    printf("MINISHELL ERROR: Found a < or a > in argument %s.\n", str);
                    printf("Please add blank spaces if your intention is to perform I/O redirection.\n");
                    restart = 1;
                    break;
                }
            }
        }
        if(restart) {
            free(cmd);
            free(argv);
            continue;
        }

        // check if program file exists
        struct stat sb;
        int res = stat(prog, &sb);
        if(res == -1 && errno == 2) {
            printf("No such file\n");
            free(cmd);
            free(argv);
            continue;
        } else if(res == -1) {
            printf("Some error occurred while checking for file %s\n", prog);
            free(cmd);
            free(argv);
            continue;
        } else if((sb.st_mode & S_IFMT) != S_IFREG) {
            printf("%s is not a regular file\n", prog);
            free(cmd);
            free(argv);
            continue;
        }

        // execute program
        pid_t pid = fork();

        if(pid != 0) {
            // we are in parent
            int * wstatus;
            wait(wstatus);
        } else {
            // deal with I/O redirection
            if(out_fd_flag) {
                int newfd = open(out_fd, O_CREAT|O_WRONLY|O_TRUNC, 00664);
                int res = dup2(newfd, 1); // 1 = stdout
                if(res == -1) {
                    printf("ERROR on dup2:\n%s\n", strerror(errno));
                    return 1;
                }
            }
            if(in_fd_flag) {
                int newfd = open(in_fd, O_RDONLY);
                int res = dup2(newfd, 0); // 0 = stdin
                if(res == -1) {
                    printf("ERROR on dup2:\n%s\n", strerror(errno));
                    return 1;
                }
            }
            // call execve
            char * const nullenvp[] = {NULL};
            if(execve(prog, argv, nullenvp) == -1) {
                printf("Error on execve: %d\n", errno);
            }
        }

        // free memory
        free(cmd); // free(prog) and free(tmp) are unnecessary, since point to adresses "mallocated" by cmd
        //            Similarly for the arguments argv[i] (i < argc)
        free(argv);
    }


}