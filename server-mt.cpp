#include "common.h"

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

#define BUFSZ 1024

void usage(int argc, char **argv) {
    printf("usage: %s <server port>\n", argv[0]);
    printf("example: %s 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

struct client_data {
    int csock;
    struct sockaddr_storage storage;
    string tags;
};

struct thread_data {
    string client_id;
    map<string, struct client_data*> client_connections;
};

string get_client_key (int csock) {
    return "client_" + to_string(csock);
}

string get_tag_text(string tag) {
    return tag.substr(1, tag.size());
}

string get_identified_tag(string tag) {
    return "<" + tag + ">";
}

bool client_has_tag(string tag, struct client_data *client) {
    printf("client_has_tag tagsL:%s, tag: %s\n", client->tags.c_str(), tag.c_str());
    return client->tags.find(get_identified_tag(tag)) != string::npos;
}

list<string> split_string(string message, string separator) {
    size_t pos = message.find(separator);
    string token;
    list<string> token_list;
    while (pos != string::npos) {
        token = message.substr(0, pos);
        token_list.push_back(token);
        printf("split_string token %s %d\n", token.c_str(), pos);

        message.erase(0, pos + separator.length());
        pos = message.find(separator);
    }

    printf("split_string token %s %d\n", message.c_str(), pos);

    token_list.push_back(message);

    return token_list;
}

list<string> get_tags(string message) {
    bool is_init_tag = false;

    list<string> words = split_string(message, " ");
    list<string> tags;
    std::list<string>::iterator it;

    for (it = words.begin(); it != words.end(); ++it) {
        string word = *it;
        if(word[0] == '#') {
            printf("(get_tags) push tag %s %s\n", word.c_str(), get_tag_text(word).c_str());
            tags.push_back(get_tag_text(word));
        }
    }

    return tags;
}

bool should_client_recv_message(string message, struct client_data *client) {
    std::list<string>::iterator it;

    list<string> tags = get_tags(message);

    for (it = tags.begin(); it != tags.end(); ++it) {
        string tag = *it;
        if(client_has_tag(tag, client)) {
            return true;
        }
    }

    return false;
}

void send_message_to_subs(string message, struct thread_data *data, string client_sender) {
    map<string, struct client_data*>::iterator it;

    for (it=data->client_connections.begin(); it!=data->client_connections.end(); ++it) {
        struct client_data* client = (struct client_data*) it->second;
        printf("(%s) sending to %s: %d, %s\n", client_sender.c_str(), it->first.c_str(), client->csock, client->tags.c_str());

        if(it->first != client_sender) {
            bool should = should_client_recv_message(message, client);
            printf("should_client_recv_message %d\n", should);
            printf("send message client %s\n", client_sender.c_str());
            if(should) {
                send(client->csock, message.c_str(), message.size() + 1, 0);
            }
        }
    }
}

void add_tag(string message, struct client_data *client) {
    string tag = get_tag_text(message);
    bool has_tag = client_has_tag(tag, client);
    string feedback = has_tag
        ? "already subscribed " + message
        : "subscribed " + message;

    if(!has_tag) {
        client->tags.append(get_identified_tag(tag));
    }

    printf("%s, %s\n", feedback.c_str(), client->tags.c_str());
    send(client->csock, feedback.c_str(), feedback.size() + 1, 0);
}

void remove_tag(string message, struct client_data *client) {
    string tag = get_tag_text(message);
    bool has_tag = client_has_tag(tag, client);
    string feedback = has_tag
        ? "unsubscribed " + message
        : "not subscribed " + message;

    if(has_tag) {
        string identified_tag = get_identified_tag(tag);

        int pos = client->tags.find(identified_tag);
        client->tags.erase(pos, identified_tag.length());
    }

    printf("%s\n", feedback.c_str());
    send(client->csock, feedback.c_str(), feedback.size() + 1, 0);
}


void * client_thread(void *data) {
    struct thread_data *ctdata = (struct thread_data *) data;

    struct client_data *cdata = (struct client_data *) ctdata->client_connections.rbegin()->second;
    string client_id = ctdata->client_connections.rbegin()->first;
    // struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);
    // addrtostr(caddr, caddrstr, BUFSZ);

    char caddrstr[BUFSZ];
    printf("[log] connection from %s\n", caddrstr);
    printf("new client %s\n", client_id.c_str());

    char buf[BUFSZ];
    memset(buf, 0, BUFSZ);
    size_t count = -1;
    while(1) {
        printf("ai\n");
        count = recv(cdata->csock, buf, BUFSZ - 1, 0);
        printf("[msg] %s, %d bytes: %s, client_id: %s\n", caddrstr, (int)count, buf, client_id.c_str());
        string message(buf);

        message = message.substr(0, message.size() - 1);
        switch(message[0]) {
            case '+':
                add_tag(message, cdata);
                break;
            case '-':
                remove_tag(message, cdata);
                break;
            case '#':
                if (message == "##kill") {
                    exit(0);
                }
                break;
            default:
                send_message_to_subs(message, ctdata, client_id);
        }

        if (count == 0) {
            // Connection terminated.
            break;
        }
    }
    printf("saiu do whilezao\n");
    if (count != strlen(buf) + 1) {
        logexit("send");
    }
    // close(cdata->csock);

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

    map<string, struct client_data*> client_connections;
    map<string, struct client_data*>::iterator client_connections_it;

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    printf("bound to %s, waiting connections\n", addrstr);
    printf("cu\n");

    struct thread_data *thread_data_1 = (struct thread_data*) malloc(sizeof(struct thread_data*));
    thread_data_1->client_connections = map<string, struct client_data*>();
    while (1) {
        printf("ui\n");
        struct sockaddr_storage cstorage;
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage);

        int csock = accept(s, caddr, &caddrlen);
        if (csock == -1) {
            logexit("accept");
        }

        struct client_data *cdata = (struct client_data*) malloc(sizeof(struct client_data*));

        if (!cdata) {
            logexit("malloc");
        }
        cdata->csock = csock;
        cdata->tags = "";
        printf("cu 1\n");
        // memcpy(&(cdata->storage), &cstorage, sizeof(cstorage));
        thread_data_1->client_connections[get_client_key(cdata->csock)] = cdata;
        thread_data_1->client_id = get_client_key(cdata->csock);
        // client_connections_it = client_connections.fidnd(get_client_key(cdata->csock));
        pthread_t tid;
        printf("cu 2\n");
        pthread_create(&tid, NULL, client_thread, thread_data_1);;

    }

    exit(EXIT_SUCCESS);
}
