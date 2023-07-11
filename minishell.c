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

typedef struct process {
        char *prog;
        char **argv;
        int argc;
        int fd_in;
        int fd_out;
        struct process *NEXT;
    } process;

void free_pipeline(process *proc_begin) {
    process *prev, *curr;
    prev = proc_begin;
    curr = proc_begin;
    while(curr != NULL) {
        printf("Deleting process %p\n", curr); //! DEBUG
        free(curr->argv);
        printf("-----> liberou memoria de algum argv\n");  //! DEBUG
        curr = curr->NEXT;
        free(prev);
        printf("-----> liberou memoria de um ponteiro processo\n");  //! DEBUG
        prev = curr;
    }
}

// FOR DEBUGGING:
void print_process(process *proc) {
    printf("Process addr %p:\n", proc);
    printf("\tprog: %s\n", proc->prog);
    printf("\targc: %d\n", proc->argc);
    printf("\targs:\n");
    for(int i = 0; i < proc->argc; i++) 
        printf("\t\t%s\n", proc->argv[i]);
    printf("\tfd_in: %d\n", proc->fd_in);
    printf("\tfd_out: %d\n", proc->fd_out);
}

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
        // linked list of processes:
        process *proc_begin;
        process *proc_end;
        const char *delim = " ";
        char *tmp;

        ssize_t trash = write(1, "$> ", 3);
        ssize_t nchars_cmd = read(0, cmd, 255);
        cmd[nchars_cmd - 1] = '\0';

        // special case: exit
        if(strcmp(cmd, "exit") == 0) {
            free(cmd);
            break;
        }

        // TODO solve case where no command (just >=0 blank spaces and ENTER)

        // populate processes list:
        int pipefd[2] = {-1, -1};
        int initial_fdin = 0;
        int last_fdout = 1;
        char has_next = 1; // whether there is a next process in the pipeline
        proc_begin = NULL;
        tmp = strtok(cmd, delim);
        proc_end = (process *) malloc(sizeof (*proc_end));
        while(has_next) {
            if(proc_begin == NULL) { // first loop cycle
                proc_begin = proc_end;
            }
            proc_end->NEXT = NULL;
            proc_end->argv = malloc(sizeof *(proc_end->argv));
            proc_end->argc = 0;
            has_next = 0;
            proc_end->prog = tmp;
            proc_end->argv[proc_end->argc++] = proc_end->prog;  // first argv is always the own command
            if(pipefd[0] != -1) { // if there is a previous process in pipeline
                proc_end->fd_in = pipefd[0]; // set pipe read-end file descriptor
            }
            proc_end->fd_out = last_fdout; // defult value that may be overwritten
            // keep reading arguments (or I/O redirectionin symbols) until pipe or end
            while(tmp = strtok(NULL, " ")) {
                // check for I/O redirection requests
                if(strcmp(tmp, "<") == 0) {
                    tmp = strtok(NULL, " ");
                    if(tmp == NULL) {
                        printf("No string after '<'... Ignoring it.\n");
                        break;
                    }
                    initial_fdin = open(tmp, O_RDONLY);
                    continue;
                }
                if(strcmp(tmp, ">") == 0) {
                    tmp = strtok(NULL, " ");
                    if(tmp == NULL) {
                        printf("No string after '>'... Ignoring it.\n");
                        break;
                    }
                    last_fdout = open(tmp, O_CREAT|O_WRONLY|O_TRUNC, 00664);
                    // update fd_out of last process (this assignment will have no effect if a pipe appears later):
                    proc_end->fd_out = last_fdout;
                    continue;
                }
                // ----------------------
                // check for pipe
                if(strcmp(tmp, "|") == 0) {
                    has_next = 1;  // there is another command after pipe
                    if(pipe(pipefd) == -1) {
                        printf("ERROR on pipe:\n%s\n", strerror(errno));
                        return 1;
                    }
                    printf("Created pipe {%d, %d}\n", pipefd[0], pipefd[1]); //! DEBUG
                    proc_end->fd_out = pipefd[1]; // set pipe write-end file descriptor
                    proc_end->NEXT = (process *) malloc(sizeof (*proc_end)); // allocate mem for next process struct
                    break; // no more args for this process
                }
                // ----------------------
                // keep processing arguments
                proc_end->argv = realloc(proc_end->argv, (proc_end->argc + 1) * sizeof *(proc_end->argv));
                // argv[argc] = malloc(sizeof tmp);
                proc_end->argv[proc_end->argc++] = tmp;
            }
            proc_end->argv = realloc(proc_end->argv, (proc_end->argc + 1) * sizeof *(proc_end->argv));
            proc_end->argv[proc_end->argc] = NULL;
            // walk in the linked list
            proc_end = proc_end->NEXT;
            // read next prog
            tmp = strtok(NULL, " ");
        }

        // Set extremity read file descriptors of pipeline (which may have been changed)
        proc_begin->fd_in = initial_fdin; // note tat proc_end->fd_out has already been set


        //* /////////////////////////////////////////////////////////////////////////////////////////////////////
        // TODO BRING IT BACK
        // // deal corner case of I/O redirection symbol (>, <) in an argument (i.e. no surrounding spaces)
        // int restart = 0;
        // for(int i = 0; i < argc; i++) {
        //     char * str = argv[i];
        //     for(int j = 0; j < strlen(str); j++) {
        //         if(str[j] == '<' || str[j] == '>') {
        //             printf("MINISHELL ERROR: Found a < or a > in argument %s.\n", str);
        //             printf("Please add blank spaces if your intention is to perform I/O redirection.\n");
        //             restart = 1;
        //             break;
        //         }
        //     }
        // }
        // if(restart) {
        //     free(cmd);
        //     free(argv);
        //     continue;
        // }
        //* /////////////////////////////////////////////////////////////////////////////////////////////////////

        // execute processes
        int n_proc = 0;
        pid_t pid;
        process *proc;
        printf("TERMINAL GRPID:  %d\n", tcgetpgrp(0)); //! DEBUG
        printf("PARENT PID: %d, GRPID: %d\n", getpid(), getpgrp()); //! DEBUG
        for(proc = proc_begin; proc != proc_end; proc = proc->NEXT) {
            //! DEBUG
            print_process(proc);

            n_proc++;
            // check if program file exists
            struct stat sb;
            int res = stat(proc->prog, &sb);
            if(res == -1 && errno == 2) {
                printf("No such file\n");
                break;
            } else if(res == -1) {
                printf("Some error occurred while checking for file %s\n", proc->prog);
                break;
            } else if((sb.st_mode & S_IFMT) != S_IFREG) {
                printf("%s is not a regular file\n", proc->prog);
                break;
            }
            // execute program
            printf("Entering child process nb %d...\n", n_proc); //! DEBUG
            pid = fork();
            if(pid == 0) {
                printf("CHILD PID: %d, GRPID: %d\n", getpid(), getpgrp()); //! DEBUG
                // set file descriptors (previously defined according to the pipes and I/O redirection)
                int res = dup2(proc->fd_out, 1);
                if(res == -1) {
                    printf("ERROR on dup2:\n%s\n", strerror(errno));
                    return 1;
                }
                res = dup2(proc->fd_in, 0);
                if(res == -1) {
                    printf("ERROR on dup2:\n%s\n", strerror(errno));
                    return 1;
                }
                // close files BEFORE execve (see https://www.gnu.org/software/libc/manual/html_node/Launching-Jobs.html)
                if(proc->fd_out != 1) {
                    res = close(proc->fd_out);
                    if(res == -1) {
                        printf("ERROR closing write file descriptor of %d:\n%s\n", getpid(), strerror(errno));
                        return 1;
                    }
                }
                if(proc->fd_in != 0) {
                    res = close(proc->fd_in);
                    if(res == -1) {
                        printf("ERROR closing read file descriptor of %d:\n%s\n", getpid(), strerror(errno));
                        return 1;
                    }
                }
                // execve
                char * const nullenvp[] = {NULL};
                if(execve(proc->prog, proc->argv, nullenvp) == -1) {
                    printf("Error on execve: %d\n", errno);
                }
            } else if(pid < 0) {
                printf("Error on fork: %d\n", errno);
            }
            else {
                printf("(In parent) Just launched child process %d. Wait for it.\n", n_proc);
                // close file descriptors in parent as well
                if(proc->fd_out != 1) {
                    res = close(proc->fd_out);
                    if(res == -1) {
                        printf("ERROR closing write file descriptor of child nb %d (1-indexed):\n%s\n", n_proc, strerror(errno));
                        return 1;
                    }
                }
                if(proc->fd_in != 0) {
                    res = close(proc->fd_in);
                    if(res == -1) {
                        printf("ERROR closing read file descriptor of child nb %d (1-indexed):\n%s\n", n_proc, strerror(errno));
                        return 1;
                    }
                }
            }
        }

        // parent waits for children
        if(pid != 0) {
            int * wstatus;
            for(int i = 0; i < n_proc; i++) {
                wait(wstatus);
                printf("One more child returned! Remaining: %d\n", n_proc - i - 1); //! DEBUG
            }
        }

        // free memory
        free(cmd);
        printf("-----> liberou memoria de cmd\n"); //! DEBUG
        free_pipeline(proc_begin);
    }


}