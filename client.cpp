#include "common.h"

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

#define BUFSZ 1024

void * client_thread(void *data) {
	int * socket_s = (int *) data;
	char buf[BUFSZ];
	memset(buf, 0, BUFSZ);
	size_t count = -1;

	count = recv(*socket_s, buf, BUFSZ, 0);
	printf("%s\n", buf);
	while(1) {
		if (count == 0) {
			// Connection terminated.
			break;
		}
		count = recv(*socket_s, buf, BUFSZ, 0);
		printf("%s\n", buf);
	}
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

	printf("connected to %s\n", addrstr);

	char buf[BUFSZ];
	memset(buf, 0, BUFSZ);
	size_t count = -1;

	unsigned total = 0;
	while(1) {
		// count = recv(s, buf + total, BUFSZ - total, 0);
		if (count == 0) {
			// Connection terminated.
			break;
		}
		fgets(buf, BUFSZ-1, stdin);
		size_t count = send(s, buf, strlen(buf)+1, 0);

		pthread_t tid;
		printf("cu 2\n");
		pthread_create(&tid, NULL, client_thread, &s);
		total += count;
	}
	close(s);

	printf("received %u bytes\n", total);
	puts(buf);

	exit(EXIT_SUCCESS);
}
