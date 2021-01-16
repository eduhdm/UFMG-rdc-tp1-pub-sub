#include "lib/common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void usage(int argc, char **argv) {
    printf("usage: %s <server IP> <server port>\n", argv[0]);
    printf("example: %s 127.0.0.1 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

#define BUFSZ 500

void * client_thread(void *data) {
    int * socket_s = (int *) data;
    char buf[BUFSZ];
    memset(buf, 0, BUFSZ);
    size_t count = -1;
    while(1) {
        if (count == 0) {
            break;
        }
        fgets(buf, BUFSZ-1, stdin);
        count = send(*socket_s, buf, strlen(buf)+1, 0);
    }

    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argc, argv);
    }

    struct sockaddr_in storage;
    if (0 != addrparse(argv[1], argv[2], &storage)) {
        usage(argc, argv);
    }

    int s;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        logexit("socket");
    }
    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != connect(s, addr, sizeof(struct sockaddr))) {
        logexit("connect");
    }

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);

    size_t count = -1;

    char buf[BUFSZ];
    memset(buf, 0, BUFSZ);

    printf("Send a message to the server by typing below:\n");
    while(1) {
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, &s);

        count = recv(s, buf, BUFSZ - 1, 0);
        if (count == 0) {
            break;
        }

        printf("%s", buf);
        memset(buf, 0, BUFSZ);
    }

    close(s);
    exit(EXIT_SUCCESS);
}
