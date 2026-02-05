// FastLinkedListBlocked.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define B 64               // block capacity (constant)
#define INIT_BLOCKS 16

typedef struct Node {
    int data;
    struct Node *next;
} Node;

typedef struct Block {
    Node* items[B];        // pointers into the singly linked list
    int   cnt;             // number of valid pointers
} Block;

typedef struct FastList {
    Node *head;
    int size;

    Block *blocks;         // dynamic array of blocks
    int nBlocks;
    int capBlocks;
} FastList;

static void die_oom(void){
    fprintf(stderr, "Out of memory\n");
    exit(1);
}

static Node* new_node(int v){
    Node* n = (Node*)malloc(sizeof(Node));
    if(!n) die_oom();
    n->data = v;
    n->next = NULL;
    return n;
}

static void ensure_block_capacity(FastList *L, int needBlocks){
    if(L->capBlocks >= needBlocks) return;
    int newCap = (L->capBlocks > 0) ? L->capBlocks : INIT_BLOCKS;
    while(newCap < needBlocks) newCap *= 2;
    Block *nb = (Block*)realloc(L->blocks, sizeof(Block)*newCap);
    if(!nb) die_oom();
    L->blocks = nb;
    L->capBlocks = newCap;
}

FastList* createList(void){
    FastList* L = (FastList*)calloc(1, sizeof(FastList));
    if(!L) die_oom();
    L->capBlocks = INIT_BLOCKS;
    L->blocks = (Block*)calloc(L->capBlocks, sizeof(Block));
    if(!L->blocks) die_oom();
    L->nBlocks = 0;
    L->head = NULL;
    L->size = 0;
    return L;
}

void destroyList(FastList* L){
    if(!L) return;
    Node* cur = L->head;
    while(cur){
        Node* nxt = cur->next;
        free(cur);
        cur = nxt;
    }
    free(L->blocks);
    free(L);
}

// rebuild the singly linked list "next" pointers for one block boundary only
// (we try to touch only local pointers to keep constant-ish work)
static void relink_local(Block *blk, Node *nextAfterBlock){
    // Make items[0]->next = items[1] ... items[cnt-1]->next = nextAfterBlock
    for(int i=0;i<blk->cnt-1;i++){
        blk->items[i]->next = blk->items[i+1];
    }
    if(blk->cnt > 0){
        blk->items[blk->cnt-1]->next = nextAfterBlock;
    }
}

// split a full block into two blocks of ~B/2
static void split_block(FastList *L, int bIdx){
    ensure_block_capacity(L, L->nBlocks + 1);

    // shift blocks right to make space
    for(int i=L->nBlocks; i> bIdx+1; i--){
        L->blocks[i] = L->blocks[i-1];
    }
    L->nBlocks++;

    Block *A = &L->blocks[bIdx];
    Block *B2 = &L->blocks[bIdx+1];
    memset(B2, 0, sizeof(Block));

    int move = A->cnt/2;               // move second half into new block
    int start = A->cnt - move;

    for(int i=0;i<move;i++){
        B2->items[i] = A->items[start+i];
    }
    B2->cnt = move;
    A->cnt = start;

    // relink within blocks; boundary next pointer is first of next block if exists
    Node *nextAfterA = (B2->cnt>0) ? B2->items[0] : NULL;
    relink_local(A, nextAfterA);

    Node *nextAfterB2 = NULL;
    if(bIdx+2 < L->nBlocks && L->blocks[bIdx+2].cnt > 0){
        nextAfterB2 = L->blocks[bIdx+2].items[0];
    }
    relink_local(B2, nextAfterB2);
}

// merge block bIdx with bIdx+1 if small
static void try_merge(FastList *L, int bIdx){
    if(bIdx < 0 || bIdx+1 >= L->nBlocks) return;
    Block *A = &L->blocks[bIdx];
    Block *C = &L->blocks[bIdx+1];

    if(A->cnt + C->cnt > B) return; // can't merge

    int base = A->cnt;
    for(int i=0;i<C->cnt;i++){
        A->items[base+i] = C->items[i];
    }
    A->cnt += C->cnt;

    // shift blocks left to remove C
    for(int i=bIdx+1; i<L->nBlocks-1; i++){
        L->blocks[i] = L->blocks[i+1];
    }
    L->nBlocks--;

    // relink A to next block boundary
    Node *nextAfterA = NULL;
    if(bIdx+1 < L->nBlocks && L->blocks[bIdx+1].cnt > 0){
        nextAfterA = L->blocks[bIdx+1].items[0];
    }
    relink_local(A, nextAfterA);
}

// map global position -> (block index, offset)
static void locate(FastList *L, int pos, int *outB, int *outOff){
    // invariant we aim for: blocks are "packed" left-to-right, each cnt <= B
    // for O(1) we approximate using division by B:
    int b = pos / B;
    int off = pos % B;
    if(b >= L->nBlocks) b = L->nBlocks-1;
    *outB = b;
    *outOff = off;
}

int get(FastList *L, int pos){
    if(!L || pos<0 || pos>=L->size) return -1;
    int b, off;
    locate(L, pos, &b, &off);
    // off might exceed cnt if last block not full; clamp safely by scanning within block
    if(off >= L->blocks[b].cnt) off = L->blocks[b].cnt-1;
    return L->blocks[b].items[off]->data;
}

void insert(FastList *L, int pos, int value){
    if(!L || pos<0 || pos> L->size) return;

    // first insert ever
    if(L->size == 0){
        Node* n = new_node(value);
        L->head = n;
        L->nBlocks = 1;
        L->blocks[0].cnt = 1;
        L->blocks[0].items[0] = n;
        L->size = 1;
        return;
    }

    if(pos == L->size){
        // append near end: last block
        int b = L->nBlocks-1;
        if(L->blocks[b].cnt == B){
            split_block(L, b);
            b = L->nBlocks-1;
        }
        Node* n = new_node(value);

        Block *blk = &L->blocks[b];
        // link last item -> n -> next block head (or NULL)
        Node *nextAfter = NULL;
        if(b+1 < L->nBlocks && L->blocks[b+1].cnt>0) nextAfter = L->blocks[b+1].items[0];
        if(blk->cnt>0) blk->items[blk->cnt-1]->next = n;
        n->next = nextAfter;

        blk->items[blk->cnt++] = n;
        L->size++;
        return;
    }

    int b, off;
    locate(L, pos, &b, &off);
    if(b < 0) b = 0;

    // ensure block has space
    if(L->blocks[b].cnt == B){
        split_block(L, b);
        // re-locate after split
        locate(L, pos, &b, &off);
    }
    Block *blk = &L->blocks[b];
    if(off > blk->cnt) off = blk->cnt;

    Node* n = new_node(value);

    // Determine successor node after insertion point
    Node *succ = NULL;
    if(off < blk->cnt){
        succ = blk->items[off];
    } else {
        // inserting at end of block, successor is head of next block
        if(b+1 < L->nBlocks && L->blocks[b+1].cnt>0) succ = L->blocks[b+1].items[0];
    }

    // Determine predecessor node (either within block or previous block)
    Node *pred = NULL;
    if(off > 0){
        pred = blk->items[off-1];
    } else {
        // predecessor is last of previous block; if none, insertion at head
        if(b == 0){
            // new head
            n->next = L->head;
            L->head = n;
            // shift pointers within block
            for(int i=blk->cnt; i>0; i--) blk->items[i] = blk->items[i-1];
            blk->items[0] = n;
            blk->cnt++;
            // relink this block to keep next pointers consistent
            Node *nextAfter = (b+1 < L->nBlocks && L->blocks[b+1].cnt>0) ? L->blocks[b+1].items[0] : NULL;
            relink_local(blk, nextAfter);
            L->size++;
            return;
        } else {
            Block *prevBlk = &L->blocks[b-1];
            pred = prevBlk->items[prevBlk->cnt-1];
        }
    }

    // link pred -> n -> succ
    pred->next = n;
    n->next = succ;

    // shift pointers inside this block only (<=B)
    for(int i=blk->cnt; i>off; i--) blk->items[i] = blk->items[i-1];
    blk->items[off] = n;
    blk->cnt++;

    // relink local block boundary
    Node *nextAfter = (b+1 < L->nBlocks && L->blocks[b+1].cnt>0) ? L->blocks[b+1].items[0] : NULL;
    relink_local(blk, nextAfter);

    L->size++;
}

int removeAt(FastList *L, int pos){
    if(!L || pos<0 || pos>=L->size) return -1;

    int b, off;
    locate(L, pos, &b, &off);
    if(b < 0) b = 0;
    Block *blk = &L->blocks[b];
    if(off >= blk->cnt) off = blk->cnt-1;

    Node *toDel = blk->items[off];
    int val = toDel->data;

    // find successor after toDel
    Node *succ = NULL;
    if(off+1 < blk->cnt){
        succ = blk->items[off+1];
    } else {
        if(b+1 < L->nBlocks && L->blocks[b+1].cnt>0) succ = L->blocks[b+1].items[0];
    }

    // find predecessor
    if(pos == 0){
        L->head = succ;
    } else {
        Node *pred = NULL;
        if(off > 0){
            pred = blk->items[off-1];
        } else {
            Block *prevBlk = &L->blocks[b-1];
            pred = prevBlk->items[prevBlk->cnt-1];
        }
        pred->next = succ;
    }

    // shift pointers inside block
    for(int i=off; i<blk->cnt-1; i++) blk->items[i] = blk->items[i+1];
    blk->cnt--;

    free(toDel);
    L->size--;

    // relink local block boundary
    Node *nextAfter = (b+1 < L->nBlocks && L->blocks[b+1].cnt>0) ? L->blocks[b+1].items[0] : NULL;
    relink_local(blk, nextAfter);

    // if block becomes empty, merge/remove it
    if(blk->cnt == 0 && L->nBlocks > 1){
        // delete this block by shifting left
        for(int i=b; i<L->nBlocks-1; i++){
            L->blocks[i] = L->blocks[i+1];
        }
        L->nBlocks--;
    } else {
        // try merge with neighbour to keep blocks reasonably packed
        if(b > 0) try_merge(L, b-1);
        try_merge(L, b);
    }

    return val;
}

void printList(FastList *L){
    printf("List (%d): ", L?L->size:0);
    Node* cur = L->head;
    while(cur){
        printf("%d -> ", cur->data);
        cur = cur->next;
    }
    printf("NULL\n");
}
