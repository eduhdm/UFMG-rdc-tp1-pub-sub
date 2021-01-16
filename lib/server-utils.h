#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <list>

using namespace std;

bool is_valid_letter(char letter);

bool is_valid_special_letter(char letter);

bool is_valid_message(string message);

bool is_valid_tag(string tag);

bool should_client_recv_message(string message, int client_id);

string get_tag_text(string tag);

string get_identified_tag(string tag);

list<string> split_string(string message, string separator);

list<string> get_tags(string message);



