#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <ctype.h>

#include "linked_list.h"

char EXIT_KEY[] = "exit";
char CD_KEY[] = "cd";
char PATH_KEY[] = "path";
char ERROR_MESSAGE[30] = "An error has occurred\n";
char REDIRECT_SYMB[] = ">";

char* bin = "/bin/";
char* usr = "/usr/bin/";

Node* path_list;

int main(int MainArgc, char *MainArgv[]) {
    path_list = create_node(bin);
    path_list->next = create_node(usr);

    char* cmd = NULL;
    FILE* input_stream = stdin;

    // Batch mode
    if (MainArgc == 2) {
        input_stream = fopen(MainArgv[1], "r");

        if (input_stream == NULL) {
            write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
            exit(EXIT_FAILURE);
        }
    } else if (MainArgc > 2) {
        write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
        exit(EXIT_FAILURE);
    }

    while (1) {
        ////////////////////// SHELL HEADER SECTION //////////////////////
        if (input_stream == stdin)
            printf("witsshell> ");

        ////////////////////// SHELL PROCESSING //////////////////////
        size_t cmd_size = 0;
        int cmd_chars_size;

        // EOF Marker for batch mode
        if (input_stream != stdin && feof(input_stream)) {
            fclose(input_stream);
            printf("\n");
            free(cmd);
            exit(0);
        }

        cmd_chars_size = getline(&cmd, &cmd_size, input_stream);

        // CTRL + D
        if (input_stream == stdin && feof(input_stream)) {
            printf("\n");
            free(cmd);
            exit(0);
        }

		// EOF
        if (cmd_chars_size == -1) {
            break;
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
                free(command);
                free_list(path_list);
                exit(0);
            } else if (strncmp(command, CD_KEY, strlen(CD_KEY)) == 0) {
                char* dir = command + strlen(CD_KEY) + 1;
                cd_command(dir);
                free(command);
                continue;
            }

            // path command
            else if (strncmp(command, PATH_KEY, strlen(PATH_KEY)) == 0) {
                if (path_command(command) != 0) {
                    write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
                }

                free(command);
                continue;
            }

            // Parallel compute for non-built-in commands
            pids[num_pids] = fork();

            if (pids[num_pids] < 0) {
                perror("fork");
                free(command);
                free(cmd);
                exit(EXIT_FAILURE);
            } else if (pids[num_pids] == 0) {
				int error = process_command(command);

                if (error == -1) {
                    free(command);
                    exit(EXIT_FAILURE);
                }

                free(command);
                exit(EXIT_SUCCESS);
            }

            num_pids++;
            free(command);
        }

        free(cmd);

        for (int i = 0; i < num_pids; i++) {
            if (waitpid(pids[i], NULL, 0) < 0) {
                perror("waitpid");
            }
        }
    }

    if (MainArgc == 2)
        fclose(input_stream);

    free_list(path_list);

    return(0);
}

Node* handle_embedded_redirect(Node* head, const char* command) {
    char* cmd_copy = strdup(command);
    char* token = strtok(cmd_copy, " ");
    char* redirect_token = NULL;

    // Search for REDIRECT_SYMB
    while (token != NULL) {
        if (strchr(token, '>') != NULL) {
            redirect_token = token;
            break;
        }

        token = strtok(NULL, " ");
    }

    // We want to add parts before and after REDIRECT_SYMB separately from REDIRECT_SYMB
    if (redirect_token != NULL) {
        char* before_redirect = strtok(redirect_token, ">");
        char* after_redirect = strtok(NULL, ">");
        
        if (before_redirect != NULL) {
            push_back(&head, before_redirect);
        }

		push_back(&head, REDIRECT_SYMB);

        if (after_redirect != NULL) {
            push_back(&head, after_redirect);
        }
    }

    free(cmd_copy);

    return head;
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

int path_command(char* _cmd) {
	if (strlen(_cmd) <= 0) {
		perror("getline");
		return -1;
	}

	char* token;
	char* command = strdup(_cmd);

	// Linked list for commands
	Node* head = NULL;

	while ((token = strsep(&command, " ")) != NULL) {
		push_back(&head, token);
	}

	// path
	if (strcmp(head->data, PATH_KEY) == 0) {
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
                    curr_head = curr_head->next;
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

				// Overwrite and add the data if not found
				if (found == false) {
					free_list(path_list);
                    path_list = create_node(next_data);
				}

				// Free memory
				free(next_data);

				curr_head = curr_head->next;
			}
		}
	}

	return 0;
}

int process_command(char* _cmd) {
    if (strlen(_cmd) <= 0) {
        perror("getline");
        return -1;
    }

    char* command = strdup(_cmd);
    char* token;

    // Linked list for commands
    Node* head = NULL;
    while ((token = strsep(&command, " ")) != NULL) {
		if (strstr(token, REDIRECT_SYMB) != NULL)
			head = handle_embedded_redirect(head, token);
		else	
        	push_back(&head, token);
    }

    // Extract the program name to be run
    size_t len = strcspn(head->data, "\n");
    char* head_cpy = strndup(head->data, len);

    // Search path_list for a viable path
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

    // If no viable path is found, return an error
    if (!path || curr_path == NULL) {
        write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
        free(head_cpy);
        free_list(head);
        free(command);
        return -1;
    }

    // Count the number of arguments for the command
    int n = 0;
    Node* curr = head;
    while (curr && strcmp(curr->data, REDIRECT_SYMB) != 0) {
        curr = curr->next;
        n++;
    }

    // Create an array to hold arguments
    char* args[n + 1];
    curr = head;

    for (int i = 0; i < n; i++) {
        len = strcspn(curr->data, "\n");
        args[i] = strndup(curr->data, len);
        curr = curr->next;
    }

    args[n] = NULL;

    // Handle redirection if necessary
    if (curr && strcmp(curr->data, REDIRECT_SYMB) == 0) {
        if (curr->next == NULL || curr->next->next != 	NULL) {
            write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
            clean(path, head_cpy, args, n);
            return -1;
        }

        len = strcspn(curr->next->data, "\n");
        char* file_name = strndup(curr->next->data, len);

        int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		
        if (fd < 0) {
            perror("open");
            free(file_name);
            clean(path, head_cpy, args, n);
            return -1;
        }

        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2");
            free(file_name);
            clean(path, head_cpy, args, n);
            return -1;
        }

        close(fd);
        free(file_name);
    }

    // Execute the program
    // args[0] = "/home/jassielndlovu/CS3/OS/WitsShell-Project/Wits-Shell-Tester/tests";
    
    execv(path, args);

    // If execv fails
    perror("execv");
    clean(path, head_cpy, args, n);

    return 0;
}

void clean(char* path, char* head_cpy, char* args[], int n) {
    free(path);
    free(head_cpy);
    for (int i = 0; i < n; i++) {
        free(args[i]);
    }
}
