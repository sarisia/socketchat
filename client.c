#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

void on_signal(int);
void *listen_routine(void *);
void exit_with_usage();

char *name;
int sockfd;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        exit_with_usage();
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));
    if (!inet_aton(argv[1], &addr.sin_addr)) {
        printf("Invalid IPv4 address: %s\n", argv[1]);
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Failed to create socket\n");
        return 1;
    }

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr))) {
        printf("Failed to connect\n");
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

    char buf[BUFSIZ];
    char sbuf[BUFSIZ];
    size_t len;

    while (1) {
        printf("Enter your name: ");
        if (fgets(buf, BUFSIZ, stdin) != NULL) {
            break;
        }
    }

    if (send(sockfd, buf, strlen(buf), 0) == -1) {
        printf("Failed to send.\n");
        close(sockfd);
        return 0;
    }

    pthread_t thread;
    if (pthread_create(&thread, NULL, (void *)listen_routine, (void *)&sockfd)) {
        printf("Failed to start listen routine\n");
        close(sockfd);
        return 1;
    }

    len = strlen(buf);
    buf[len-1] = '\0';
    name = (char *)malloc(sizeof(char) * len);
    strcpy(name, buf);

    int quit = 0;
    while (!quit) {
        // printf("%s > ", name);
        if (fgets(buf, BUFSIZ, stdin) == NULL) {
            continue;
        }

        len = strlen(buf);
        if (len < 1) {
            continue;
        }

        if (buf[0] == '/') {
            // handle commands
            // help
            if (len > 4 && !strncmp(&(buf[1]), "help", 4)) {
                printf("/help - show help\n");
                printf("/quit, /exit - disconnect from server\n");
                printf("%s > ", name);
                continue;
            } else if (len > 4 && !strncmp(&(buf[1]), "quit", 4) || !strncmp(&(buf[1]), "exit", 4)) {
                strcpy(sbuf, "Q@\n");
                quit = 1;
            }
        } else {
            // normal message sending
            snprintf(sbuf, BUFSIZ, "%s%s", "M@", buf);            
        }

        if (send(sockfd, sbuf, strlen(sbuf), 0) == -1) {
            printf("Failed to send\n");
        }
    }

    sleep(5);
    close(sockfd);
    printf("\r");
    fflush(stdout);
    return 0;
}

void on_signal(int sig) {
    printf("\rInterrupt...\n");
    close(sockfd);
    exit(0);
}

void *listen_routine(void *arg) {
    int sockfd = *(int *)arg;
    char buf[BUFSIZ];
    int recvlen;
    while (1) {
        recvlen = recv(sockfd, buf, BUFSIZ, 0);
        if(recvlen <= 0) {
            printf("Closed by peer\n");
            exit(0);
        }
        buf[recvlen] = '\0';
        printf("\r%s", buf);
        printf("%s > ", name);
        fflush(stdout);
    }
}

void exit_with_usage() {
    printf("Usage: client <ip> <port>\n");
    exit(0);
}
