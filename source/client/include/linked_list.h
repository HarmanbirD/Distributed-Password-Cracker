#ifndef CLIENT_LINKED_LIST_H
#define CLIENT_LINKED_LIST_H

#include <stdlib.h>
#include <string.h>
#include <printf.h>

struct node
{
    char            data[512];
    struct node     *next;
};

void                init_list(struct node **head, char *data);
void                push(struct node *head, char *data);
void                pop(struct node **head);
void                delete_tail(struct node *head);
void                print_list(struct node* head);

#endif //CLIENT_LINKED_LIST_H
