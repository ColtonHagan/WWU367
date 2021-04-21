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
char localhost[] = "140.160.30.109";
int main(int argc, char * argv[]) {
        static struct option args[] = {
                {
                        "head",
                        no_argument,
                        NULL,
                        0
                },
                {
                        "tail",
                        no_argument,
                        NULL,
                        1
                },
                {
                        "rraddr",
                        required_argument,
                        NULL,
                        2
                },
                {
                        "rrport",
                        required_argument,
                        NULL,
                        3
                },
                {
                        "llport",
                        required_argument,
                        NULL,
                        4
                },
                {
                        NULL,
                        0,
                        NULL,
                        0
                }
        };
        char type[10] = "";
        char raddr[1000] = "";
        int lport = 0;
        int rport = 0;
        int opt = 0;
        while ((opt = getopt_long_only(argc, argv, "", args, NULL)) != -1) {
                switch (opt) {
                case 0:
                        if (type[0] != '\0') {
                                printf("Error: Both -head and -tail roles requested");
                                exit(EXIT_FAILURE);
                        }
                        strcpy(type, "head");
                        break;
                case 1:
                        if (type[0] != '\0') {
                                printf("Error: Both -head and -tail roles requested");
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
                                printf("Error: rrport address out of range \"value\"\n");
                                exit(EXIT_FAILURE);
                        }
                        break;
                case 4:
                        lport = atoi(optarg);
                        if (lport < 1 || lport > 65535) {
                                printf("Error: llport address out of range \"value\"\n");
                                exit(EXIT_FAILURE);
                        }
                        break;
                default:
                        exit(1);
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

        printf("type = %s\n", type);
        printf("radr = %s\n", raddr);
        printf("rport = %d\n", rport);
        printf("lport = %d\n", lport);

        if ((strcmp(type, "head") == 0)) {
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
                /* Check host argument and assign host name. */
		port = PROTOPORT;
		sad.sin_port = htons((u_short)port);
                host = localhost;
                /* Convert host name to equivalent IP address and copy to sad. */
                ptrh = gethostbyname(host);
                if (((char * ) ptrh) == NULL) {
                        fprintf(stderr, "invalid host: %s\n", host);
                        exit(EXIT_FAILURE);
                }
                memcpy(&sad.sin_addr, ptrh -> h_addr, ptrh -> h_length);
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
                if (connect(sd, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
                        fprintf(stderr, "connect failed\n");
                        exit(EXIT_FAILURE);
                }
                //sprintf(buf,"Say hi!\n");
                //send(sd,buf,strlen(buf),0);
		char buffer[1000];
    		while((n = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
        		write(1,buffer,n);
			send(sd,buffer,strlen(buffer),0);
    		}
                /* Close the socket. */
                closesocket(sd);
                /* Terminate the client program gracefully. */
                exit(0);
        } else if ((strcmp(type, "tail") == 0)) {
                struct hostent * ptrh; /* pointer to a host table entry */
                struct protoent * ptrp; /* pointer to a protocol table entry */
                struct sockaddr_in sad; /* structure to hold server's address */
                struct sockaddr_in cad; /* structure to hold client's address */
                int sd, sd2; /* socket descriptors */
                int n;
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
                port = PROTOPORT;
		sad.sin_port = htons((u_short)port);
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
                /*Eliminate "Address already in use" error message.*/
                int flag = 1;
                /*if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, & flag, sizeof(int)) == -1) {
                        fprintf(stderr, "set sock opt error\n");
                        exit(1);
                }*/
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
                        /* Prints out data --untested */
                        while ((n = recv(sd2, buf, sizeof(buf), 0)) > 0) {
            			write(1, buf, n);
			}
                        closesocket(sd2);
                }
        }
}
