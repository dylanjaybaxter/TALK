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
#include <pwd.h>
#include "talk.h"

/*Option mask constants*/
#define VERBOSE 0x01
#define ACCEPT 0x02
#define NOWNDW 0x04
#define DEFAULT_BACKLOG 100

/*Others*/
#define LINE_LENGTH 8056
#define LOCAL 0
#define REMOTE 1
#define NUM_FDS 2
#define PORT_MAX 65535
#define PORT_MIN 1

/*Prototypes*/
void init_hint(struct addrinfo* hint);
void clearStdin(void);

int main(int argc, char* const argv[]) {
    /*Parse options*/
    /*Iterate through options */
    uint8_t optMask = 0;
    int opt;
    int sock;
    int lsock;
    int argRemain;
    int port;
    int i, c;
    char* end;
    socklen_t len;
    struct addrinfo* infoptr;
    struct addrinfo* curr;
    struct addrinfo hint;
    struct passwd *pwd;
    char answer[4];
    char hbuf[NI_MAXHOST];
    struct sockaddr_in sa, lsinfo, sinfo;

    /*char locAdd[INET_ADDRSTRLEN], remAdd[INET_ADDRSTRLEN];*/
    init_hint(&hint);
    hint.ai_family = AF_INET;

    char inBuf[LINE_LENGTH];
    int numRead = 0;

    /*Polling*/
    struct pollfd fds[2];
    /*Set stdin poll*/
    fds[LOCAL].fd = 0;
    fds[LOCAL].events = POLLIN;
    fds[LOCAL].revents = 0;

    /*Set socket poll*/
    fds[REMOTE].events = POLLIN;
    fds[REMOTE].revents = 0;

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

    /*Compute Remaining arguments*/
    argRemain = argc-optind;
    if(optMask & VERBOSE){
        printf("argRemain: %d\n", argRemain);
        printf("optMask: %d\n", optMask);
    }


    /*-----------------If hostname is included, act as client----------------*/
    if(argRemain == 2){
        if(optMask & VERBOSE){
            printf("Acting as client\n");
        }

        /*Parse and validate port*/
        port = strtol(argv[argc-1], &end, 10);
        if((*end != '\0')){
            printf("Port must be a valid positive integer\n");
            printf("Server usage: ./mytalk -vaN <hostname> <port>\n");
            exit(EXIT_FAILURE);
        }
        if((port < PORT_MIN) || (port > PORT_MAX)){
            printf("Invalid port.  Must be in range[%d %d]\n",
                PORT_MIN, PORT_MAX);
            printf("Server usage: ./mytalk -vaN <hostname> <port>\n");
            exit(EXIT_FAILURE);
        }

        /*Look up peer address*/
        if( -1 == getaddrinfo(argv[optind], argv[argc-1], &hint, &infoptr)){
            return -1;
        }
        /*Create socket and connect to server*/
        curr = infoptr;
        if(-1 == (sock = socket(AF_INET, SOCK_STREAM, 0))){
            perror("Socket Creation");
            exit(EXIT_FAILURE);
        }

        while(curr != NULL){
            if(optMask & VERBOSE){
                printf("Connecting socket\n");
            }
            if(-1 != connect(sock,curr->ai_addr, curr->ai_addrlen)){
                fds[REMOTE].fd = sock;
                printf("Connection Established\n");
                break;
            }else{
                close(sock);
            }
            curr = curr->ai_next;
        }
        if(curr == NULL){
            printf("Cannot connect to %s\n", argv[argc-2]);
            return -1;
        }
        if(optMask & VERBOSE){
            printf("Waiting for response from %s\n", argv[argc-2]);
        }
        pwd = getpwuid(getuid());
        if(-1 == send(sock, pwd->pw_name, strlen(pwd->pw_name), 0)){
            perror("Send uid");
            exit(EXIT_FAILURE);
        }
        if(0 < (numRead = recv(sock, inBuf, LINE_LENGTH-1, 0))){
            inBuf[numRead] = '\0';
        }
        if(strcmp("ok", inBuf)){
            printf("%s declined connection with %s\n", argv[argc-2],inBuf);
            exit(EXIT_FAILURE);
        }
        else{
            printf("Host Responded %s\n",inBuf);
        }

        /*Turn on windowing*/
        if(!(optMask &NOWNDW)){
            start_windowing();
        }

        /*Send and recieve loop*/
        fds[LOCAL].revents = 0;
        fds[REMOTE].revents = 0;
        while(1){
            if(optMask & VERBOSE){
                fprint_to_output("Polling...\n");
            }
            /*Poll for changes in stdin or socket*/
            if((-1 == poll(fds,NUM_FDS,-1))){
                perror("Polling");
                exit(EXIT_FAILURE);
            }

            /*If stdin has changed*/
            if((fds[LOCAL].revents & POLLIN)){
                if(optMask & VERBOSE){
                    fprint_to_output("User input detected\n");
                }
                /*Update buffer from stdin*/
                update_input_buffer();

                /*Check for EOF*/
                if(has_hit_eof()){
                    /*Stop windowing*/
                    if(!(optMask &NOWNDW)){
                        stop_windowing();
                    }
                    /*close sockets*/
                    close(sock);
                    return 0;
                }

                /*If end of line, send to server*/
                if(has_whole_line()){
                    /*Clear the buffer*/
                    memset(inBuf,'\0', LINE_LENGTH);
                    /*Read from the buffer*/
                    if((-1 ==(numRead = read_from_input(inBuf, LINE_LENGTH)))){
                        stop_windowing();
                        perror("Read from Terminal");
                        exit(EXIT_FAILURE);
                    }
                    inBuf[numRead] = '\0';
                    if(optMask & VERBOSE){
                        fprint_to_output("Sending...\n");
                    }
                    /*Send packet*/
                    if(-1 == send(sock, inBuf, numRead, 0)){
                        stop_windowing();
                        perror("Sending");
                        exit(EXIT_FAILURE);
                    }
                }

                fds[LOCAL].revents = 0;
            }
            /*If incoming message*/
            if(fds[REMOTE].revents & POLLIN){
                if(optMask & VERBOSE){
                    fprint_to_output("Incoming message detected\n");
                    fprint_to_output("Recieving on sock %d inbuf %s", sock);
                }
                /*Clear buffer*/
                memset(inBuf,'\0', LINE_LENGTH);

                /*Read from socket*/
                if(0 < (numRead = recv(sock, inBuf, LINE_LENGTH-1, 0))){
                    inBuf[numRead] = '\0';
                    if(optMask & VERBOSE){
                        fprint_to_output("Recieved message of length %d\n",
                         numRead);
                    }
                    /*Write to screen*/
                    if(-1 == write_to_output(inBuf, numRead)){
                        stop_windowing();
                        perror("Write to buffer");
                        exit(EXIT_FAILURE);
                    }
                }
                else if(numRead == 0){
                    break;
                }
                else{
                    stop_windowing();
                    printf("Sock = %d", sock);
                    perror("recieve:");
                    exit(EXIT_FAILURE);
                }
                fds[REMOTE].revents = 0;
            }
        }

        fprint_to_output("\n Connection Closed. ^C to terminate\n");
        while((c = getchar()) != -1){
            /*Do nothing*/
            putchar(c);
        }
        /*Stop windowing*/
        if(!(optMask &NOWNDW)){
            stop_windowing();
        }
        /*close sockets*/
        close(sock);
    }

    /*--------------------If not, act as server-----------------------------*/
    else if(argRemain == 1){
        if(optMask & VERBOSE){
            printf("Acting as server\n");
        }
        /*Create a socket*/
        if(-1==(lsock = socket(AF_INET, SOCK_STREAM, 0))){
            perror("Server Socket");
            exit(EXIT_FAILURE);
        }

        /*Attach address*/
        sa.sin_family = AF_INET;

        /*Parse and validate port*/
        port = strtol(argv[argc-1], &end, 10);
        if((*end != '\0')){
            printf("Port must be a valid positive integer\n");
            printf("Server usage: ./mytalk -vaN <hostname> <port>\n");
            exit(EXIT_FAILURE);
        }
        if((port < PORT_MIN) || (port > PORT_MAX)){
            printf("Invalid port.  Must be in range[%d %d]\n",
                PORT_MIN, PORT_MAX);
            printf("Server usage: ./mytalk -vaN <hostname> <port>\n");
            exit(EXIT_FAILURE);
        }
        /*Attach address cont.*/
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = htonl(INADDR_ANY);

        /*Bind the socket to an address*/
        if(optMask & VERBOSE){
            printf("Binding...\n");
        }
        if((-1 == bind(lsock, (struct sockaddr*)&sa, sizeof(sa)))){
            perror("Binding");
            exit(EXIT_FAILURE);
        }

        /*Wait for Connection*/
        if(optMask & VERBOSE){
            printf("Listening...\n");
        }
        if(-1==listen(lsock, DEFAULT_BACKLOG)){
            perror("listening");
            exit(EXIT_FAILURE);
        }

        /*Wait for a worthy client*/
        while(1){
            /*Accept connection*/
            if(optMask & VERBOSE){
                printf("Accepting...\n");
            }
            len = sizeof(sa);
            if(-1 == (sock = accept(lsock, (struct sockaddr*)&lsinfo, &len))){
                perror("Accept");
                exit(EXIT_FAILURE);
            }

            /*Gather info*/
            len = sizeof(sinfo);
            getsockname(sock, (struct sockaddr*)&sinfo, &len);
            if(-1 == getnameinfo((struct sockaddr*)&sinfo, len,
                                    hbuf, sizeof(hbuf), NULL, 0, 0)){
                perror("Getnameinfo");
                exit(EXIT_FAILURE);
            }

            /*Check username and response*/
            if(0 < (numRead = recv(sock, inBuf, LINE_LENGTH-1, 0))){
                inBuf[numRead] = '\0';
            }
            if(!(optMask & ACCEPT)){
                printf("Mytalk request from %s@%s. Accept (y/n)?\n",
                inBuf, hbuf);
                memset(answer, '\0', 4);
                i=0;
                while((c = getchar())!= '\n'){
                    if(i<3){
                        answer[i]=c;
                        i++;
                    }
                }
                if(optMask & VERBOSE){
                    printf("Answer entered is %s\n", answer);
                }
                if(!(strcmp("yes", answer)) || !(strcmp("y", answer))){
                    strcpy(answer, "ok\0");
                    if(-1 == send(sock, answer, 3, 0)){
                        perror("Send Response");
                        exit(EXIT_FAILURE);
                    }
                    break;
                }
                else{
                    strcpy(answer, "no\0");
                    if(-1 == send(sock, answer, 3, 0)){
                        perror("Send Response");
                        exit(EXIT_FAILURE);
                    }
                    close(sock);
                }
            }
            else{
                if(optMask & VERBOSE){
                    printf("Auto-accepting Request from %s@%s\n", inBuf, hbuf);
                }
                strcpy(answer, "ok\0");
                if(-1 == send(sock, answer, 3, 0)){
                    perror("Send Response");
                    exit(EXIT_FAILURE);
                }
                break;
            }
        }

        /*Set the socket for polling*/
        fds[REMOTE].fd = sock;

        /*Start windowing*/
        if(!(optMask &NOWNDW)){
            start_windowing();
        }

        /*Send and recieve loop*/
        while(1){
            if(optMask & VERBOSE){
                fprint_to_output("Polling...\n");
            }
            /*Poll socket and stdin*/
            if((-1 == poll(fds,2,-1))){
                perror("Polling");
                exit(EXIT_FAILURE);
            }

            /*If user types*/
            if(fds[LOCAL].revents & POLLIN){
                if(optMask & VERBOSE){
                    fprint_to_output("User Input\n");
                }
                /*Update buffer*/
                update_input_buffer();

                /*Check for EOF*/
                if(has_hit_eof()){
                    /*Stop Windowing*/
                    if(!(optMask &NOWNDW)){
                        stop_windowing();
                    }
                    /*Close sockets*/
                    close(sock);
                    close(lsock);
                    return 0;
                }

                /*If whole line, write message*/
                if(has_whole_line()){
                    /*Read the line from the buffer*/
                    if(-1 == (numRead = read_from_input(inBuf, LINE_LENGTH))){
                        inBuf[numRead] = '\0';
                        perror("Read from Terminal");
                        exit(EXIT_FAILURE);
                    }
                    /*Send the read line*/
                    if(-1 == send(sock, inBuf, numRead, 0)){
                        perror("Send");
                        exit(EXIT_FAILURE);
                    }
                }
            }
            /*If there is change in the socket*/
            if(fds[REMOTE].revents & POLLIN){
                if(optMask & VERBOSE){
                    fprint_to_output("From Connection\n");
                }
                /*Read from the socket*/
                if(0 < (numRead = recv(sock, inBuf, LINE_LENGTH-1, 0))){
                    inBuf[numRead] = '\0';
                    /*Write the line to output*/
                    if(-1 == write_to_output(inBuf, numRead)){
                        perror("Write to buffer");
                        exit(EXIT_FAILURE);
                    }
                }
                else if(numRead == 0){
                    break;
                }
                else{
                    stop_windowing();
                    printf("Current sock %d\n", sock);
                    perror("Recieve");
                    exit(EXIT_FAILURE);
                }
            }
        }

        fprint_to_output("\n Connection Closed. ^C to terminate\n");
        while((c = getchar()) != -1){
            /*Do nothing*/
            putchar(c);
        }
        /*Stop Windowing*/
        if(!(optMask &NOWNDW)){
            stop_windowing();
        }

        /*Close sockets*/
        close(sock);
        close(lsock);
    }
    else{
        printf("You Never should have come here\n");
        /*Error!*/
    }
    return 0;
}

void clearStdin(void){
    int c;
    while((c = getchar()) != '\n'){
        /*Do nothing*/
    }
}

 void init_hint(struct addrinfo* hint){
    hint->ai_flags = 0;
    hint->ai_family = AF_INET;
    hint->ai_socktype = SOCK_STREAM;
    hint->ai_protocol = 0;
    hint->ai_addrlen = 0;
    hint->ai_addr = NULL;
    hint->ai_canonname = NULL;
    hint->ai_next = NULL;
}