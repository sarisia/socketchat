#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "llist.h"

typedef struct _payload {
    int conn;
    char *msg;
} payload;

void on_signal(int);
void *connect_routine(void *);
void *disconnect_routine(void *);
void *serveloop(void *);
void *handle_packet_routine(void *);
void broadcast(char *);
void exit_with_usage();

int sockfd;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        exit_with_usage();
    }

    // TODO: gomi
    FD_ZERO(&fds);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));
    if (!inet_aton(argv[1], &addr.sin_addr)) {
        printf("[main] Invalid IPv4 address: %s\n", argv[1]);
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("[main] Failed to create socket\n");
        return 1;
    }

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr))) {
        printf("[main] Failed to bind to socket\n");
        close(sockfd);
        return 1;
    }

    if (listen(sockfd, 10)) {
        printf("[main] Failed to listen\n");
        close(sockfd);
        return 1;
    }

    pthread_t thread;
    if (pthread_create(&thread, NULL, (void *)serveloop, NULL) != 0) {
        printf("[main] Failed to boot up serveloop\n");
        close(sockfd);
        return 1;
    }

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = on_signal;
    if (sigaction(SIGINT, &act, NULL)) {
        printf("Failed to set signal handler\n");
        close(sockfd);
        return 1;
    }

    while (1) {
        int conn = accept(sockfd, NULL, NULL);
        if (conn == -1) {
            printf("[main] Accept failed\n");
            close(sockfd);
            return 0;
        }
        pthread_t thread;
        if (pthread_create(&thread, NULL, (void *)connect_routine, (void *)&conn) == 0) {
            printf("[main] New connection fd: %d\n", conn);
        } else {
            printf("[main] Failed to spawn thread for fd: %d\n", conn);
            close(conn);
        }
    }
}

void on_signal(int sig) {
    printf("\rInterrupt...\n");
    close(sockfd);
    exit(0);
}

void *connect_routine(void *arg) {
    int conn = *(int *)arg;
    printf("[connect_routine] Handling socket %d\n", conn);

    char buf[BUFSIZ];
    printf("[connect_routine] Waiting for enter name...\n");
    int recvSize = recv(conn, buf, BUFSIZ, 0);
    buf[recvSize] = '\0';

    int slen = strlen(buf);
    if (slen < 1) {
        printf("[connect_routine] Invalid packet!\n");
        close(conn);
        return NULL;
    }
    
    buf[recvSize-1] = '\0';
    char *name = (char *)malloc(sizeof(char) * (slen + 1));
    if (name == NULL) {
        printf("[connect_routine] Out of memory!\n");
        return NULL;
    }
    strcpy(name, buf);
    ladd(conn, name);

    snprintf(buf, BUFSIZ, "### %s joined ###\n", name);
    broadcast(buf);
}

void *disconnect_routine(void *arg) {
    char buf[BUFSIZ];
    int conn = *(int *)arg;
    lnode *user = lget_user(conn);
    if (user == NULL) {
        printf("[disconnect_routine] User not found for fd: %d\n", conn);
        return NULL;
    }
    snprintf(buf, BUFSIZ, "### %s disconnected ###\n", user->name);
    broadcast(buf);
    lremove(conn);
    close(conn);
    free(user);
}

void *serveloop(void *arg) {
    fd_set rfd;
    int lrfd;
    char buf[BUFSIZ];
    int recvSize;
    int slen;
    pthread_t thread;
    fd_set f;
    int sel;
    struct timeval tout = {0, 0};

    printf("[serveloop] Booted!\n");
    while (1) {
        // pthread_mutex_lock(&fdmut);
        // rfd = fds;
        // lrfd = fdlen;
        // printf("Acquired\n");
        // pthread_mutex_unlock(&fdmut);

        // printf("[serveloop] Waiting select...\n");
        // int sel = select(lrfd+1, &rfd, NULL, NULL, &tout);
        // printf("[serveloop] Selected: %d\n", sel);
        pthread_mutex_lock(&mut);
        for (lnode *node = head; node != NULL; node = node->next) {
            int fd = node->val;
            // printf("Check socket %d\n", fd);
            FD_ZERO(&f);
            FD_SET(fd, &f);
            sel = select(fd+1, &f, NULL, NULL, &tout);
            // printf("Select %d\n", sel);
            if (sel > 0) {
                printf("[serveloop] Socket %d alive!\n", fd);
                recvSize = recv(fd, buf, BUFSIZ, 0);
                if (recvSize <= 0) {
                    printf("[serveloop] fd: %d disconnected!\n", fd);
                    if (pthread_create(&thread, NULL, (void *)disconnect_routine, (void *)&fd) != 0) {
                        printf("[serveloop] Failed to spawn thread to disconnect: %d\n", fd);
                        // FIXME
                    }
                    continue;
                }
                buf[recvSize] = '\0';
                slen = strlen(buf);
                if (slen < 2) {
                    printf("[serveloop] Invalid packet! fd: %d\n", fd);
                    continue;
                }
                buf[recvSize-1] = '\0';
                char *packet = (char *)malloc(sizeof(char) * (slen+1));
                if (packet == NULL) {
                    printf("[serveloop] Out of memory!\n");
                    continue;
                }
                strcpy(packet, buf);
                payload *pl = (payload *)malloc(sizeof(payload));
                if (pl == NULL) {
                    printf("[serverloop] Out of memory!");
                    continue;
                }
                pl->conn = fd;
                pl->msg = packet;
                if (pthread_create(&thread, NULL, (void *)handle_packet_routine, (void *)pl) != 0) {
                    printf("[serveloop] Failed to spawn packet handler thread!\n");
                    continue;
                }
            }
        }
        pthread_mutex_unlock(&mut);
        // sleep(1);
    }
}

void *handle_packet_routine(void *arg) {
    char buf[BUFSIZ];
    payload *pl = (payload *)arg;
    char *packet = pl->msg;
    int plen = strlen(packet);
    printf("[handle_packet_routine] Packet from fd %d: %s\n", pl->conn, packet);
    if (packet[1] != '@') {
        printf("[handle_packet_routine] Invalid packet!\n");
        // free(pl);
        return NULL;
    }

    lnode *user = lget_user(pl->conn);
    if (user == NULL) {
        printf("[handle_packet_routine] User not found: %d\n", pl->conn);
        // free(pl);
        return NULL;
    }

    switch (packet[0]) {
        case 'Q': // quit
            disconnect_routine((void *)&(pl->conn));
            break;
        case 'M':
            // TODO: odd bug
            snprintf(buf, BUFSIZ, "[%s] %s\n", user->name, &(packet[2]));
            broadcast(buf);
            break;
    }

    // free(pl);
}

void broadcast(char *msg) {
    pthread_mutex_lock(&fdmut);
    int lconn = fdlen;
    pthread_mutex_unlock(&fdmut);

    int conns[lconn];

    pthread_mutex_lock(&mut);
    lnode *node = head;
    for (int i = 0; i < lconn; ++i) {
        if (node == NULL) {
            conns[i] = 0;
            continue;
        }

        conns[i] = node->val;
        node = node->next;
    }
    pthread_mutex_unlock(&mut);

    for (int i = 0; i < lconn; ++i) {
        if (!conns[1]) {
            continue;
        }
        if (send(conns[i], msg, strlen(msg), 0) == -1) {
            printf("[broadcast] Failed to send to fd: %d\n", conns[i]);
        }
    }
}

void exit_with_usage() {
    printf("Usage: server <ip> <port>\n");
    exit(0);
}
