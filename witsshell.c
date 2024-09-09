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

int main() {
	char EXIT_KEY[] = "exit\n";
	char CD_KEY[] = "cd";
	char PATH_KEY[] = "path\n";
	char _PATH_KEY[] = "path";
	char ERROR_MESSAGE[30] = "An error has occurred\n";

	char BIN[100] = "/bin/";
	char USR_BIN[100] = "/usr/bin/";

	char* bin = "/bin/";
	char* usr = "/usr/bin/";

	Node* path_list = create_node(bin);
	path_list->next = create_node(usr);

	char* cmd = NULL;

	size_t cmd_size = 0;
	size_t cmd_chars_size;

	while (1) {
		////////////////////// SHELL HEADER SECTION //////////////////////

		printf("witsshell> ");

		////////////////////// SHELL PROCESSING //////////////////////

		Node* head = NULL;

		cmd_chars_size = getline(&cmd, &cmd_size, stdin);

		if (cmd_chars_size == -1 && !feof(stdin)) {
			perror("getline");
			continue;
		}

		char* token;
		char* command = strdup(cmd);

		// To prevent seg faults after strsep function
		char* command_cpy = strdup(command);
		command_cpy[strcspn(command_cpy, "\n")] = "\0";

		while ((token = strsep(&command, " ")) != NULL) {
			push_back(&head, token);
		}

		// ////////////////////// SHELL FORKING SECTION //////////////////////
		
		if (feof(stdin)) {
			printf("\nCTRL+D detected. Exiting shell...\n");
			free_list(head);
			free(command);
			free(command_cpy);
			free(cmd);
			exit(0);
		}

		// cd
		if (strcmp(head->data, CD_KEY) == 0) {
			if (head->next == NULL || head->next->next != NULL) {
				write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
			} else {
				pid_t pid = fork();

				if (pid < 0) {
					// That is, if the fork failed
					
					perror("fork");
					exit(EXIT_FAILURE);
				} else if (pid == 0) {
					// Child process

					size_t len = strcspn(head->next->data, "\n");

					char* next_cpy = (char*) malloc(len + 1); 

					strncpy(next_cpy, head->next->data, len);

					if (chdir(next_cpy) != 0) {
						write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
						free(next_cpy);
						exit(EXIT_FAILURE);
					}
					
					free(next_cpy);
					exit(EXIT_SUCCESS);
				} else {
					// Parent process

					wait(NULL);
				}
			}
		}
		
		// exit
		else if (strcmp(head->data, EXIT_KEY) == 0) {
			free_list(head);
			free(command);
			free(command_cpy);
			free(cmd);
			exit(0);
		}

		// path
		else if (strcmp(head->data, PATH_KEY) == 0 || strcmp(head->data, _PATH_KEY) == 0) {
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
				Node* curr = path_list;

				while (curr) {

				}
				// Extract the program name to be run
				size_t len = strcspn(head->data, "\n");

				char* head_cpy = (char*) malloc(len + 1);

				strncpy(head_cpy, head->data, len);

				// Build the paths
				char* usr_path = strdup(USR_BIN);
				char* bin_path = strdup(BIN);
				
				strcat(usr_path, head_cpy);
				strcat(bin_path, head_cpy);

				// Check is the paths exists
				int usr_access_error = access(usr_path, F_OK);
				int bin_access_error = access(bin_path, F_OK);

				if (usr_access_error == -1 && bin_access_error == -1) {
					write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
					free(head_cpy);
					free(usr_path);
					free(bin_path);
					exit(EXIT_FAILURE);
				}

				// Get the number of arguments to be passed to head program
				int n = 0;

				Node* curr = head;

				while (curr) {
					curr = curr->next;
					n++;
				}

				// Declare the array of size n to hold arguments
				char* args[n + 1];

				curr = head;

				for (int i = 0; i < n; i++) {
					size_t len = strcspn(curr->data, "\n");

					char* curr_cpy = (char*) malloc(len + 1);

					strncpy(curr_cpy, curr->data, len);

					args[i] = curr_cpy;

					curr = curr->next;
				}

				args[n] = NULL;

				// We now want to call the program with the accessible path
				if (bin_access_error != -1) {
					// Run the non-built-in program
					execv(bin_path, args);

					perror("execv");
				} else if (usr_access_error != -1) {
					execv(usr_path, args);

					perror("execv");
				}

				// Free memory
				for (int i = 0; i < n; i++) {
					free(args[i]);
				}

				exit(EXIT_SUCCESS);
			} else {
				// Parent process

				wait(NULL);
			}
		}

		// ////////////////////// SHELL TERMINATION //////////////////////

		free_list(head);
		free(command);
		free(command_cpy);

		head = NULL;
	}

	free(cmd);
	return(0);
}
