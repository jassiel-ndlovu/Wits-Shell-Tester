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
	char* cmd = NULL;
	size_t cmd_size = 0;
	size_t cmd_chars_size;

	while (1) {
		////////////////////// SHELL HEADER SECTION //////////////////////

		printf("witsshell> ");

		////////////////////// SHELL PROCESSING //////////////////////

		// int pid = fork();
		Node* head = NULL;

		cmd_chars_size = getline(&cmd, &cmd_size, stdin);

		char* token;
		char* command = strdup(cmd);

		while ((token = strsep(&command, " ")) != NULL) {
			push_back(&head, token);
		}

		printf("Executing %s command...\n", head->data);

		////////////////////// SHELL TERMINATION //////////////////////
		
		free_list(head);
		
		free(command);

		if (feof(stdin)) {
			printf("\nCTRL+D detected. Exiting shell...\n");
			exit(0);
		}

		if (strcmp(cmd, "exit")) break;
	}

	free(cmd);
	return(0);
}
