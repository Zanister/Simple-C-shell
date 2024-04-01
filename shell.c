#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

extern char **get_line();

void execute_command(char **args, int background) {
    if (strcmp(args[0], "cd") == 0) {
        // Handle cd command in the parent process
        if (args[1] == NULL) {
            // No directory provided, go to the home directory
            if (chdir(getenv("HOME")) != 0) {
                perror("chdir");
            }
        } else {
            // Change directory to the specified path
            if (chdir(args[1]) != 0) {
                perror("chdir");
            }
        }
        return;  // Return without forking or executing other commands
    }

    int input_redirection = 0; // Flag to indicate if input redirection is requested
    int output_redirection = 0; // Flag to indicate if output redirection is requested
    int input_fd; // File descriptor for input redirection
    int output_fd; // File descriptor for output redirection

    // Check for input and output redirection
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            input_redirection = 1;
            args[i] = NULL; // Remove '<' from command arguments
            if (args[i + 1] != NULL) {
                // Open the file for reading
                input_fd = open(args[i + 1], O_RDONLY);
                if (input_fd == -1) {
                    perror("open");
                    return;
                }
                args[i + 1] = NULL; // Remove the file name from command arguments
            } else {
                // Missing filename after '<'
                fprintf(stderr, "Syntax error: Missing filename after '<'\n");
                return;
            }
        } else if (strcmp(args[i], ">") == 0) {
            output_redirection = 1;
            args[i] = NULL; // Remove '>' from command arguments
            if (args[i + 1] != NULL) {
                // Open the file for writing, create if it doesn't exist, truncate if it does
                output_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_fd == -1) {
                    perror("open");
                    return;
                }
                args[i + 1] = NULL; // Remove the file name from command arguments
            } else {
                // Missing filename after '>'
                fprintf(stderr, "Syntax error: Missing filename after '>'\n");
                return;
            }
        }
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        // Set up input redirection if requested
        if (input_redirection) {
            // Duplicate the file descriptor for stdin to the input file descriptor
            if (dup2(input_fd, STDIN_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(input_fd); // Close the original file descriptor for the input file
        }

        // Set up output redirection if requested
        if (output_redirection) {
            // Duplicate the file descriptor for stdout to the output file descriptor
            if (dup2(output_fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(output_fd); // Close the original file descriptor for the output file
        }

        // Execute the command using execvp
        if (execvp(args[0], args) < 0) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else {
        // Parent process
        if (!background) {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) {
                int exit_status = WEXITSTATUS(status);
                if (exit_status != 0) {
                    printf("Command exited with abnormal status: %d\n", exit_status);
                }
            }
        }
    }
}


void free_args(char **args) {
    int i = 0;
    while (args[i] != NULL) {
        free(args[i]);
        i++;
    }
    // free(args);
}

int main() {
    char **args;

    while (1) {
        printf("$ "); // Print the dollar sign as a prompt
        fflush(stdout); // Flush the output buffer to ensure prompt is displayed

        args = get_line();
        
        if (args == NULL) {
            fprintf(stderr, "Error: Failed to get user input\n");
            continue;
        }

        if (args[0] != NULL && strcmp(args[0], "exit") == 0) {
            // Free memory and exit if the user enters "exit"
            free_args(args);
            exit(EXIT_SUCCESS);
        }

        int background = 0;
        int i = 0;
        while (args[i] != NULL) {
            if (strcmp(args[i], "&") == 0) {
                background = 1;
                args[i] = NULL;
                break;
            }
            i++;
        }

        // Check if the command contains a pipe
        int pipe_pos = -1;
        i = 0;
        while (args[i] != NULL) {
            if (strcmp(args[i], "|") == 0) {
                pipe_pos = i;
                break;
            }
            i++;
        }

        if (pipe_pos != -1) {
            // Create pipe
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }

            // Split the command into two parts
            args[pipe_pos] = NULL;
            char **first_command = args;
            char **second_command = &args[pipe_pos + 1];

            // Fork to execute the first command
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            } else if (pid == 0) {
                // Child process
                // Close the read end of the pipe
                close(pipefd[0]);

                // Redirect stdout to the write end of the pipe
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);

                // Execute the first command
                execute_command(first_command, background);
                free_args(first_command); // Free memory for the first command
                // free_args(args); // Free memory for the entire command
                exit(EXIT_SUCCESS);
            } else {
                // Parent process
                // Close the write end of the pipe
                close(pipefd[1]);

                // Redirect stdin to the read end of the pipe
                dup2(pipefd[0], STDIN_FILENO);
                close(pipefd[0]);

                // Execute the second command
                execute_command(second_command, background);
                free_args(second_command); // Free memory for the second command

                // Wait for the first command to finish
                waitpid(pid, NULL, 0);
            }
        } else {
            // No pipe found, execute the command as usual
            execute_command(args, background);
        }

        // Free memory allocated for command arguments
        free_args(args);
    }

    return 0;
}
