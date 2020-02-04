#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "llist.h"
#include <sys/select.h>

lnode *head = NULL;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

int fdlen = 0;
pthread_mutex_t fdmut = PTHREAD_MUTEX_INITIALIZER;

void ladd(int i, char *name) {
    lnode *node = (lnode *)malloc(sizeof(lnode));
    if (node == NULL) {
        printf("Failed to allocate memory: lnode\n");
        return;
    }

    pthread_mutex_lock(&mut);
    node->val = i;
    node->name = name;
    node->next = head;
    head = node;
    ldump_fds();
    pthread_mutex_unlock(&mut);

    printf("[ladd] Registered fd: %d, name: %s\n", i, name);
}

void lremove(int i) {
    pthread_mutex_lock(&mut);

    lnode *prev, *cur;
    prev = NULL;
    cur = head;
    while (cur != NULL) {
        if (cur->val == i) {
            if (prev == NULL) {
                head = NULL;
            } else {
                prev->next = cur->next;
                free(cur->name);
                free(cur);
            }
            break;
        }
        prev = cur;
        cur = prev->next;
    }
    ldump_fds();
    pthread_mutex_unlock(&mut);

    printf("[lremove] Removed fd: %d\n", i);
}

lnode *lget_user(int i) {
    lnode *ret = (lnode *)malloc(sizeof(lnode));
    lnode *node;
    pthread_mutex_lock(&mut);
    for (node = head; node->val != i; node = node->next) {}
    // COPY!
    *ret = *node;
    pthread_mutex_unlock(&mut);

    return ret;
}

void ldump_fds() {
    pthread_mutex_lock(&fdmut);
    // FD_ZERO(&fds);
    fdlen = 0;
    for (lnode *node = head; node != NULL; node = node->next) {
        // FD_SET(node->val, &fds);
        ++fdlen;
    }
    pthread_mutex_unlock(&fdmut);
}
