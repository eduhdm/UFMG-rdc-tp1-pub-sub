#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <string>
#include <list>
#include <iostream>
#include <iterator>

#define MAX_LENGTH 500

using namespace std;

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

bool is_valid_message(string message) {
    if(message.size() > MAX_LENGTH || message[message.size() - 1] != '\n') {
        return false;
    }

    for (int i = 0; i < message.size(); i++) {
        char letter = message[i];
        if(
            !(is_valid_letter(letter)
            || is_valid_special_letter(letter)
            || (letter >= '0' && letter <= '9')
            || letter == '\0')
        ) {
            return false;
        }
    }

    return true;
}

bool is_valid_tag(string tag) {
    for (int i = 0; i < tag.size(); i++) {
        if(!is_valid_letter(tag[i])) {
            return false;
        }
    }

    return true;
}

string get_identified_tag(string tag) {
    return "<" + tag + ">";
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
