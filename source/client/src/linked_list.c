#include "linked_list.h"

void init_list(struct node **head, char *data)
{
    struct node *next_node = NULL;

    next_node = (struct node *) malloc(sizeof(struct node));

    strcpy(next_node -> data, data);
    next_node -> next = NULL;
    *head = next_node;
}

void push(struct node *head, char *data)
{
    struct node* current = head;

    while (current -> next != NULL)
    {
        current = current -> next;
    }

    current -> next = (struct node *) malloc(sizeof(struct node));
    strcpy(current -> next -> data, data);
    current -> next -> next = NULL;
}

void pop(struct node **head)
{
    struct node *next_node = NULL;

    if (*head == NULL) {
        return;
    }

    next_node = (*head)->next;
    free(*head);
    *head = next_node;
}

void delete_tail(struct node *head)
{
    if (head -> next == NULL) {
        free(head);
        return;
    }

    struct node *current = head;
    while (current -> next -> next != NULL) {
        current = current -> next;
    }

    free(current->next);
    current->next = NULL;
}

void print_list(struct node* head)
{
    struct node* current = head;

    while (current != NULL) {
        printf("%s\n", current -> data);
        current = current -> next;
    }
}
