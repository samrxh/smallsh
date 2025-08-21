#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Constants
#define INPUT_LENGTH 2048
#define MAX_ARGS 512

// Prototypes
struct command_line *parse_input();
void open_file(struct command_line *curr_command);
void check_background_processes();
void free_command(struct command_line *curr_command);
void handle_SIGINT();
void handle_SIGTSTP();

// Global variables
bool foreground_only_mode = false;
int background_processes[10];
int num_background_processes = 0;

// Struct to hold command line arguments borrowed and modified from sample_parser.c given on the assignment page
struct command_line
{
    char *argv[MAX_ARGS + 1];
    int argc;
    char *input_file;
    char *output_file;
    bool is_bg;
};

// Program Name: hoovesam_assignment4.c
// Author: Samuel Hoover
// This program is a small shell that takes in user input and executes commands. It has the built-in commands cd,
// status, and exit. It can run commands in the background and redirect input and output. It also has the ability to
// toggle foreground-only mode by pressing CTRL-Z. In foreground-only mode, the shell will ignore the "&" operator.

int main()
{
    struct command_line *curr_command;
    pid_t pid;
    int status;
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0}, ignore_action = {0};

    // Ignore SIGINT
    ignore_action.sa_handler = SIG_IGN;
    sigaction(SIGINT, &ignore_action, NULL);

    // Handle SIGTSTP
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    while(true)
    {
        // Check for any background processes that have finished
        check_background_processes();

        curr_command = parse_input();

        if (curr_command == NULL || curr_command->argc == 0)
        {
            free_command(curr_command);
            continue;
        }

        // 'exit' command
        if (curr_command->argc > 0 && !strcmp(curr_command->argv[0], "exit"))
        {
            for (int i = 1; i <= num_background_processes; i++)
            {
                kill(background_processes[i], SIGKILL);
            }
            free_command(curr_command);
            break;
        }

        // 'cd' command
        else if (curr_command->argc > 0 && !strcmp(curr_command->argv[0], "cd"))
        {
            if (curr_command->argc > 1)
            {
                chdir(curr_command->argv[1]);
            }
            else
            {
                chdir(getenv("HOME"));
            }
        }

        // 'status' command
        else if (curr_command->argc > 0 && !strcmp(curr_command->argv[0], "status"))
        {
            if (WIFEXITED(status))
            {
                printf("exit value %d\n", WEXITSTATUS(status));
            }
            else if (WIFSIGNALED(status))
            {
                printf("terminated by signal %d\n", WTERMSIG(status));
            }
            fflush(stdout);
        }

        // Other commands can be run in the foreground or background
        else if (curr_command->is_bg)
        {
            pid = fork();
            switch (pid)
            {
                case -1:
                    perror("fork() failed!");

                case 0:
                    // Ignore SIGINT
                    ignore_action.sa_handler = SIG_IGN;
                    sigaction(SIGINT, &ignore_action, NULL);

                    // Ignore SIGTSTP
                    ignore_action.sa_handler = SIG_IGN;
                    sigaction(SIGTSTP, &ignore_action, NULL);

                    // Redirect input and output if necessary
                    open_file(curr_command);

                    // If no input or output file is specified, redirect to /dev/null
                    if (!curr_command->input_file) {
                        int dev_null = open("/dev/null", O_RDONLY);
                        dup2(dev_null, STDIN_FILENO);
                        close(dev_null);
                    }
                    if (!curr_command->output_file) {
                        int dev_null = open("/dev/null", O_WRONLY);
                        dup2(dev_null, STDOUT_FILENO);
                        close(dev_null);
                    }

                    execvp(curr_command->argv[0], curr_command->argv);

                    // This will be executed only if execvp returns an error
                    perror(curr_command->argv[0]);
                    exit(1);

                default:
                    // Don't wait for the child to finish
                    printf("background pid is %d\n", pid);
                    fflush(stdout);

                    // Add the process to the array of background processes
                    num_background_processes++;
                    background_processes[num_background_processes] = pid;

            }
        }

        else
        {
            pid = fork();
            switch (pid)
            {
                case -1:
                    perror("fork() failed!");
                    free_command(curr_command);
                    continue;

                case 0:
                    // Handle SIGINT
                    SIGINT_action.sa_handler = handle_SIGINT;
                    sigfillset(&SIGINT_action.sa_mask);
                    SIGINT_action.sa_flags = 0;
                    sigaction(SIGINT, &SIGINT_action, NULL);

                    // Ignore SIGTSTP
                    ignore_action.sa_handler = SIG_IGN;
                    sigaction(SIGTSTP, &ignore_action, NULL);

                    // Redirect input and output if necessary
                    open_file(curr_command);

                    execvp(curr_command->argv[0], curr_command->argv);

                    // This will be executed only if execvp returns an error
                    perror(curr_command->argv[0]);
                    exit(1);

                default:
                    // Wait for the child to finish
                    waitpid(pid, &status, 0);
                    if (WIFSIGNALED(status))
                    {
                        printf("terminated by signal %d\n", WTERMSIG(status));
                        fflush(stdout);
                    }
            }

        }

        free_command(curr_command);

    }

    return EXIT_SUCCESS;
}

// Command line parser borrowed and modified from sample_parser.c given on the assignment page
struct command_line *parse_input()
{
    char input[INPUT_LENGTH];
    struct command_line *curr_command = (struct command_line *) calloc(1, sizeof(struct command_line));

    // Get input
    printf(": ");
    fflush(stdout);
    if (fgets(input, INPUT_LENGTH, stdin) == NULL)
    {
        free(curr_command);
        return NULL;
    }

    // Tokenize the input
    char *token = strtok(input, " \n");
    while(token)
    {
        if (token[0] == '#' && curr_command->argc == 0)
        {
            free(curr_command);
            return NULL;
        }
        else if(!strcmp(token,"<"))
        {
            token = strtok(NULL," \n");
            if (token == NULL) {
                free(curr_command);
                return NULL;
            }
            curr_command->input_file = strdup(token);
        }
        else if(!strcmp(token,">"))
        {
            token = strtok(NULL," \n");
            if (token == NULL) {
                free(curr_command);
                return NULL;
            }
            curr_command->output_file = strdup(token);
        }
        else
        {
            curr_command->argv[curr_command->argc++] = strdup(token);
        }
        token=strtok(NULL," \n");
    }

    // Check if the last argument is "&" and set is_bg to true if it is and remove it from the args
    if (curr_command->argc > 0 && !strcmp(curr_command->argv[curr_command->argc - 1], "&"))
    {
        if (!foreground_only_mode)
        {
            curr_command->is_bg = true;
        }

        curr_command->argc--;
        curr_command->argv[curr_command->argc] = NULL;
    }

    return curr_command;
}

// Open the input and output files
void open_file(struct command_line *curr_command)
{
    if (curr_command->input_file)
    {
        int input_fd = open(curr_command->input_file, O_RDONLY);
        if (input_fd != -1)
        {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        else
        {
            perror(curr_command->input_file);
            exit(1);
        }
    }

    if (curr_command->output_file)
    {
        int output_fd = open(curr_command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_fd != -1)
        {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }
        else
        {
            perror(curr_command->output_file);
            exit(1);
        }
    }
}

// Check for any background processes that have finished
void check_background_processes()
{
    pid_t pid;
    int status;

    for (int i = 1; i <= num_background_processes; i++)
    {
        if (waitpid(background_processes[i], &status, WNOHANG) > 0)
        {
            if (WIFEXITED(status))
            {
                printf("background pid %d is done: exit value %d\n", background_processes[i], WEXITSTATUS(status));
            }
            else if (WIFSIGNALED(status))
            {
                printf("background pid %d is done: terminated by signal %d\n", background_processes[i], WTERMSIG(status));
            }
            fflush(stdout);

            // Remove the process from the array of background processes
            for (int j = i; j < num_background_processes; j++)
            {
                background_processes[j] = background_processes[j + 1];
            }
            num_background_processes--;
        }
    }
}

// Free the memory allocated for the command line
void free_command(struct command_line *curr_command)
{
    if (curr_command == NULL)
    {
        return;
    }
    if (curr_command->input_file)
    {
        free(curr_command->input_file);
    }
    if (curr_command->output_file)
    {
        free(curr_command->output_file);
    }
    for(int i = 0; i < curr_command->argc; i++){
        free(curr_command->argv[i]);
    }
    free(curr_command);
}

// Handle SIGINT
void handle_SIGINT()
{
    char *message = "terminated by signal 2\n";
    write(STDOUT_FILENO, message, 24);
    kill(getpid(), SIGINT);
}

// Handle SIGTSTP
void handle_SIGTSTP()
{
    if (foreground_only_mode)
    {
        char *message = "\nExiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 30);
        foreground_only_mode = false;
    }
    else
    {
        char *message = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 50);
        foreground_only_mode = true;
    }
}
