#ifndef unix
#define WIN32#include <windows.h>
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
#define PROTOPORT 36711 /* default port number */
#define QLEN 6 /* size of request queue */
int clientSocket(int port, char * host) {
        int sd; /* socket descriptor */
        struct hostent * ptrh; /* pointer to a host table entry */
        struct protoent * ptrp; /* pointer to a protocol table entry */
        struct sockaddr_in sad; /* structure to hold an IP address */
        memset((char * ) & sad, 0, sizeof(sad)); /* clear sockaddr structure */
        sad.sin_family = AF_INET; /* set family to Internet */
        sad.sin_port = htons((u_short) port);
        /* Convert host name to equivalent IP address and copy to sad. */
        ptrh = gethostbyname(host);
        if (((char * ) ptrh) == NULL) {
                fprintf(stderr, "Error: invalid host: %s\n", host);
                exit(EXIT_FAILURE);
        }
        memcpy( & sad.sin_addr, ptrh -> h_addr, ptrh -> h_length);
        /* Map TCP transport protocol name to protocol number. */
        if (((long int)(ptrp = getprotobyname("tcp"))) == 0) {
                fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
                exit(EXIT_FAILURE);
        }
        /* Create a socket. */
        sd = socket(PF_INET, SOCK_STREAM, ptrp -> p_proto);
        if (sd < 0) {
                fprintf(stderr, "Error: Socket creation failed\n");
                exit(EXIT_FAILURE);
        }
        /* Connect the socket to the specified server. */
        if (connect(sd, (struct sockaddr * ) & sad, sizeof(sad)) < 0) {
                fprintf(stderr, "Error: Failed to make connection to %s:%d\n",
                        host, port);
                exit(EXIT_FAILURE);
        }
        return sd;
}

int serverSocket(int port) {
        int sd; /* socket descriptor */
        struct hostent * ptrh; /* pointer to a host table entry */
        struct protoent * ptrp; /* pointer to a protocol table entry */
        struct sockaddr_in sad; /* structure to hold an IP address */
        memset((char * ) & sad, 0, sizeof(sad)); /* clear sockaddr structure */
        sad.sin_family = AF_INET; /* set family to Internet */
        sad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */
        sad.sin_port = htons((u_short) port);
        /* Map TCP transport protocol name to protocol number */
        if (((long int)(ptrp = getprotobyname("tcp"))) == 0) {
                fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
                exit(EXIT_FAILURE);
        }
        /* Create a socket */
        sd = socket(PF_INET, SOCK_STREAM, ptrp -> p_proto);
        if (sd < 0) {
                fprintf(stderr, "Error: Socket creation failed\n");
                exit(EXIT_FAILURE);
        }
        /*Eliminate "Address already in use" error message. */
        int flag = 1;
        if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, & flag, sizeof(int)) == -1) {
                fprintf(stderr, "Error: Set sock opt failed\n");
                exit(1);
        }
        /* Bind a local address to the socket */
        if (bind(sd, (struct sockaddr * ) & sad, sizeof(sad)) < 0) {
                fprintf(stderr, "Error: Bind failed\n");
                exit(EXIT_FAILURE);
        }
        /* Specify size of request queue */
        if (listen(sd, QLEN) < 0) {
                fprintf(stderr, "Error: Listen failed\n");
                exit(EXIT_FAILURE);
        }
        return sd;
}

void createConnection(char type[], char raddr[], int lport, int rport) {
        struct sockaddr_in cad; /* structure to hold client's address */
        int sd, sd2, sd3; /* socket descriptors */
        int n; /* number of characters read */
        int alen; /* length of address */
        char buf[1000]; /* buffer for string the server sends */
        #ifdef WIN32
        WSADATA wsaData;
        WSAStartup(0x0101, & wsaData);
        #endif
        //if head type
        if ((strcmp(type, "head") == 0)) {
                sd = clientSocket(rport, raddr);
                //Reads stdin, writes, and sends it
                while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
                        write(1, buf, n);
                        send(sd, buf, n, 0);
                }
                /* Closes and exits */
                closesocket(sd);
                exit(0);
                //if tail type
        } else if ((strcmp(type, "tail") == 0)) {
                sd = serverSocket(lport);
                /* accepts and handles requests */
                while (1) {
                        alen = sizeof(cad);
                        if ((sd2 = accept(sd, (struct sockaddr * ) & cad, & alen)) < 0) {
                                fprintf(stderr, "Error: Accept failed\n");
                                exit(EXIT_FAILURE);
                        }
                        //Recieves and writes
                        while ((n = recv(sd2, buf, sizeof(buf), 0)) > 0) {
                                write(1, buf, n);
                        }
                        closesocket(sd2);
                }
                /* Closes and exits */
                closesocket(sd);
                exit(0);
                //If middle type
        } else if (strcmp(type, "middle") == 0) {
                sd = serverSocket(lport);
                sd3 = clientSocket(rport, raddr);
                /* accept and handle requests */
                while (1) {
                        alen = sizeof(cad);
                        if ((sd2 = accept(sd, (struct sockaddr * ) & cad, & alen)) < 0) {
                                fprintf(stderr, "Error: Accept failed\n");
                                exit(EXIT_FAILURE);
                        }
                        //Recieves, writes, and sends
                        while ((n = recv(sd2, buf, sizeof(buf), 0)) > 0) {
                                write(1, buf, n);
                                send(sd3, buf, n, 0);
                        }
                        closesocket(sd2);
                }
                /* Closes and exits */
                closesocket(sd);
                closesocket(sd3);
                exit(0);
        }
}

int main(int argc, char * argv[]) {
        static struct option args[] = {
                {"head", no_argument, NULL, 0},
                {"tail", no_argument, NULL, 1},
                {"rraddr", required_argument, NULL, 2},
                {"rrport", required_argument, NULL, 3},
                {"llport", required_argument, NULL, 4},
                {NULL , 0, NULL, 0}
        };
        char type[10] = ""; //Type of connection (tail,head,middle)
        char raddr[1000] = ""; //rraddr -- ip address
        int lport = PROTOPORT; //llport
        int rport = PROTOPORT; //rrport
        int opt = 0; //Arg opt

        //Reads in arguments
        while ((opt = getopt_long_only(argc, argv, "", args, NULL)) != -1) {
                switch (opt) {
                case 0:
                        if (type[0] != '\0') {
                                printf("Error: Both -head and -tail roles requested\n");
                                exit(EXIT_FAILURE);
                        }
                        strcpy(type, "head");
                        break;
                case 1:
                        if (type[0] != '\0') {
                                printf("Error: Both -head and -tail roles requested\n");
                                exit(EXIT_FAILURE);
                        }
                        strcpy(type, "tail");
                        break;
                case 2:
                        strcpy(raddr, optarg);
                        break;
                case 3:
                        rport = atoi(optarg);
                        if (rport < 1 || rport > 65535) {
                                printf("Error: rrport address out of range %d\n", rport);
                                exit(EXIT_FAILURE);
                        }
                        break;
                case 4:
                        lport = atoi(optarg);
                        if (lport < 1 || lport > 65535) {
                                printf("Error: llport address out of range %d\n", lport);
                                exit(EXIT_FAILURE);
                        }
                        break;
                default:
                        exit(EXIT_FAILURE);
                }
        }
        if (type[0] == '\0') {
                strcpy(type, "middle");
        }

        if (raddr[0] == '\0' && (strcmp(type, "middle") == 0)) {
                printf("Error: Middle role requested -rraddr required\n");
                exit(EXIT_FAILURE);
        }

        if (raddr[0] == '\0' && (strcmp(type, "head") == 0)) {
                printf("Error: Head role requested -rraddr required\n");
                exit(EXIT_FAILURE);
        }

        if (raddr[0] == '\0') {
                strcpy(raddr, "localhost");
        }
        createConnection(type, raddr, lport, rport);
}
