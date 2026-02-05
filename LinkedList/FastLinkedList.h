#ifndef FAST_LINKED_LIST_H
#define FAST_LINKED_LIST_H

typedef struct Node {
    int data;
    struct Node *next;
} Node;

typedef struct Block {
    Node *head;
    int count;
    struct Block *next;
} Block;

typedef struct {
    Block *bhead;
    int size;

    int B;                    // block size target (e.g., 64)
} FastLinkedList;

FastLinkedList* createList(void);
void destroyList(FastLinkedList *list);

int  get(FastLinkedList *list, int pos);
void insert(FastLinkedList *list, int pos, int value);
int  removeAt(FastLinkedList *list, int pos);

void printList(FastLinkedList *list);

#endif
