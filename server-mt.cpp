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
#define MAX_LENGTH 500

void usage(int argc, char **argv) {
    printf("usage: %s <server port>\n", argv[0]);
    printf("example: %s 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

map<int, string> client_connections;

string get_tag_text(string tag) {
    return tag.substr(1, tag.size());
}

bool is_valid_letter(char letter) {
    char uppercaseChar = toupper(letter);

    return uppercaseChar >= 'A' && uppercaseChar <= 'Z';
}

bool is_valid_special_letter(char letter) {
    return (
        letter == ' '
        || letter == ','
        || letter == '.'
        || letter == '?'
        || letter == '!'
        || letter == ':'
        || letter == ';'
        || letter == '+'
        || letter == '-'
        || letter == '*'
        || letter == '/'
        || letter == '='
        || letter == '@'
        || letter == '#'
        || letter == '$'
        || letter == '%'
        || letter == '('
        || letter == ')'
        || letter == '['
        || letter == ']'
        || letter == '{'
        || letter == '}'
        || letter == '\n'
    );
}

bool is_valid_tag(string tag) {
    for (int i = 0; i < tag.size(); i++) {
        if(!is_valid_letter(tag[i])) {
            return false;
        }
    }

    return true;
}

bool is_valid_message(string message) {
    if(message.size() > MAX_LENGTH || message[message.size() - 1] != '\n') {
        return false;
    }

    for (int i = 0; i < message.size(); i++) {
        char letter = message[i];
        if(
            !(is_valid_letter(letter)
            || is_valid_special_letter(letter)
            || (letter >= '0' && letter <= '9'))
        ) {
            return false;
        }
    }

    return true;
}

string get_identified_tag(string tag) {
    return "<" + tag + ">";
}

bool client_has_tag(string tag, int client_id) {
    string client_tags = client_connections[client_id];
    printf("client_has_tag %s in %s.\n", tag.c_str(), client_tags.c_str());
    return client_tags.find(get_identified_tag(tag)) != string::npos;
}

list<string> split_string(string message, string separator) {
    size_t pos = message.find(separator);
    string token;
    list<string> token_list;
    while (pos != string::npos) {
        token = message.substr(0, pos);
        token_list.push_back(token);

        message.erase(0, pos + separator.length());
        pos = message.find(separator);
    }

    token_list.push_back(message);

    return token_list;
}

list<string> get_tags(string message) {
    list<string> words = split_string(message, " ");
    list<string> tags;
    std::list<string>::iterator it;

    for (it = words.begin(); it != words.end(); ++it) {
        string word = *it;
        if(word[0] == '#') {
            tags.push_back(get_tag_text(word));
        }
    }

    return tags;
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
        // printf("(%d) sending to %d: %d, %s\n", client_id_sender, it->first, client_id, client_tags.c_str());

        if(client_id != client_id_sender) {
            if(should_client_recv_message(message, client_id)) {
                // printf("(%d)uhuuuu sending to %d: %d, %s | message: %s\n ", client_id_sender, it->first, client_id, client_tags.c_str(), message.c_str());
                send(client_id, message.c_str(), message.size() + 1, 0);
            }
        }
    }
}

void add_tag(string message, int client_id) {
    string tag = get_tag_text(message);
    if(!is_valid_tag(get_tag_text(message))) {
        return;
    }

    bool has_tag = client_has_tag(tag, client_id);
    string feedback = has_tag
        ? "already subscribed " + message
        : "subscribed " + message;

    string *client_tags = &client_connections[client_id];
    if(!has_tag) {
        client_tags->append(get_identified_tag(tag));
    }

    // printf("%s, %s\n", feedback.c_str(), client_tags->c_str());
    send(client_id, feedback.c_str(), feedback.size() + 1, 0);
}

void remove_tag(string message, int client_id) {
    string tag = get_tag_text(message);
    if(!is_valid_tag(get_tag_text(message))) {
        return;
    }

    bool has_tag = client_has_tag(tag, client_id);
    string feedback = has_tag
        ? "unsubscribed " + message
        : "not subscribed " + message;

    string *client_tags = &client_connections[client_id];

    if(has_tag) {
        string identified_tag = get_identified_tag(tag);

        int pos = client_tags->find(identified_tag);
        client_tags->erase(pos, identified_tag.length());
    }

    // printf("%s, %s\n", feedback.c_str(), client_tags->c_str());
    send(client_id, feedback.c_str(), feedback.size() + 1, 0);
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

    char buf[BUFSZ];
    memset(buf, 0, BUFSZ);
    size_t count = -1;
    while(1) {
        count = recv(csock, buf, BUFSZ - 1, 0);
        if (count == 0) {
            printf("diconnecting client %d\n", csock);
            client_connections.erase(csock);
            close(csock);
            pthread_exit(EXIT_SUCCESS);
            break;
        }

        string message(buf);
        if(!is_valid_message(message)) {
            printf("disconnect\n");
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
