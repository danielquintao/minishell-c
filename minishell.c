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
#include <termios.h>

// -----------------------------------------------------------------------------
// STRUCTS
// -----------------------------------------------------------------------------
// c.f. https://www.gnu.org/software/libc/manual/html_node/Data-Structures.html

/* A process is a single process.  */
typedef struct process
{
  struct process *next;       /* next process in pipeline */
  char **argv;                /* for exec */
  int argc;                   // not in GNU manual but useful
  pid_t pid;                  /* process ID */
  char completed;             /* true if process has completed */
  char stopped;               /* true if process has stopped */
  int status;                 /* reported status value */
} process;

/* A job is a pipeline of processes.  */
typedef struct job
{
  struct job *next;           /* next active job */
  char *command;              /* command line, used for messages */
  process *first_process;     /* list of processes in this job */
  pid_t pgid;                 /* process group ID */
  char notified;              /* true if user told about stopped job */
  struct termios tmodes;      /* saved terminal modes */
  int stdin, stdout, stderr;  /* standard i/o channels */
} job;

// -----------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------
// c.f. https://www.gnu.org/software/libc/manual/html_node/Implementing-a-Shell.html

job *first_job = NULL;
pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;

// -----------------------------------------------------------------------------
// UTILITY FUNCTIONS
// -----------------------------------------------------------------------------
// c.f. https://www.gnu.org/software/libc/manual/html_node/Implementing-a-Shell.html

// free memory associated to pipeline of processes
void free_pipeline(process *proc_begin) {
    process *prev, *curr;
    prev = proc_begin;
    curr = proc_begin;
    while(curr != NULL) {
        printf("Deleting process %p\n", curr); //! DEBUG
        free(curr->argv);
        printf("-----> liberou memoria de algum argv\n");  //! DEBUG
        curr = curr->next;
        free(prev);
        printf("-----> liberou memoria de um ponteiro processo\n");  //! DEBUG
        prev = curr;
    }
}

// free memory associated to a specific job
void free_job(job* j) {
    // find job just before it to avoid breaking job linked list
    job * just_before;
    for(just_before = first_job; just_before != NULL; just_before = just_before->next) {
        if(just_before != NULL && just_before->next == j) {
            // fix jobs list
            just_before->next = j->next;
        }
    }
    // finally free job
    free_pipeline(j->first_process);
    j->next = NULL;
    free(j->command);
    free(j);
}

/* Find the active job with the indicated pgid.  */
job * find_job (pid_t pgid)
{
  job *j;

  for (j = first_job; j; j = j->next)
    if (j->pgid == pgid)
      return j;
  return NULL;
}

/* Return true if all processes in the job have stopped or completed.  */
int job_is_stopped (job *j)
{
  process *p;

  for (p = j->first_process; p; p = p->next)
    if (!p->completed && !p->stopped)
      return 0;
  return 1;
}

/* Return true if all processes in the job have completed.  */
int job_is_completed (job *j)
{
  process *p;

  for (p = j->first_process; p; p = p->next)
    if (!p->completed)
      return 0;
  return 1;
}

/* Make sure the shell is running interactively as the foreground job before proceeding. */
void init_shell ()
{

  /* See if we are running interactively.  */
  shell_terminal = STDIN_FILENO;
  shell_is_interactive = isatty (shell_terminal);

  if (shell_is_interactive)
    {
      /* Loop until we are in the foreground.  */
      while (tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp ()))
        kill (- shell_pgid, SIGTTIN);

      /* Ignore interactive and job-control signals.  */
      signal (SIGINT, SIG_IGN);
      signal (SIGQUIT, SIG_IGN);
      signal (SIGTSTP, SIG_IGN);
      signal (SIGTTIN, SIG_IGN);
      signal (SIGTTOU, SIG_IGN);
      signal (SIGCHLD, SIG_IGN);

      /* Put ourselves in our own process group.  */
      shell_pgid = getpid ();
      if (setpgid (shell_pgid, shell_pgid) < 0)
        {
          perror ("Couldn't put the shell in its own process group");
          exit (1);
        }

      /* Grab control of the terminal.  */
      tcsetpgrp (shell_terminal, shell_pgid);

      /* Save default terminal attributes for shell.  */
      tcgetattr (shell_terminal, &shell_tmodes);
    }
}

/* Format information about job status for the user to look at.  */
void format_job_info (job *j, const char *status)
{
  fprintf (stderr, "%ld (%s): %s\n", (long)j->pgid, status, j->command);
}

/* Store the status of the process pid that was returned by waitpid.
   Return 0 if all went well, nonzero otherwise.  */
int mark_process_status (pid_t pid, int status)
{
  job *j;
  process *p;

  if (pid > 0)
    {
      /* Update the record for the process.  */
      for (j = first_job; j; j = j->next)
        for (p = j->first_process; p; p = p->next)
          if (p->pid == pid)
            {
              p->status = status;
              if (WIFSTOPPED (status))
                p->stopped = 1;
              else
                {
                  p->completed = 1;
                  if (WIFSIGNALED (status))
                    fprintf (stderr, "%d: Terminated by signal %d.\n",
                             (int) pid, WTERMSIG (p->status));
                }
              return 0;
             }
      fprintf (stderr, "No child process %d.\n", pid);
      return -1;
    }
  else if (pid == 0 || errno == ECHILD)
    /* No processes ready to report.  */
    return -1;
  else {
    /* Other weird errors.  */
    perror ("waitpid");
    return -1;
  }
}

/* Check for processes that have status information available,
   blocking until all processes in the given job have reported.  */
void wait_for_job (job *j)
{
  int status;
  pid_t pid;

  do
    pid = waitpid (WAIT_ANY, &status, WUNTRACED);
  while (!mark_process_status (pid, status)
         && !job_is_stopped (j)
         && !job_is_completed (j));
}

/* Put job j in the foreground.  If cont is nonzero,
   restore the saved terminal modes and send the process group a
   SIGCONT signal to wake it up before we block.  */
void put_job_in_foreground (job *j, int cont)
{
  /* Put the job into the foreground.  */
  tcsetpgrp (shell_terminal, j->pgid);

  /* Send the job a continue signal, if necessary.  */
  if (cont)
    {
      tcsetattr (shell_terminal, TCSADRAIN, &j->tmodes);
      if (kill (- j->pgid, SIGCONT) < 0)
        perror ("kill (SIGCONT)");
    }

  /* Wait for it to report.  */
  wait_for_job (j);

  /* Put the shell back in the foreground.  */
  tcsetpgrp (shell_terminal, shell_pgid);

  /* Restore the shell's terminal modes.  */
  tcgetattr (shell_terminal, &j->tmodes);
  tcsetattr (shell_terminal, TCSADRAIN, &shell_tmodes);
}

/* Put a job in the background.  If the cont argument is true, send
   the process group a SIGCONT signal to wake it up.  */
void put_job_in_background (job *j, int cont)
{
  /* Send the job a continue signal, if necessary.  */
  if (cont)
    if (kill (-j->pgid, SIGCONT) < 0)
      perror ("kill (SIGCONT)");
}

/* Check for processes that have status information available,
   without blocking.  */
void update_status (void)
{
  int status;
  pid_t pid;

  do
    pid = waitpid (WAIT_ANY, &status, WUNTRACED|WNOHANG);
  while (!mark_process_status (pid, status));
}

/* Notify the user about stopped or terminated jobs.
   Delete terminated jobs from the active job list.  */
void do_job_notification (void)
{
  job *j, *jlast, *jnext;

  /* Update status information for child processes.  */
  update_status ();

  jlast = NULL;
  for (j = first_job; j; j = jnext)
    {
      jnext = j->next;

      /* If all processes have completed, tell the user the job has
         completed and delete it from the list of active jobs.  */
      if (job_is_completed (j)) {
        format_job_info (j, "completed");
        if (jlast)
          jlast->next = jnext;
        else
          first_job = jnext;
        free_job (j);
      }

      /* Notify the user about stopped jobs,
         marking them so that we won't do this more than once.  */
      else if (job_is_stopped (j) && !j->notified) {
        format_job_info (j, "stopped");
        j->notified = 1;
        jlast = j;
      }

      /* Don't say anything about jobs that are still running.  */
      else
        jlast = j;
    }
}

/* Mark a stopped job J as being running again.  */
void mark_job_as_running (job *j)
{
  process *p;

  for (p = j->first_process; p; p = p->next)
    p->stopped = 0;
  j->notified = 0;
}

void launch_process (process *p, pid_t pgid,
                int infile, int outfile, int errfile,
                int foreground)
{
  pid_t pid;

  if (shell_is_interactive)
    {
      /* Put the process into the process group and give the process group
         the terminal, if appropriate.
         This has to be done both by the shell and in the individual
         child processes because of potential race conditions.  */
      pid = getpid ();
      if (pgid == 0) pgid = pid;
      setpgid (pid, pgid);
      if (foreground)
        tcsetpgrp (shell_terminal, pgid);

      /* Set the handling for job control signals back to the default.  */
      signal (SIGINT, SIG_DFL);
      signal (SIGQUIT, SIG_DFL);
      signal (SIGTSTP, SIG_DFL);
      signal (SIGTTIN, SIG_DFL);
      signal (SIGTTOU, SIG_DFL);
      signal (SIGCHLD, SIG_DFL);
    }

  /* Set the standard input/output channels of the new process.  */
  if (infile != STDIN_FILENO)
    {
      dup2 (infile, STDIN_FILENO);
      close (infile);
    }
  if (outfile != STDOUT_FILENO)
    {
      dup2 (outfile, STDOUT_FILENO);
      close (outfile);
    }
  if (errfile != STDERR_FILENO)
    {
      dup2 (errfile, STDERR_FILENO);
      close (errfile);
    }

  /* Exec the new process.  Make sure we exit.  */
  execvp (p->argv[0], p->argv);
  perror ("execvp");
  exit (1);
}

void launch_job (job *j, int foreground)
{
  process *p;
  pid_t pid;
  int mypipe[2], infile, outfile;

  infile = j->stdin;
  for (p = j->first_process; p; p = p->next)
    {
      /* Set up pipes, if necessary.  */
      if (p->next)
        {
          if (pipe (mypipe) < 0)
            {
              perror ("pipe");
              exit (1);
            }
          outfile = mypipe[1];
        }
      else
        outfile = j->stdout;

      /* Fork the child processes.  */
      pid = fork ();
      if (pid == 0)
        /* This is the child process.  */
        launch_process (p, j->pgid, infile,
                        outfile, j->stderr, foreground);
      else if (pid < 0)
        {
          /* The fork failed.  */
          perror ("fork");
          exit (1);
        }
      else
        {
          /* This is the parent process.  */
          p->pid = pid;
          if (shell_is_interactive)
            {
              if (!j->pgid)
                j->pgid = pid;
              setpgid (pid, j->pgid);
            }
        }

      /* Clean up after pipes.  */
      if (infile != j->stdin)
        close (infile);
      if (outfile != j->stdout)
        close (outfile);
      infile = mypipe[0];
    }

  if (!shell_is_interactive)
    wait_for_job (j);
  else if (foreground)
    put_job_in_foreground (j, 0);
  else
    put_job_in_background (j, 0);
}

/* Continue the job J.  */
void continue_job (job *j, int foreground)
{
  mark_job_as_running (j);
  if (foreground)
    put_job_in_foreground (j, 1);
  else
    put_job_in_background (j, 1);
}

// Custom version of GNU man's do_job_notification function (see above) that is more suitable for
// answering user's command "jobs".
// Motivations:
//  1. Original do_job_notification mark stopped jobs to notify the user only once.
//  2. We want to print the job position in the jobs linked list instead of its pgid as does do_job_notification
// Indeed, do_job_notification is intended to be called without user's explicit request
void do_custom_job_notification (void)
{
  job *j, *jlast, *jnext;

  /* Update status information for child processes.  */
  update_status ();

  jlast = NULL;
  int i = 0;
  for (j = first_job; j; j = jnext)
    {
      jnext = j->next;

      /* Notify the user about stopped jobs */
      if (job_is_stopped (j)) {
        printf("[%d] (%s): %s\n", i, "stopped", j->command);
        jlast = j;
      }

      /* Don't say anything about jobs that are still running.  */
      else
        jlast = j;
    
      i++;
    }
}

//! FOR DEBUGGING:
void print_process(process *proc) {
    printf("Process addr %p:\n", proc);
    printf("\tprog: %s\n", proc->argv[0]);
    printf("\targc: %d\n", proc->argc);
    printf("\targs:\n");
    for(int i = 0; i < proc->argc; i++) 
        printf("\t\t%s\n", proc->argv[i]);
}

int main() {

    // init shell
    init_shell();

    // welcome message
    printf(" ____  _____    ___  ____  ___ ___           _____ ___ ___ ______  ___    ___\n");
    printf("| |\\ \\ | |\\ \\   | | | |\\ \\ | | | |  _____   /  __/ | |_| | |  ___  | |    | |\n");
    printf("| | \\ \\| | \\ \\  | | | | \\ \\| | | | |_____| _\\  \\   |  _  | |  ___  | |__  | |__\n");
    printf("|_|  \\___|  \\_\\ |_| |_|  \\___| |_|        /____/   |_| |_| |_____  |____| |____|\n");

    while(1) {
        // According to GNU man, "A good place to put a such a check for terminated and stopped jobs is
        // just before prompting for a new command". So we'll do it here
        do_job_notification();

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

        // special case 1: exit
        if(strcmp(cmd, "exit") == 0) {
            free(cmd);
            break;
        }

        // special case 2: list jobs
        if(strcmp(cmd, "jobs") == 0) {
            free(cmd);
            do_custom_job_notification();
            continue;
        }

        // special case 3: no command
        if(strcmp(cmd, "") == 0) {
            free(cmd);
            continue;
        }

        // First parse of cmd
        tmp = strtok(cmd, delim);

        // special case 4: fg %N (bring process to foreground)
        if(strcmp(tmp, "fg") == 0) {
            // read job id
            tmp = strtok(NULL, " ");
            if(tmp == NULL) {
                printf("Usage: fg N \n\twhere N is the (0-indexed) job position in the list of jobs.\n");
                free(cmd);
                continue;
            }
            int N = atoi(tmp);
            free(cmd);
            job* j = first_job;
            if(j == NULL) {
                printf("Could not find job %d\n", N);
                continue;
            }
            for(int i = 0; i < N; i++) {
                j = j->next;
                if(j == NULL) {
                    printf("Could not find job %d\n", N);
                    break;
                }
            }
            if(j != NULL)
                put_job_in_foreground (j, 1);
            // go to next loop
            continue;
        }

        // create job
        char has_next = 1; // whether there is a next process in the pipeline
        proc_begin = NULL;
        proc_end = (process *) malloc(sizeof (*proc_end));
        proc_end->completed = 0;
        proc_end->stopped = 0;
        job* new_job = (job *) malloc(sizeof (*new_job));
        new_job->stdin = 0;
        new_job->stdout = 1;
        new_job->stderr = 2;
        new_job->pgid = 0; // by examining function launch_job, we understand the default val of pgid is expected to be 0
        new_job->notified = 0;
        new_job->command = malloc(sizeof cmd);
        strcpy(new_job->command, cmd);
        while(has_next) {
            if(proc_begin == NULL) { // first loop cycle
                proc_begin = proc_end;
            }
            proc_end->next = NULL;
            proc_end->argv = malloc(sizeof *(proc_end->argv));
            proc_end->argc = 0;
            has_next = 0;
            proc_end->argv[proc_end->argc++] = tmp;  // first argv is always the own command
            // keep reading arguments until pipe or end
            while(tmp = strtok(NULL, " ")) {
                // check for I/O redirection requests
                if(strcmp(tmp, "<") == 0) {
                    tmp = strtok(NULL, " ");
                    if(tmp == NULL) {
                        printf("No string after '<'... Ignoring it.\n");
                        break;
                    }
                    new_job->stdin = open(tmp, O_RDONLY);
                    continue;
                }
                if(strcmp(tmp, ">") == 0) {
                    tmp = strtok(NULL, " ");
                    if(tmp == NULL) {
                        printf("No string after '>'... Ignoring it.\n");
                        break;
                    }
                    new_job->stdout = open(tmp, O_CREAT|O_WRONLY|O_TRUNC, 00664);
                    continue;
                }
                // ----------------------
                // check for pipe
                if(strcmp(tmp, "|") == 0) {
                    has_next = 1;  // there is another command after pipe
                    proc_end->next = (process *) malloc(sizeof (*proc_end)); // allocate mem for next process struct
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
            proc_end = proc_end->next;
            // read next prog
            tmp = strtok(NULL, " ");
        }
        new_job->first_process = proc_begin;
        // add job to job queue:
        if(first_job == NULL)
            first_job = new_job;
        else {
            job *j, *jlast;
            jlast = NULL;
            for (j = first_job; j != NULL; j = j->next) jlast = j;
            jlast->next = new_job;
        }
        // launch job
        launch_job(new_job, 1);

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

        // free memory
        free(cmd);
        printf("-----> liberou memoria de cmd\n"); //! DEBUG
        // free_pipeline(proc_begin);
    }


}