#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#include "linked_list.h"

// int MainArgc, char *MainArgv[]
char EXIT_KEY[] = "exit";
char CD_KEY[] = "cd";
char PATH_KEY[] = "path\n";
char _PATH_KEY[] = "path";
char ERROR_MESSAGE[30] = "An error has occurred\n";
char REDIRECT_SYMB[] = ">";

char* bin = "/bin/";
char* usr = "/usr/bin/";

int main() {
	Node* path_list = create_node(bin);
	path_list->next = create_node(usr);

	char* cmd = NULL;

	while (1) {
		////////////////////// SHELL HEADER SECTION //////////////////////

		printf("witsshell> ");

		////////////////////// SHELL PROCESSING //////////////////////
		size_t cmd_size = 0;
		size_t cmd_chars_size;

		cmd_chars_size = getline(&cmd, &cmd_size, stdin);

		// CTRL + D
		if (feof(stdin)) {
			printf("\n");
			free(cmd);
			exit(0);
		}

		if (cmd_chars_size == -1) {
			perror("getline");
			free(cmd);
			continue;
		}

		// Parallel compute
		char* token;
		pid_t pids[32];
		int num_pids = 0;

		while ((token = strsep(&cmd, "&")) != NULL) {
			// Skip empty commands
			trim(token);

			if (strlen(token) == 0) continue;

			// Handle built-ins (exit and cd)
			size_t len = strcspn(token, "\n");
			char* command = strndup(token, len);

			trim(command);

			// exit command
			if (strcmp(command, EXIT_KEY) == 0) {
				free(cmd);
				exit(0);
			} else if (strncmp(command, CD_KEY, strlen(CD_KEY)) == 0) {
				char* dir = command + strlen(CD_KEY) + 1;

				cd_command(dir);

				free(command);
				continue;
			}

			// Parallel compute for non-built-in commands and path
			pids[num_pids] = fork();

			if (pids[num_pids] < 0) {
				perror("fork");
				free(command);
				exit(EXIT_FAILURE);
			} else if (pids[num_pids] == 0) {
				process_command(command, path_list);
				free(command);
				exit(EXIT_SUCCESS);
			}

			num_pids++;
			free(command);
		}

		for (int i = 0; i < num_pids; i++)
			waitpid(pids[i], NULL, 0);
	}

	free(cmd);
	free_list(path_list);

	return(0);
}

void trim(char* str) {
    if (str == NULL || strlen(str) == 0) {
        return;
    }

    // Trim leading spaces
    char* start = str;
    while (isspace((unsigned char) *start)) {
        start++;
    }

    // All spaces string
    if (*start == 0) {
        str[0] = 0;
        return;
    }

    // Trim trailing spaces
    char* end = str + strlen(str) - 1;
    while (end > start && isspace((unsigned char) *end)) {
        end--;
    }

    // Set the new null terminator
    *(end + 1) = '\0';

    // Shift the trimmed string to the start
    memmove(str, start, end - start + 2);
}

void cd_command(char* dir) {
	// cd
	size_t len = strcspn(dir, "\n");

	char* next_cpy = strndup(dir, len);

	if (chdir(next_cpy) != 0) {
		write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
	}

	free(next_cpy);
}

void process_command(char* _cmd, Node* path_list) {
	if (strlen(_cmd) <= 0) {
		perror("getline");
		return;
	}

	char* token;
	char* command = strdup(_cmd);

	// Linked list for commands
	Node* head = NULL;

	while ((token = strsep(&command, " ")) != NULL) {
		push_back(&head, token);
	}

	// ////////////////////// SHELL FORKING SECTION //////////////////////

	// path
	if (strcmp(head->data, PATH_KEY) == 0 || strcmp(head->data, _PATH_KEY) == 0) {
		// Free list if path command has not args
		if (!head->next) {
			free_list(path_list);
			path_list = NULL;
		} 
		else {
			// head list
			Node* curr_head = head->next;

			while (curr_head) {
				// Extract and format curr_head->data
				size_t len = strcspn(curr_head->data, "\n");
				char* next_data = (char*) malloc(len + 1);
				strncpy(next_data, curr_head->data, len);

				// Check path's existence
				if (access(next_data, F_OK) == -1) {
					write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
					free(next_data);
					continue;
				}

				// Search for path in path_list that matches next_data
				Node* curr = path_list;
				bool found = false;

				while (curr) {
					// Comparison
					if (strcmp(curr->data, next_data) == 0) {
						found = true;
						break;
					}

					curr = curr->next;
				}

				// Add the data if not found
				if (found == false) 
					push_back(&path_list, next_data);
				
				// Free memory
				free(next_data);

				curr_head = curr_head->next;
			}
		}
	}

	// non-built-in commands
	else {
		pid_t pid = fork();

		if (pid < 0) {
			// That is, if the fork failed

			perror("fork");
			exit(EXIT_FAILURE);
		} else if (pid == 0) {
			// Child process

			// Extract the program name to be run
			size_t len = strcspn(head->data, "\n");

			char* head_cpy = strndup(head->data, len);

			// Search path_list for viable path
			Node* curr_path = path_list;

			char* path = NULL;

			while (curr_path) {
				path = malloc(strlen(curr_path->data) + strlen(head_cpy) + 1);

				strcpy(path, curr_path->data);
				strcat(path, head_cpy);

				if (access(path, F_OK) == 0) break;

				free(path);
				path = NULL;
				curr_path = curr_path->next;
			}

			// If no viable path is found, return an error and exit fork
			// Otherwise, the viable path is now in curr->data
			if (!path) {
				write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
				free(head_cpy);
				exit(EXIT_FAILURE);
			}

			// Get the number of arguments to be passed to head program
			int n = 0;

			Node* curr = head;

			while (curr && strcmp(curr->data, REDIRECT_SYMB) != 0) {
				curr = curr->next;
				n++;
			}

			// Declare the array of size n to hold arguments
			char* args[n + 1];

			curr = head;

			for (int i = 0; i < n; i++) {
				len = strcspn(curr->data, "\n"); 

				args[i] = strndup(curr->data, len);

				curr = curr->next;
			}

			args[n] = NULL;

			// Check for redirection
			if (curr && strcmp(curr->data, REDIRECT_SYMB) == 0) {
				// Handle errors: missing file name
				if (curr->next == NULL) {
					write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
					free(head_cpy);
					free(path);
					for (int i = 0; i < n; i++) free(args[i]);
					exit(EXIT_FAILURE);
				}

				// Extract and format the file name
				len = strcspn(curr->next->data, "\n");
				char* file_name = strndup(curr->next->data, len);

				// File output
				int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);

				if (fd < 0) {
					perror("open");
					free(file_name);
					free(head_cpy);
					free(path);
					for (int i = 0; i < n; i++) free(args[i]);
					exit(EXIT_FAILURE);
				}

				if (dup2(fd, STDOUT_FILENO) < 0) {
					perror("dup2");
					free(file_name);
					free(head_cpy);
					free(path);
					for (int i = 0; i < n; i++) free(args[i]);
					exit(EXIT_FAILURE);
				}

				close(fd);
				free(file_name);
			}

			// Run the non-built-in program
			execv(path, args);

			perror("execv");

			// Free memory
			free(path);
			free(head_cpy);

			for (int i = 0; i < n; i++) {
				free(args[i]);
			}

			exit(EXIT_SUCCESS);
		} else {
			// Parent process
			int  saved_stdout = dup(STDOUT_FILENO);

			wait(NULL);

			// Restore stdout to the terminal
			dup2(saved_stdout, STDOUT_FILENO);

			close(saved_stdout);
		}
	}

	// ////////////////////// SHELL TERMINATION //////////////////////

	free_list(head);
	free(command);

	head = NULL;
}
