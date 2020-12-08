#ifndef ARGUMENTS_PARSER_H
#define ARGUMENTS_PARSER_H

#include <stdio.h>
#include <string.h>
#include <netdb.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct arguments {
    char* user;
    char* password;
    char* host_name;
    char* path;
    char* file_name;
} arguments;

int parseArguments(char* url, arguments *args);
int getIP(char *ip, char *host);

#endif