/*
CPE 357 Asgn 5
Author: Dylan Baxter (dybaxter)
File: mytalk.c
Description: This file contains main functionality f
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

/*Option mask constants*/
#define VERBOSE 0x01
#define ACCEPT 0x02
#define NOWNDW 0x04
#define DEFAULT_BACKLOG 100

/*Others*/
#define LINE_LENGTH 256
#define DEBUG 1

/*Prototypes*/
void init_hint(struct addrinfo* hint);

int main(int argc, char* const argv[]) {
    /*Parse options*/
    /*Iterate through options */
    uint8_t optMask = 0;
    int opt;
    int sock;
    int argRemain;
    int port;
    char* end;
    socklen_t len;
    struct addrinfo* infoptr;
    struct addrinfo* curr;
    struct addrinfo hint;
    struct sockaddr_in sa;
    init_hint(&hint);
    hint.ai_family = AF_INET;

    char inBuf[LINE_LENGTH];
    int numRead = 0;

    /*Polling*/
    struct pollfd fds[2];
    /*Set stdin poll*/
    fds[0].fd = 1;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    /*Set socket poll*/
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    /*Search for option -n and assign k if it exsists*/
    while((opt = getopt(argc, argv,"vaN")) != -1){
      /*Flag Verbose*/
      if(opt == 'v'){
         optMask = optMask | VERBOSE;
      }
      /*Flag accept*/
      else if(opt == 'a'){
          optMask = optMask | ACCEPT;
      }
      /*Flag N*/
      else if(opt == 'N'){
          optMask = optMask | NOWNDW;
      }
    }

    argRemain = optind - argc;

    /*If hostname is included, act as client*/
    if(argRemain == 2){
        if(DEBUG){
            printf("Acting as client\n");
        }
        /*Look up peer address*/
        if( -1 == getaddrinfo(argv[optind], argv[optind+1], &hint, &infoptr)){
            return -1;
        }
        /*Create socket and connect to server*/
        curr = infoptr;
        while(curr != NULL){
            if(DEBUG){
                printf("Extablishing socket\n");
            }
            sock = socket(
                curr->ai_family,
                curr->ai_socktype,
                curr->ai_protocol);
            if(sock == -1){
                /*Error*/
                perror("Socket");
                exit(EXIT_FAILURE);
            }
            if(DEBUG){
                printf("Connecting socket\n");
            }
            if(-1 != connect(sock,curr->ai_addr, curr->ai_addrlen)){
                fds[1].fd = sock;
                printf("Connection Established\n");
                break;
            }
            curr = curr->ai_next;
        }
        if(curr == NULL){
            printf("No Connection\n");
            return -1;
        }

        /*Turn on windowing*/
        start_windowing();

        /*Send and recieve loop*/
        while(!(has_hit_eof())){
            if(DEBUG){
                printf("Polling\n");
            }
            poll(fds,2,-1);
            if(fds[0].revents & POLLIN){
                if(DEBUG){
                    printf("User input detected\n");
                }
                update_input_buffer();
                if(has_whole_line()){
                    if(-1 == read_from_input(inBuf, LINE_LENGTH)){
                        perror("Read from Terminal");
                        exit(EXIT_FAILURE);
                    }
                    send(sock, inBuf, LINE_LENGTH, 0);
                }
            }
            if(fds[1].revents & POLLIN){
                if(DEBUG){
                    printf("Incoming message detected\n");
                }
                if((numRead = recv(sock, inBuf, LINE_LENGTH, 0))){
                    if(-1 == write_to_output(inBuf, numRead)){
                        perror("Write to buffer");
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }
        if(DEBUG){
            printf("Closing\n");
        }
        /*Stop windowing*/
        stop_windowing();

        /*close sockets*/
        close(sock);
    }

    /*If not, act as server*/
    else if(argRemain == 1){
        if(DEBUG){
            printf("Acting as server\n");
        }
        /*Create a socket*/
        if(-1==(sock = socket(AF_INET, SOCK_STREAM, 0))){
            perror("Server Socket");
            exit(EXIT_FAILURE);
        }
        fds[1].fd = sock;

        /*Attach address*/
        sa.sin_family = AF_INET;
        port = strtol(argv[1], &end, 10);
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = htonl(INADDR_ANY);

        if(DEBUG){
            printf("Binding...\n");
        }
        bind(sock, (struct sockaddr*)&sa, sizeof(sa));

        /*Wait for Connection*/
        if(DEBUG){
            printf("Listening...\n");
        }
        listen(sock, DEFAULT_BACKLOG);

        /*Accept connection*/
        len = sizeof(sa);
        accept(sock, (struct sockaddr*)&sa, &len);

        /*Send and recieve loop*/
        while(!(has_hit_eof())){
            if(DEBUG){
                printf("Polling...\n");
            }
            poll(fds,2,-1);
            if(fds[0].revents & POLLIN){
                if(DEBUG){
                    printf("From user\n");
                }
                update_input_buffer();
                if(has_whole_line()){
                    if(-1 == read_from_input(inBuf, LINE_LENGTH)){
                        perror("Read from Terminal");
                        exit(EXIT_FAILURE);
                    }
                    send(sock, inBuf, LINE_LENGTH, 0);
                }
            }
            if(fds[1].revents & POLLIN){
                if(DEBUG){
                    printf("From Connection\n");
                }
                if((numRead = recieve(sock, inBuf, LINE_LENGTH, 0))){
                    if(-1 == write_to_output(inBuf, numRead)){
                        perror("Write to buffer");
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }
        if(DEBUG){
            printf("Closing...\n");
        }
        /*Close sockets*/
        close(sock);
    }
    else{
        printf("You Never should have come here\n");
        /*Error!*/
    }
    return 0;
}


 void init_hint(struct addrinfo* hint){
    hint->ai_flags = 0;
    hint->ai_family = AF_INET;
    hint->ai_socktype = 0;
    hint->ai_protocol = 0;
    hint->ai_addrlen = 0;
    hint->ai_addr = NULL;
    hint->ai_canonname = NULL;
    hint->ai_next = NULL;
}