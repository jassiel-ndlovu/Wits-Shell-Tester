#include "linked_list.h"

Node* create_node(const char* data) {
    Node* new_node = (Node*)malloc(sizeof(Node));

    if (!new_node) {
        perror("Failed to create node");
        exit(EXIT_FAILURE);
    }

    new_node->data = strdup(data);
    new_node->next = NULL;

    return new_node;
}

void push_front(Node** head, const char* data) {
    Node* new_node = create_node(data);
    new_node->next = *head;
    *head = new_node;
}

void push_back(Node** head, const char* data) {
    Node* new_node = create_node(data);

    if (*head == NULL) {
        *head = new_node;
        return;
    }

    Node* temp = *head;

    while (temp->next != NULL) {
        temp = temp->next;
    }

    temp->next = new_node;
}

void delete_node(Node** head, const char* key) {
    Node* temp = *head;
    Node* prev = NULL;

    if (temp != NULL && temp->data == key) {
        *head = temp->next;
        free(temp);
        return;
    }

    while (temp != NULL && temp->data != key) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) return;

    prev->next = temp->next;
    free(temp);
}

void print_list(Node* head) {
    Node* temp = head;

    while (temp != NULL) {
        printf("%s -> ", temp->data);
        temp = temp->next;
    }

    printf("NULL\n");
}

void free_list(Node* head) {
    Node* temp;

    while (head != NULL) {
        temp = head;
        head = head->next;
        free(temp);
    }
}
