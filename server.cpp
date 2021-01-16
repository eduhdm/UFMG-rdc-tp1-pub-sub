#include "lib/common.h"
#include "lib/server-utils.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <map>
#include <list>
#include <iostream>
#include <iterator>
#include <set>
#include <string> // for string class
#include <iostream>

using namespace std;

#define BUFSZ 500

map<int, string> client_connections;
bool should_disconnect = false;

void usage(int argc, char **argv) {
    printf("usage: %s <server port>\n", argv[0]);
    printf("example: %s 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

bool client_has_tag(string tag, int client_id) {
    string client_tags = client_connections[client_id];

    return client_tags.find(get_identified_tag(tag)) != string::npos;
}

bool should_client_recv_message(string message, int client_id) {
    std::list<string>::iterator it;

    list<string> tags = get_tags(message);

    for (it = tags.begin(); it != tags.end(); ++it) {
        string tag = *it;
        if(client_has_tag(tag, client_id)) {
            return true;
        }
    }

    return false;
}

void send_message_to_subs(string message, int client_id_sender) {
    map<int, string>::iterator it;

    for (it=client_connections.begin(); it!=client_connections.end(); ++it) {
        string client_tags = it->second;
        int client_id = it->first;

        if(client_id != client_id_sender) {
            if(should_client_recv_message(message, client_id)) {

                string final_message = message + '\n';
                send(client_id, final_message.c_str(), final_message.size(), 0);
            }
        }
    }
}

void add_tag(string message, int client_id) {
    string tag = get_tag_text(message);
    if(!is_valid_tag(tag)) {
        return;
    }

    string feedback = "already subscribed " + message + '\n';
    string *client_tags = &client_connections[client_id];

    if(!client_has_tag(tag, client_id)) {
        feedback = "subscribed " + message + '\n';
        client_tags->append(get_identified_tag(tag));
    }

    send(client_id, feedback.c_str(), feedback.size(), 0);
}

void remove_tag(string message, int client_id) {
    string tag = get_tag_text(message);
    if(!is_valid_tag(tag)) {
        return;
    }

    string feedback = "not subscribed " + message + '\n';
    string *client_tags = &client_connections[client_id];

    if(client_has_tag(tag, client_id)) {
        string identified_tag = get_identified_tag(tag);
        feedback = "subscribed " + message + '\n';

        int pos = client_tags->find(identified_tag);
        client_tags->erase(pos, identified_tag.length());
    }

    send(client_id, feedback.c_str(), feedback.size(), 0);
}

void close_connections() {
    map<int, string>::iterator it;

    for (it=client_connections.begin(); it!=client_connections.end(); ++it) {
        close(it->first);
    }
}

void * client_thread(void *data) {
    int *p_csock = (int*) data;
    int csock = *p_csock;
    string client_tags = client_connections[csock];

    printf("[log] connection from client %d\n", csock);

    size_t count = -1;
    char buf[BUFSZ];
    memset(buf, 0, BUFSZ);

    while(1) {
        count = recv(csock, buf, BUFSZ, 0);
        if (count == 0 || should_disconnect) {
            printf("diconnecting client %d\n", csock);
            client_connections.erase(csock);
            close(csock);
            pthread_exit(EXIT_SUCCESS);
            break;
        }

        string message(buf);
        if(!is_valid_message(message)) {
            printf("diconnecting client %d\n", csock);
            client_connections.erase(csock);
            close(csock);
            break;
        }

        message = message.substr(0, message.size() - 1);
        printf("receive msg client %d: %s\n", csock, message.c_str());
        switch(message[0]) {
            case '+':
                add_tag(message, csock);
                break;
            case '-':
                remove_tag(message, csock);
                break;
            default:
                if (message == "##kill") {
                    should_disconnect = true;
                    close_connections();
                    exit(0);
                } else {
                    send_message_to_subs(message, csock);
                }
        }
    }

    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argc, argv);
    }

    struct sockaddr_in storage;
    if (0 != server_sockaddr_init(argv[1], &storage)) {
        usage(argc, argv);
    }

    int s;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        logexit("socket");
    }

    int enable = 1;
    if (0 != setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) {
        logexit("setsockopt");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != bind(s, addr, sizeof(struct sockaddr))) {
        logexit("bind");
    }

    if (0 != listen(s, 10)) {
        logexit("listen");
    }

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    printf("bound to %s, waiting connections\n", addrstr);

    while (1) {
        struct sockaddr_storage cstorage;
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage);

        int csock = accept(s, caddr, &caddrlen);
        if (csock == -1) {
            logexit("accept");
        }

        client_connections[csock] = "";
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, &csock);
    }

    exit(EXIT_SUCCESS);
}
