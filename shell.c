#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "linked_list.h"

char* bin = "/bin/";
char* usr = "/usr/bin/";

int main(int argc, char *argv[]) {
    Node* path_list = create_node(bin);
    path_list->next = create_node(usr);

    FILE *file;
    char *command = NULL;
    size_t command_size = 0;
    size_t line_size;

    if (argc != 2) {
        printf("Usage: ./myshell batchfile\n");
        return 1;
    }

    file = fopen(argv[1], "r");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    // Read commands line by line using getline
    while ((line_size = getline(&command, &command_size, file)) != -1) {
        // Strip newline character if present
        command[strcspn(command, "\n")] = '\0';

        // Check if the command is 'exit' and terminate
        if (strcmp(command, "exit") == 0) {
            printf("Exiting shell...\n");
            break;
        }

        pid_t pid1 = fork();

        if (pid1 < 0) {
            perror("First fork failed");
            exit(EXIT_FAILURE);
        } else if (pid1 == 0) {
            // First child process

            // Call process_command, which will fork again (second fork)
            if (process_command(command, path_list) == -1) {
                exit(EXIT_FAILURE);  // Exit first child on failure
            }

            // First child waits for the second fork (created inside process_command)
            wait(NULL);  // Wait for the grandchild process to finish

            exit(EXIT_SUCCESS);  // First child exits after the command finishes
        } else {
            // Parent process waits for the first child to finish
            waitpid(pid1, NULL, 0);
        }

        // Execute the command (in a real shell, you'd handle this more robustly)
        printf("Executing command: %s\n", command);
    }

    free(command);
    fclose(file);
    return 0;
}

int process_command(char* _cmd, Node* path_list) {
    // Tokenize and process the command

    // Fork again (second fork) to run the actual command (e.g., ls)
    pid_t pid2 = fork();

    if (pid2 < 0) {
        perror("Second fork failed");
        return -1;
    } else if (pid2 == 0) {
        // This is the grandchild (second fork)
        // execv("/bin/ls");  // Replace process with the command (e.g., ls)
        // perror("execv failed");
        exit(EXIT_SUCCESS);  // Exit if execv fails
    } else {
        // First child (created in main) does nothing special here,
        // it will wait for the second fork (grandchild) in the main function
    }

    return 0;
}