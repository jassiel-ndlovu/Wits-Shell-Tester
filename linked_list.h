#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
    char* data;
    struct Node* next;
} Node;

Node* create_node(const char* data);
void push_front(Node** head, const char* data);
void push_back(Node** head, const char* data);
void delete_node(Node** head, const char* key);
void print_list(Node* head);
void free_list(Node* head);

#endif
