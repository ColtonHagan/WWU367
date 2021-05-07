#ifndef unix
#define WIN32#include <windows.h>
#include <winsock.h>
#else
#define closesocket close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
char outputDir[10]; //Direction of output
char displayDir[10]; //Direction of display
char src[100]; //SRC file
bool lpl = false; //looper left
bool lpr = false; //looper right
int leftSd, rightSd; //left/right sd, 0 if doesn't exist
fd_set rfds; //select set
char localIP[100] = "*", localPort[100] = "*";
char rightPort[100] = "*", rightIP[100] = "*", leftIP[100] = "*", leftPort[100] ="*";
char rightStatus[20] = "DISCONNECTED", leftStatus[20] = "DISCONNECTED";

//For reading input
int cmdLen; //length of commad
char cmd[100]; //command
char prevCh; //prevous char
bool insertMode = false; //if in insert mode

//Gets optional command from given cmd
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
//uses given comman
void proccessCmd(char cmd[]) {
    //quit
    if((cmd[0] == 'q') && (strlen(cmd) == 1)) {
        closesocket(rightSd);
        closesocket(leftSd);
        nocbreak();
        endwin();
        exit(0);
    //src
    } else if(strcmp(cmd,"src") == 0) {
        printf("src no opt cmds\r\n"); // temp
    } else if(strstr(cmd,"src")) {
        char* optCdm = getOptCmd(cmd, "src");
        //incase you have src with extra spaces/enters
        if(optCdm != NULL) {
            printf("Opt cmd = %s\r\n",optCdm); //temp
        }
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
    //display cmds
    } else if(strcmp(cmd,"dsplr") == 0) {
        if(strcmp(type, "head") == 0 || strcmp(type, "middle")== 0)
            strcpy(displayDir, "lr");
        else 
            printf("Error: Can't display right, connection is tail\r\n");
    } else if(strcmp(cmd,"dsprl") == 0) {
        if(strcmp(type, "tail") == 0 || strcmp(type, "middle") == 0) {
            strcpy(displayDir, "rl");
        } else {
            printf("Error: Can't display left, connection is head\r\n");
        }
    } else if(strcmp(cmd,"dsp") == 0) {
        printf("Display = %s\r\n", displayDir);
    //loop cmds
    } else if (strcmp(cmd,"lpl") == 0) {
        lpl = true;
    } else if (strcmp(cmd,"lpl0") == 0) {
        lpl = false;
    } else if (strcmp(cmd,"lpr") == 0) {
        lpr = true;
    } else if (strcmp(cmd,"lpr0") == 0) {
        lpr = false;
    //drop
    } else if (strcmp(cmd,"drpr") == 0) {
        if(rightSd != -1) {
            closesocket(rightSd);
		    FD_CLR(rightSd,&rfds);
		    rightSd = -1;
		    strcpy(rightStatus, "DISCONNECTED");
        } else {
            printf("Error: Right connection doesn't exist\r\n");
        }
    } else if(strcmp(cmd,"drpl") == 0) {
        if(leftSd != -1) {
            closesocket(leftSd);
            FD_CLR(leftSd,&rfds);
		    leftSd = -1;
		    strcpy(leftStatus, "DISCONNECTED");
        } else {
            printf("Error: Left connection doesn't exist\r\n");
        }
    //show connection STDIN_FILENO
    } else if(strcmp(cmd, "rght") == 0) {
        printf("%s:%s:%s:%s\r\n", localIP, localPort, rightIP, rightPort);
        printf("%s\r\n",rightStatus);
    } else if(strcmp(cmd, "lft") == 0) {
        printf("%s:%s:%s:%s\r\n", leftIP, leftPort, localIP, localPort);
        printf("%s\r\n",leftStatus);
    //Not current command
    } else {
        printf("Error: Command not recognised\r\n");
    }
    refresh();
}

void readInput(char currentCh) {
        bool modeChange = false;
        //moves to new start line with enter
        if(currentCh == 10) {
            printf("\r");
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
            refresh();
        }
        //prints slash without any meta
        if(currentCh == '\\' && prevCh != '\\') {
            addch(currentCh);
            refresh();
        //ignores backspace
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
            //proccess word/command on enter
            } else if (currentCh == 10) {
                //prints enter
                addch(currentCh);
                refresh();
                //proccess cmd
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
        //extra refresh to deal with outliers
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
        strcpy(rightStatus, "LISTENING");
        /* Connect the socket to the specified server. */
        if (connect(sd, (struct sockaddr * ) & sad, sizeof(sad)) < 0) {
                fprintf(stderr, "Error: Failed to make connection to %s:%d\n",
                        host, port);
                exit(EXIT_FAILURE);
        }
        strcpy(rightStatus, "CONNECTED");
        //Gets local port
        socklen_t len = sizeof(sad);
        getsockname(sd, (struct sockaddr*)&sad, &len);
        if((int) ntohs(sad.sin_port) > 0) {
            snprintf(localPort, 100, "%d", (int) ntohs(sad.sin_port));
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
        //get local port
       socklen_t len = sizeof(sad);
        getsockname(sd, (struct sockaddr*)&sad, &len);
        if((int) ntohs(sad.sin_port) > 0) {
            snprintf(localPort, 100, "%d", (int) ntohs(sad.sin_port));
        }
        return sd;
}

void createConnection(char raddr[], int lport, int rport) {
        struct sockaddr_in cad; /* structure to hold client's address */
        int sd, sd2, sd3; /* socket descriptors */
        int n; /* number of characters read */
        int alen; /* length of address */
        char buf[1000]; /* buffer for string the server sends */
        fd_set loopRfds = rfds; //allows to drop
        int retval; 
        #ifdef WIN32
        WSADATA wsaData;
        WSAStartup(0x0101, & wsaData);
        #endif
        
        //ncurses set up
        setlocale(LC_ALL,"");  
        initscr();  
        cbreak();
        nl();
        noecho();
        intrflush(stdscr, FALSE);   
        keypad(stdscr, TRUE); 
        clear();
        
        //gets local ip address
        char hostBuf[100];
        int localName = gethostname(hostBuf, sizeof(hostBuf));
        struct hostent *host_entry = gethostbyname(hostBuf);
        strcpy(localIP, inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0])));
        
        //if head type
        if ((strcmp(type, "head") == 0)) {
                sd = clientSocket(rport, raddr);
                rightSd = sd;
                leftSd = -1;
                FD_ZERO(&rfds);
                FD_SET(STDIN_FILENO, &rfds);
                FD_SET(sd, &rfds);
                while(1) {
                    loopRfds = rfds;
                    retval = select (sd+1, &loopRfds, NULL, NULL, NULL);
                    if (retval == -1) {
    	               printf("Error: Select error\n");
    	            } else if (FD_ISSET(STDIN_FILENO, &loopRfds)) {
    	               readInput(getch());
    	            } else if (FD_ISSET(sd, &loopRfds)) {
    	               if((n = recv(sd, buf, sizeof(buf), 0)) == 0) {
    	                    //printf("\r\nDisconected\r\n");
    	               }
                       if(buf[0] == 10) {
                           printf("\r\n");
                       } else {
                           write(1, buf, n);
                       }
                       if(lpl) {
                            send(sd, buf, n, 0);
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
                        strcpy(leftStatus, "LISTENING");
                        if ((sd2 = accept(sd, (struct sockaddr * ) & cad, & alen)) < 0) {
                                fprintf(stderr, "Error: Accept failed\n");
                                exit(EXIT_FAILURE);
                        }
                        strcpy(leftStatus, "CONNECTED");
                        leftSd = sd2;
                        rightSd = -1;
                        FD_ZERO(&rfds);
                        FD_SET(STDIN_FILENO, &rfds);
                        FD_SET(sd2, &rfds);
                        while(1) {
                            loopRfds = rfds;
                            retval = select (sd2+1, &loopRfds, NULL, NULL, NULL);
                            if (retval == -1) {
    	                       printf("Error: Select error\n");
    	                    } else if (FD_ISSET(STDIN_FILENO, &loopRfds)) {
                                readInput(getch());
    	                    } else if (FD_ISSET(sd2, &loopRfds)) {
    	                        if((n = recv(sd2, buf, sizeof(buf), 0)) == 0) {
    	                            //printf("\r\nDisconected\r\n");
    	                        }
                                if(buf[0] == 10) {
                                    printf("\r\n");
                                } else {
                                    write(1, buf, n);
                                }
                                if(lpr) {
                                    send(sd2, buf, n, 0);
                                }
    	                    }
                        }
	                    closesocket(sd2);
                }
                /* Closes and exits */
                closesocket(sd);
                //If middle type
        } else if (strcmp(type, "middle") == 0) {
                sd = serverSocket(lport);
                sd3 = clientSocket(rport, raddr);
                /* accept and handle requests */
                while (1) {
                        alen = sizeof(cad);
                        strcpy(leftStatus, "LISTENING");
                        if ((sd2 = accept(sd, (struct sockaddr * ) & cad, & alen)) < 0) {
                                fprintf(stderr, "Error: Accept failed\n");
                                exit(EXIT_FAILURE);
                        }
                        strcpy(leftStatus, "CONNECTED");
                        leftSd = sd2;
                        rightSd = sd3;
                        FD_ZERO(&rfds);
                        FD_SET(sd3, &rfds);
                        FD_SET(sd2, &rfds);
                        FD_SET(STDIN_FILENO, &rfds);
                        while(1) {
                            loopRfds = rfds;
                            retval = select(max(sd3,sd2)+1, &loopRfds, NULL, NULL, NULL);
                            if (retval == -1) {
    	                       printf("Error: Select error\n");
                            } else if (FD_ISSET(STDIN_FILENO, &loopRfds)) {
                                readInput(getch());
    	                    } else if (FD_ISSET(sd3, &loopRfds)) {
    	                        if((n = recv(sd3, buf, sizeof(buf), 0)) == 0) {
    	                            printf("\r\nDisconected\r\n");
    	                        }
    	                        //prints if rl to left
    	                        if(strcmp(displayDir, "rl") == 0) {
                                    if(buf[0] == 10) {
                                        printf("\r\n");
                                    } else {
                                        write(1, buf, n);
                                    }
    	                        }
    	                        if(lpl)
    	                            send(sd3, buf, n, 0);
    	                        else
                                    send(sd2, buf, n, 0);
    	                    } else if (FD_ISSET(sd2, &loopRfds)) {
    	                        if((n = recv(sd2, buf, sizeof(buf), 0)) == 0) {
    	                            printf("\r\nDisconected\r\n");
    	                        }
    	                        if(strcmp(displayDir, "lr") == 0) {
                                    if(buf[0] == 10) {
                                        printf("\r\n");
                                    } else {
                                        write(1, buf, n);
                                    }
    	                        }
    	                        if(lpr) {
    	                            if(send(sd2, buf, n, 0) == 0) {
    	                                printf("\r\nDisconected\r\n");
    	                            }
    	                        } else {
                                    if(send(sd3, buf, n, 0) == 0) {
                                        printf("\r\nDisconected\r\n");
                                    }
    	                        }
    	                    }
                        }
                /* Closes and exits */
                closesocket(sd2);
                }
                closesocket(sd);
                closesocket(sd3);
        }
        nocbreak();
        endwin();
        exit(0);
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
                {"lpr", no_argument, NULL, 10},
                {"lpl", no_argument, NULL, 11},
                {NULL , 0, NULL, 0}
        };
        char raddr[1000] = ""; //rraddr -- ip address
        char tempDisDir[5] = "";
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
                        strcpy(displayDir, "lr");
                        break;
                case 1:
                        if (type[0] != '\0') {
                                printf("Error: Both -head and -tail roles requested\n");
                                exit(EXIT_FAILURE);
                        }
                        strcpy(type, "tail");
                        strcpy(outputDir, "left");
                        strcpy(displayDir, "rl");
                        break;
                case 2:
                        strcpy(raddr, optarg);
                        strcpy(rightIP, optarg);
                        break;
                case 3:
                        rport = atoi(optarg);
                        strcpy(rightPort, optarg);
                        if (rport < 1 || rport > 65535) {
                                printf("Error: rrport address out of range %d\n", rport);
                                exit(EXIT_FAILURE);
                        }
                        break;
                case 4:
                        lport = atoi(optarg);
                        strcpy(leftPort, optarg);
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
                    printf("src with %s\n", src); //temp
                    break;
                case 6:
                    printf("left side persistant\n"); //temp
                    break;
                case 7:
                    printf("right side persistant\n"); //temp
                    break;
                case 8:
                    if(tempDisDir[0] != '\0') {
                        strcpy(tempDisDir, "lr");
                    } else {
                        printf("Error: Both -dsplf and -dsplr roles requested\n");
                        exit(0);
                    } 
                    break;
                case 9:
                    if (tempDisDir[0] != '\0') {
                        strcpy(tempDisDir, "rl");
                    } else {
                        printf("Error: Both -dsplf and -dsplr roles requested\n");
                        exit(0);
                    }
                    break;
                case 10:
                    printf("looper right\n"); // temp
                    lpr = true;
                    break;
                case 11:
                    printf("looper left\n"); // temp
                    lpl = true;
                    break;
                default:
                        exit(EXIT_FAILURE);
                }
        }
        
        if (type[0] == '\0') {
                strcpy(type, "middle");
                strcpy(outputDir, "right");
                if(tempDisDir[0] != '\0')
                    strcpy(displayDir, tempDisDir);
                else
                    strcpy(displayDir, "lr");
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

