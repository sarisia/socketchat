#pragma once
#ifndef _LLIST_H_

#define _LLIST_H_

#include <stdlib.h>
#include <pthread.h>
#include <sys/select.h>

// linked list!
typedef struct _lnode {
    int val;
    char *name;
    struct _lnode *next;
} lnode;

extern lnode *head;
extern pthread_mutex_t mut;

fd_set fds;
extern int fdlen;
extern pthread_mutex_t fdmut;

void ladd(int, char *);
void lremove(int);
lnode *lget_user(int);
void ldump_fds();

#endif
