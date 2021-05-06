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
#include <curses.h>
#include <locale.h>
#include <sys/types.h>  
#include <ctype.h>
#include <string.h>
#define PROTOPORT 36711 /* default port number */
#define QLEN 6 /* size of request queue */

//for commands/arguments
char type[10] = ""; //Type of connection (tail,head,middle)
char outputDir[10];
char displayDir[10];
char src[100];

//For reading input
int cmdLen;
char cmd[100];
char prevCh;
bool insertMode = false;

char* getOptCmd(char* cmd, char* baseCmd) {
    char* optCmd = strtok(cmd, " =");
    while(optCmd != NULL) {
        if(strcmp(optCmd,baseCmd) != 0) {
            return optCmd;
        }
        optCmd = strtok(NULL, " =");
    }
    return NULL;
}
void proccessCmd(char cmd[]) {
    //quit
    if((cmd[0] == 'q') && (strlen(cmd) == 1)) {
        nocbreak();
        endwin();
        exit(0);
    //src
    } else if(strstr(cmd,"src")) {
        printf("Opt cmd = %s\r\n",getOptCmd(cmd, "src"));
    //output cmds
    } else if(strcmp(cmd,"outl") == 0) {
        if(strcmp(type, "tail") == 0|| strcmp(type, "middle")== 0)
            strcpy(outputDir, "left");
        else 
            printf("Error: Can't change output direction to left, connection is head\r\n");
    } else if(strcmp(cmd,"outr" )== 0) {
        if(strcmp(type, "head") == 0|| strcmp(type, "middle") == 0) {
            strcpy(outputDir, "right");
        } else {
            printf("Error: Can't change output direction to right, connection is tail\r\n");
        }
    } else if(strcmp(cmd,"out") == 0) {
        printf("Output direction is %s\r\n", outputDir);
    //Error
    } else {
        printf("Error: Command not recognised\r\n");
    }
    refresh();
}

void readInput(char currentCh, int leftSd, int rightSd) {
        bool modeChange = false;
        //show space correctly
        if(currentCh == 10) {
            printf("\r\n");
        } 
        //backspace
        if (currentCh == 7 || currentCh == 127 || currentCh == 263) {
            int x,y;
            getyx(stdscr,y,x);
            if(!(x == 0)) {
                move(y,x-1);
                delch();
                cmd[cmdLen] = '\0';
                cmdLen--;
            }
        }
        
        //prints slash without any meta
        if(currentCh == '\\' && prevCh != '\\') {
            addch(currentCh);
            refresh();
        //ignores
        } else if (currentCh == 7 || currentCh == 127 || currentCh > 255)  {
        //exits insert mode
        } else if(insertMode) {
            if(currentCh == 27 && prevCh != '\\') {
                modeChange = true;
                insertMode = false;
                cmd[0] = '\0';
                cmdLen = 0;
                printf("\r\nEntering command mode:\r\n");
            //sends data
            } else {
                addch(currentCh);
                if(strcmp(outputDir, "left") == 0) {
                    send(leftSd, &currentCh, 1, 0);
                } else {
                    send(rightSd, &currentCh, 1, 0);
                }
                refresh();
            }
        // command mode
        } else {
            //enters insert mode
            if(currentCh == 'i' && prevCh != '\\') {
                modeChange = true;
                insertMode = true;
                printf("\r\nEntering insert mode:\r\n");
            // quits
            /*} else if (currentCh == 'q' && prevCh != '\\') {
                nocbreak();
                endwin();
                exit(0);*/
            //proccess word/command on enter
            } else if (currentCh == 10) {
                cmd[cmdLen] = '\0';
                printf("cmd = %s of length %d\r\n", cmd, cmdLen);
                proccessCmd(cmd);
                cmd[0] = '\0';
                cmdLen = 0;
            //adds char to current comman
            } else {
                addch(currentCh);
                cmd[cmdLen] = currentCh;
                cmdLen++;
                refresh();
            }
        }
        //update prevCH
        if(!modeChange)
            prevCh = currentCh;
        else
            prevCh = '\0';
        refresh();
}

//Finds max of 2 given values and returns
int max(int x, int y) {
  return (x > y) ? x : y;
}

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

void createConnection(char raddr[], int lport, int rport) {
        struct sockaddr_in cad; /* structure to hold client's address */
        int sd, sd2, sd3; /* socket descriptors */
        int n; /* number of characters read */
        int alen; /* length of address */
        char buf[1000]; /* buffer for string the server sends */
        fd_set rfds;
        int retval; 
        #ifdef WIN32
        WSADATA wsaData;
        WSAStartup(0x0101, & wsaData);
        #endif
        
        setlocale(LC_ALL,"");  
        initscr();  
        cbreak();
        nl();
        noecho();
        intrflush(stdscr, FALSE);   
        keypad(stdscr, TRUE); 
        clear();
        
        //if head type
        if ((strcmp(type, "head") == 0)) {
                sd = clientSocket(rport, raddr);
                while(1) {
                    FD_ZERO(&rfds);
                    FD_SET(STDIN_FILENO, &rfds);
                    FD_SET(sd, &rfds);
                    retval = select (sd+1, &rfds, NULL, NULL, NULL);
                    if (retval == -1) {
    	               printf("Error: Select error\n");
    	            } else if (FD_ISSET(STDIN_FILENO, &rfds)) {
    	               readInput(getch(), 0, sd);
    	            } else if (FD_ISSET(sd, &rfds)) {
    	               n = recv(sd, buf, sizeof(buf), 0);
                       if(buf[0] == 10) {
                           printf("\r\n");
                       } else {
                           write(1, buf, n);
                       }
    	            }
                }
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
                        while(1) {
                            FD_ZERO(&rfds);
                            FD_SET(STDIN_FILENO, &rfds);
                            FD_SET(sd2, &rfds);
                            retval = select (sd2+1, &rfds, NULL, NULL, NULL);
                            if (retval == -1) {
    	                       printf("Error: Select error\n");
    	                    } else if (FD_ISSET(STDIN_FILENO, &rfds)) {
                                readInput(getch(), sd2, 0);
    	                    } else if (FD_ISSET(sd2, &rfds)) {
    	                        n = recv(sd2, buf, sizeof(buf), 0);
                                if(buf[0] == 10) {
                                    printf("\r\n");
                                } else {
                                    write(1, buf, n);
                                }
    	                    }
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
                        while(1) {
                            FD_ZERO(&rfds);
                            FD_SET(sd3, &rfds);
                            FD_SET(sd2, &rfds);
                            FD_SET(STDIN_FILENO, &rfds);
                            retval = select(max(sd3,sd2)+1, &rfds, NULL, NULL, NULL);
                            if (retval == -1) {
    	                       printf("Error: Select error\n");
                            } else if (FD_ISSET(STDIN_FILENO, &rfds)) {
                                readInput(getch(), sd2, sd3);
    	                    } else if (FD_ISSET(sd3, &rfds)) {
    	                        n = recv(sd3, buf, sizeof(buf), 0);
                                if(buf[0] == 10) {
                                    printf("\r\n");
                                } else {
                                    write(1, buf, n);
                                }
                                send(sd2, buf, n, 0);
    	                    } else if (FD_ISSET(sd2, &rfds)) {
    	                        n = recv(sd2, buf, sizeof(buf), 0);
                                if(buf[0] == 10) {
                                    printf("\r\n");
                                } else {
                                    write(1, buf, n);
                                }
                                send(sd3, buf, n, 0);
    	                    }
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
                {"src", optional_argument, NULL, 5},
                {"prsl", no_argument, NULL, 6},
                {"prsr", no_argument, NULL, 7},
                {"dsplr", no_argument, NULL, 8},
                {"dsprl", no_argument, NULL, 9},
                {NULL , 0, NULL, 0}
        };
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
                        strcpy(outputDir, "right");
                        break;
                case 1:
                        if (type[0] != '\0') {
                                printf("Error: Both -head and -tail roles requested\n");
                                exit(EXIT_FAILURE);
                        }
                        strcpy(type, "tail");
                        strcpy(outputDir, "left");
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
                case 5:
                    if(optarg)
                        strcpy(src, optarg); //This only works with = -- is this fixable?
                    else 
                        strcpy(src, "scriptin");
                    printf("src with %s\n", src);
                    break;
                case 6:
                    printf("left side persistant\n");
                    break;
                case 7:
                    printf("right side persistant\n");
                    break;
                case 8:
                    strcpy(displayDir, "right");
                    printf("%s side display\n", displayDir);
                    break;
                case 9:
                    strcpy(displayDir, "left");
                    printf("%s side display\n", displayDir);
                    break;
                default:
                        exit(EXIT_FAILURE);
                }
        }
        if (type[0] == '\0') {
                strcpy(type, "middle");
                strcpy(outputDir, "right");
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
        createConnection(raddr, lport, rport);
}





