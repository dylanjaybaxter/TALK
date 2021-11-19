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
#define STDIN_FD 0
#define SOCK_FD 1

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
    fds[STDIN_FD].fd = 0;
    fds[STDIN_FD].events = POLLIN;
    fds[STDIN_FD].revents = 0;

    /*Set socket poll*/
    fds[SOCK_FD].events = POLLIN;
    fds[SOCK_FD].revents = 0;

    /*Search for option -n and assign k if it exsists*/
    while((opt = getopt(argc, argv,"vaN")) != -1){
      /*Flag Verbose*/
      if(opt == 'v'){
         optMask = optMask | VERBOSE;
         set_verbosity(1);
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

    argRemain = argc-optind;
    if(DEBUG){
        printf("argRemain: %d\n", argRemain);
    }
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
            sock = socket(AF_INET,SOCK_STREAM,0);
            if(sock == -1){
                /*Error*/
                perror("Socket");
                exit(EXIT_FAILURE);
            }
            if(DEBUG){
                printf("Connecting socket\n");
            }
            if(-1 != connect(sock,curr->ai_addr, curr->ai_addrlen)){
                fds[SOCK_FD].fd = sock;
                printf("Connection Established\n");
                break;
            }else{
                close(sock);
            }
            curr = curr->ai_next;
        }
        if(curr == NULL){
            printf("No Connection\n");
            return -1;
        }

        /*Turn on windowing*/
        if(!(optMask &NOWNDW)){
            start_windowing();
        }

        /*Send and recieve loop*/
        while(!(has_hit_eof())){
            if(DEBUG){
                perror("Error 1");
                fprint_to_output("(client)Polling\n");
            }
            /*Poll for changes in stdin or socket*/
            poll(fds,2,-1);
            /*If stdin has changed*/
            if((fds[STDIN_FD].revents & POLLIN)){
                if(DEBUG){
                    perror("Error 2");
                    fprint_to_output("(client)User input detected\n");
                }
                /*Update buffer from stdin*/
                update_input_buffer();
                /*If end of line, send to server*/
                if(has_whole_line()){
                    /*Clear the buffer*/
                    memset(inBuf,0, LINE_LENGTH);
                    /*Read from the buffer*/
                    if((-1 == read_from_input(inBuf, LINE_LENGTH))){
                        stop_windowing();
                        perror("(client)Read from Terminal");
                        exit(EXIT_FAILURE);
                    }
                    if(DEBUG){
                        fprint_to_output("(client)Sending...\n");
                    }
                    /*Send packet*/
                    send(sock, inBuf, LINE_LENGTH, 0);
                }
                if(DEBUG){
                    perror("Error 3");
                }
            }
            /*If incoming message*/
            if(fds[SOCK_FD].revents & POLLIN){
                if(DEBUG){
                    fprint_to_output("(client)Incoming message detected\n");
                    fprint_to_output("Recieving on sock %d inbuf %s", sock);
                }
                /*Clear buffer*/
                memset(inBuf,0, LINE_LENGTH);
                /*Read from socket*/
                if(0 < (numRead = recv(sock, inBuf, LINE_LENGTH, 0))){
                    if(DEBUG){
                        fprint_to_output("(client)Recieved message of length %d\n", numRead);
                        fprint_to_output("%s\n", inBuf);
                    }
                    /*Write to screen*/
                    if(-1 == write_to_output(inBuf, numRead)){
                        stop_windowing();
                        perror("(client)Write to buffer");
                        exit(EXIT_FAILURE);
                    }
                }else{
                    stop_windowing();
                    printf("Sock = %d", sock);
                    perror("recieve:");
                    exit(EXIT_FAILURE);
                }
            }
        }
        if(DEBUG){
            printf("Closing\n");
        }
        /*Stop windowing*/
        if(!(optMask &NOWNDW)){
            stop_windowing();
        }
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
        /*Set the socket for polling*/
        fds[SOCK_FD].fd = sock;

        /*Attach address*/
        sa.sin_family = AF_INET;
        port = strtol(argv[1], &end, 10);
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = htonl(INADDR_ANY);

        /*Bind the socket to an address*/
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
        if(DEBUG){
            printf("Accepting...\n");
        }
        len = sizeof(sa);
        accept(sock, (struct sockaddr*)&sa, &len);

        /*Start windowing*/
        if(!(optMask &NOWNDW)){
            start_windowing();
        }

        /*Send and recieve loop*/
        while(!(has_hit_eof())){
            if(DEBUG){
                printf("Polling...\n");
            }
            /*Poll socket and stdin*/
            poll(fds,2,-1);

            /*If user types*/
            if(fds[STDIN_FD].revents & POLLIN){
                if(DEBUG){
                    printf("From user\n");
                }
                /*Update buffer*/
                update_input_buffer();
                /*If whole line, write message*/
                if(has_whole_line()){
                    /*Read the line from the buffer*/
                    if(-1 == read_from_input(inBuf, LINE_LENGTH)){
                        perror("Read from Terminal");
                        exit(EXIT_FAILURE);
                    }
                    /*Send the read line*/
                    send(sock, inBuf, LINE_LENGTH, 0);
                }
            }
            /*If there is change in the socket*/
            if(fds[SOCK_FD].revents & POLLIN){
                if(DEBUG){
                    printf("From Connection\n");
                }
                /*Read from the socket*/
                if((numRead = recv(sock, inBuf, LINE_LENGTH, 0))){
                    /*Write the line to output*/
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
        /*Stop Windowing*/
        if(!(optMask &NOWNDW)){
            stop_windowing();
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