#ifndef unix
#define WIN32
#include <windows.h>
#include <winsock.h>
#else
#define closesocket close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#define PROTOPORT 36711 /* default protocol port number */
#define QLEN 6 /* size of request queue */
int visits = 0;
char localhost[] = "local host";
int main(int argc, char * argv[]) {
        strcpy(localhost, argv[2]);
        if ((strcmp(argv[1], "client") == 0)) { //client
                printf("client:\n");
                struct hostent * ptrh; /* pointer to a host table entry */
                struct protoent * ptrp; /* pointer to a protocol table entry */
                struct sockaddr_in sad; /* structure to hold an IP address */
                int sd; /* socket descriptor */
                int port; /* protocol port number */
                char * host; /* pointer to host name */
                int n; /* number of characters read */
                char buf[1000]; /* buffer for data from the server */
                #ifdef WIN32
                WSADATA wsaData;
                WSAStartup(0x0101, & wsaData);
                #endif
                memset((char * ) & sad, 0, sizeof(sad)); /* clear sockaddr structure */
                sad.sin_family = AF_INET; /* set family to Internet */
                port = PROTOPORT;
                sad.sin_port = htons((u_short)port);
                host = localhost;
                printf("port: %d\n", port);
                printf("host: %s\n", host);
                /* Convert host name to equivalent IP address and copy to sad. */
                ptrh = gethostbyname(host);
                if (((char * ) ptrh) == NULL) {
                        fprintf(stderr, "invalid host: %s\n", host);
                        exit(EXIT_FAILURE);
                }
                memcpy( & sad.sin_addr, ptrh -> h_addr, ptrh -> h_length);
                /* Map TCP transport protocol name to protocol number. */
                if (((long int)(ptrp = getprotobyname("tcp"))) == 0) {
                        fprintf(stderr, "cannot map \"tcp\" to protocol number");
                        exit(EXIT_FAILURE);
                }
                /* Create a socket. */
                sd = socket(PF_INET, SOCK_STREAM, ptrp -> p_proto);
                if (sd < 0) {
                        fprintf(stderr, "socket creation failed\n");
                        exit(EXIT_FAILURE);
                }
                /* Connect the socket to the specified server. */
                if (connect(sd, (struct sockaddr * ) & sad, sizeof(sad)) < 0) {
                        fprintf(stderr, "connect failed\n");
                        exit(EXIT_FAILURE);
                }
                /* Repeatedly read data from socket and write to user's screen. */
                n = recv(sd, buf, sizeof(buf), 0);
                while (n > 0) {
                        write(1, buf, n);
                        n = recv(sd, buf, sizeof(buf), 0);
                }
                /* Close the socket. */
                closesocket(sd);
                /* Terminate the client program gracefully. */
                exit(0);
        } else if ((strcmp(argv[1], "server") == 0)) { //server
                printf("server:\n");
                struct hostent * ptrh; /* pointer to a host table entry */
                struct protoent * ptrp; /* pointer to a protocol table entry */
                struct sockaddr_in sad; /* structure to hold server's address */
                struct sockaddr_in cad; /* structure to hold client's address */
                int sd, sd2; /* socket descriptors */
                int port; /* protocol port number */
                int alen; /* length of address */
                char buf[1000]; /* buffer for string the server sends */
                #ifdef WIN32
                WSADATA wsaData;
                WSAStartup(0x0101, & wsaData);
                #endif
                memset((char * ) & sad, 0, sizeof(sad)); /* clear sockaddr structure */
                sad.sin_family = AF_INET; /* set family to Internet */
                sad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */
                port = PROTOPORT; /* use default port number */
                sad.sin_port = htons((u_short) port);
                /* Map TCP transport protocol name to protocol number */
                if (((long int)(ptrp = getprotobyname("tcp"))) == 0) {
                        fprintf(stderr, "cannot map \"tcp\" to protocol number");
                        exit(EXIT_FAILURE);
                }
                /* Create a socket */
                sd = socket(PF_INET, SOCK_STREAM, ptrp -> p_proto);
                if (sd < 0) {
                        fprintf(stderr, "socket creation failed\n");
                        exit(EXIT_FAILURE);
                }
                /* Bind a local address to the socket */
                if (bind(sd, (struct sockaddr * ) & sad, sizeof(sad)) < 0) {
                        fprintf(stderr, "bind failed\n");
                        exit(EXIT_FAILURE);
                }
                /* Specify size of request queue */
                if (listen(sd, QLEN) < 0) {
                        fprintf(stderr, "listen failed\n");
                        exit(EXIT_FAILURE);
                }
                /* Main server loop - accept and handle requests */
                while (1) {
                        alen = sizeof(cad);
                        if ((sd2 = accept(sd, (struct sockaddr * ) & cad, & alen)) < 0) {
                                fprintf(stderr, "accept failed\n");
                                exit(EXIT_FAILURE);
                        }
                        visits++;
                        sprintf(buf, "This server has been contacted %d time%s\n",
                                visits, visits == 1 ? "." : "s.");
                        send(sd2, buf, strlen(buf), 0);
                        closesocket(sd2);
                }
        }
}
