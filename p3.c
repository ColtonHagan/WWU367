/*
Name : Colton Hagan
Date : 5/7/21
Class : CS367
Program : Piggy2 program, networking program matching assignment description 
*/

#ifndef unix
#define WIN32
#include <windows.h>
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
#define NUMWINS 7
#define RES_BUF_SIZE 80

//functions
void proccessCmd(char cmd[]);
void readCmdFile(char filename[]);

//ncurses
WINDOW *w[NUMWINS];
WINDOW *sw[NUMWINS];

//ncurses cmd helper
bool cmdFull = true;
char prevCmd[10][20];
int  prevCmdNum = 0;
int  viewPrevCmd = 0;

//for commands/arguments
char type[10] = ""; //Type of connection (tail,head,middle)
char outputDir[10]; //Direction of output
char displayDir[10]; //Direction of display
char src[100]; //SRC file
bool lpl, lpr; //loopers
bool prsl, prsr; //persistant

//Socket info
int sd1, leftSd, rightSd; //left/right sd, 0 if doesn't exist
fd_set rfds; //select set
struct sockaddr_in cad; /* structure to hold client's address */
char localIP[100] = "*", localPort[100] = "*";
char rightPort[100] = "*", rightIP[100] = "*", leftIP[100] = "*", leftPort[100] = "*";
char rightStatus[20] = "DISCONNECTED", leftStatus[20] = "DISCONNECTED";
char raddr[1000] = ""; //rraddr
int lport = PROTOPORT; //llport
int rport = PROTOPORT; //rrport

//For reading input
int cmdLen; //length of commad
char cmd[100]; //command
char prevCh; //prevous char
bool insertMode = false; //if in insert mode

void updateWin(int i) {
  touchwin(w[i]);
  wrefresh(sw[i]);
}

void updateAll() {
    for (int i=0;i<NUMWINS;i++) updateWin(i);
}

void createWindows() {
    char response[RES_BUF_SIZE];
      int WPOS[NUMWINS][4]= { {16,66,0,0},{16,66,0,66},{16,66,16,0},{16,66,16,66},
        {3,132,32,0},{5,132,35,0},{3,132,40,0} };
      setlocale(LC_ALL,"");
      initscr();
      refresh();
      cbreak();
      noecho();
      nl();
      intrflush(stdscr, FALSE); 
      keypad(stdscr, TRUE);
      clear();
    
     if (  !(LINES==43) || !(COLS==132) ) { 
        move(0,0);
        addstr("P3 requires a screen size of 132 columns and 43 rows");
        move(1,0);
        addstr("Set screen size to 132 by 43 and try again");
        move(2,0);
        addstr("Press enter to terminate program");
        refresh();
        getstr(response);            // Pause so we can see the screen 
        endwin();
        exit(EXIT_FAILURE);
    }
    
    // create the 7 windows and the seven subwindows
    for (int i=0;i<NUMWINS;i++) {
         w[i]=newwin(WPOS[i][0],WPOS[i][1],WPOS[i][2],WPOS[i][3]);
         scrollok(w[i],TRUE);
         sw[i]=subwin(w[i],WPOS[i][0]-2,WPOS[i][1]-2,WPOS[i][2]+1,WPOS[i][3]+1);
         scrollok(sw[i],TRUE);
         wborder(w[i],0,0,0,0,0,0,0,0);
         touchwin(w[i]);
         wrefresh(w[i]);
         wrefresh(sw[i]);
    }
    
     // Write some stuff to the windows 
     wmove(sw[0],0,0);
     wprintw(sw[0],"Data arriving from the left");
     wmove(sw[1],0,0);
     waddstr(sw[1],"Data leaving right side");
     wmove(sw[2],0,0);
     waddstr(sw[2],"Data leaving the left side");
     wmove(sw[3],0,0);
     waddstr(sw[3],"Data arriving from the right"); 
     wmove(sw[4],0,0);
     waddstr(sw[4],"Commands");
     wmove(sw[5],0,0);
     waddstr(sw[5],"Input");
     wmove(sw[6],0,0);
     waddstr(sw[6],"Errors");
    
    // Place cursor at commands
    updateAll();
    wmove(sw[4],0,0); 
    updateWin(4);
}

// Makes and returns socket for client (which sends information) from given port and host
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
    while(1) {
        if (connect(sd, (struct sockaddr * ) & sad, sizeof(sad)) < 0) {
            if(!prsr) {
                fprintf(stderr, "Error: Failed to make connection to %s:%d\n", host, port);
                nocbreak();
                endwin();
                exit(EXIT_FAILURE);
            }
        } else {
            break;
        }
    }
    strcpy(rightStatus, "CONNECTED");
    socklen_t len = sizeof(sad);
    getsockname(sd, (struct sockaddr * ) & sad, & len);
    if ((int) ntohs(sad.sin_port) > 0) {
        snprintf(localPort, 100, "%d", (int) ntohs(sad.sin_port));
    }
    snprintf(rightPort, 100, "%d", port);
    return sd;
}

// Makes and returns socket for server (which recieves information) from given port
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
    socklen_t len = sizeof(sad);
    getsockname(sd, (struct sockaddr * ) & sad, & len);
    if ((int) ntohs(sad.sin_port) > 0) {
        snprintf(localPort, 100, "%d", (int) ntohs(sad.sin_port));
    }
    snprintf(leftPort, 100, "%d", port);
    return sd;
}

//Gets optional command from given cmd
char * getOptCmd(char * cmd, char * baseCmd) {
    char * optCmd = strtok(cmd, " =");
    while (optCmd != NULL) {
        if (strcmp(optCmd, baseCmd) != 0) {
            return optCmd;
        }
        optCmd = strtok(NULL, " =");
    }
    return NULL;
}

void readCmdFile(char filename[]) {
    FILE * fp;
    char cmd[1000];
    fp = fopen(filename, "r");
    if (fp == NULL) {
        wprintw(sw[6], "\r\nFile %s does not exist", filename);
        updateWin(6);
    } else {
        while (fgets(cmd, sizeof(cmd), fp)) {
            int len = strlen(cmd);
            if (len > 0 && cmd[len - 1] == '\n')
                cmd[len - 1] = 0;
            wprintw(sw[5], "\r%s\r\n", cmd);
            proccessCmd(cmd);
            updateWin(5);
        }
    }
    updateWin(5);
}

//uses given command
void proccessCmd(char cmd[]) {
    //quit
    if ((cmd[0] == 'q') && (strlen(cmd) == 1)) {
        closesocket(rightSd);
        closesocket(leftSd);
        closesocket(sd1);
        nocbreak();
        endwin();
        exit(0);
        //src
    } else if (strcmp(cmd, "src") == 0) {
        readCmdFile("scriptin");
    } else if (strstr(cmd, "src")) {
        char * optCdm = getOptCmd(cmd, "src");
        //incase you have src with extra spaces/enters
        if (optCdm != NULL) {
            readCmdFile(optCdm);
        }
        //output cmds
    } else if (strcmp(cmd, "outl") == 0) {
        if (strcmp(type, "tail") == 0 || strcmp(type, "middle") == 0)
            strcpy(outputDir, "left");
        else
            waddstr(sw[6],"\r\nError: Can't change output direction to left, connection is head");
            updateWin(6);
    } else if (strcmp(cmd, "outr") == 0) {
        if (strcmp(type, "head") == 0 || strcmp(type, "middle") == 0) {
            strcpy(outputDir, "right");
        } else {
             waddstr(sw[6],"\r\nError: Can't change output direction to right, connection is tail");
             updateWin(6);
        }
    } else if (strcmp(cmd, "out") == 0) {
        wprintw(sw[4],"\r\nOutput direction is %s", outputDir);
        cmdFull = true;
        updateWin(4);
        //display cmds
    } else if (strcmp(cmd, "lpl") == 0) {
        lpl = true;
    } else if (strcmp(cmd, "lpl0") == 0) {
        lpl = false;
    } else if (strcmp(cmd, "lpr") == 0) {
        lpr = true;
    } else if (strcmp(cmd, "lpr0") == 0) {
        lpr = false;
        //drop
    } else if (strcmp(cmd, "drpr") == 0) {
        if (rightSd != -1) {
            closesocket(rightSd);
            FD_CLR(rightSd, & rfds);
            rightSd = -1;
            strcpy(rightStatus, "DISCONNECTED");
            if (prsr) {
                strcpy(rightStatus, "LISTENING");
                rightSd = clientSocket(rport, raddr);
                FD_SET(rightSd, & rfds);
                strcpy(rightStatus, "CONNECTED");
            }
        } else {
             waddstr(sw[6],"\r\nError: Right connection doesn't exist");
             updateWin(6);
        }
    } else if (strcmp(cmd, "drpl") == 0) {
        if (leftSd != -1) {
            closesocket(leftSd);
            FD_CLR(leftSd, & rfds);
            leftSd = -1;
            strcpy(leftStatus, "DISCONNECTED");
            if (prsl) {
                strcpy(leftStatus, "LISTENING");
                int alen = sizeof(cad);
                if ((leftSd = accept(sd1, (struct sockaddr * ) & cad, & alen)) < 0) {
                        waddstr(sw[6],"\r\nError: Accept failed");
                        nocbreak();
                        endwin();
                        exit(EXIT_FAILURE);
                }
                FD_SET(leftSd, & rfds);
                strcpy(leftStatus, "CONNECTED");
            }
        } else {
             waddstr(sw[6],"\r\nError: Left connection doesn't exist");
             updateWin(6);
        }
        //show connection STDIN_FILENO
    } else if (strcmp(cmd, "rght") == 0) {
        if(strcmp(rightStatus,"DISCONNECTED") == 0)
            wprintw(sw[4],"%s:%s:*:* ",localIP, localPort); 
        else    
            wprintw(sw[4],"%s:%s:%s:%s ", localIP, localPort, rightIP, rightPort);
        wprintw(sw[4],"%s", rightStatus);
        cmdFull = true;
        updateWin(4);
    } else if (strcmp(cmd, "lft") == 0) {
        if(strcmp(leftStatus,"DISCONNECTED") == 0)
            wprintw(sw[4],"*:*::%s:%s ",localIP, localPort); 
        else 
            wprintw(sw[4],"%s:%s:%s:%s ", leftIP, leftPort, localIP, localPort);
        wprintw(sw[4],"%s", leftStatus);
        cmdFull = true;
        updateWin(4);
        //Not current command
    } else if (strcmp(cmd, "prsr") == 0) {
        prsr = true;
    } else if (strcmp(cmd, "prsl") == 0) {
        prsl = true;
    } else {
         waddstr(sw[6],"\r\nError: Command not recognised");
         updateWin(6);
    }
    updateWin(6);
    updateWin(4);
}

//reads input via insert/cmd adding given char to either
void readInput(int currentCh) {
    bool modeChange = false; 
    int x, y;
    getyx(sw[4], y, x);
    
    //moves to new start line with enter
    if (currentCh == 10) {
        if(insertMode) {
            waddch(sw[5],'\r');
            updateWin(5);
        } else {
            waddch(sw[4],'\r');
            updateWin(4);
        }
    }
    
    //backspace
    if (currentCh == 127 || currentCh == 263) {
        if (!(x == 0)) {
            wmove(sw[4], y, x - 1);
            wdelch(sw[4]);
            cmd[cmdLen] = '\0';
            if(cmdLen > 0)
                cmdLen--;
        }
    }
    
    //del
    /*if(currentCh == 126) {
        wdelch(sw[4]);
        updateWin(4);
    }*/
    
    //deals esc commands cause I could'nt find a better way
    if(!insertMode && currentCh == 27) {
        currentCh = wgetch(sw[4]);
        if (currentCh == '[') {
            currentCh = wgetch(sw[4]);
            //moveleft
            if(currentCh == 68) {
                if (x != 0)
                    wmove(sw[4], y, x - 1);
            //del
            } else if (currentCh == 51) {
                wdelch(sw[4]);
            //home
            } else if (currentCh == 72) {
                wmove(sw[4],y, 0);
            //end    
            } else if (currentCh == 70) {
                wmove(sw[4], y, cmdLen);
            //up
            } else if (currentCh == 65) {
                werase(sw[4]);
                cmd[cmdLen] = '\0';
                if(viewPrevCmd < prevCmdNum) {
                    strcpy(cmd, prevCmd[viewPrevCmd]);
                    cmdLen = strlen(cmd);
                    waddstr(sw[4], cmd);
                    viewPrevCmd++;
                } else {
                    strcpy(cmd, prevCmd[prevCmdNum-1]);
                    cmdLen = strlen(cmd);
                    waddstr(sw[4],cmd);
                }
            }
        }
        updateWin(4);
        currentCh = 127;
    }
    
    //prints slash without any meta        
    if (currentCh == '\\' && prevCh != '\\') {
        if(insertMode) {
            waddch(sw[5],currentCh);
            updateWin(5);
        } else {
            waddch(sw[4], currentCh);
            updateWin(4);
        }
    //ignores some things
    } else if (currentCh == 127 || currentCh ==  263 || currentCh ==  126) {
        //exits insert mode
    } else if (insertMode) {
        if (currentCh == 27 && prevCh != '\\') {
            modeChange = true;
            insertMode = false;
            cmd[0] = '\0';
            cmdLen = 0;
            viewPrevCmd = 0;
            waddstr(sw[4], "\r\n");
            wmove(sw[4], 0, 0);
            updateWin(4);
        } else {
            waddch(sw[5], currentCh);
            if (strcmp(outputDir, "left") == 0) {
                send(leftSd, & currentCh, 1, 0);
            } else {
                send(rightSd, & currentCh, 1, 0);
            }
            updateWin(5);
        }
        // command mode
    } else {
        //enters insert mode
        if (currentCh == 'i' && prevCh != '\\') {
            modeChange = true;
            insertMode = true;
            waddstr(sw[5], "\r\n");
            wmove(sw[5], 0, 0);
            //printf("\r\nEntering insert mode:\r\n");
            //proccess word/command on enter
        } else if (currentCh == 10) {
            //prints enter
            waddch(sw[4], currentCh);
            updateWin(4);
            viewPrevCmd = 0;
            //proccess cmd
            cmd[cmdLen] = '\0';
            char tempCopy[10][20];
            for(int i = 0; i < 10; i++) {
                strcpy(tempCopy[i], prevCmd[i]);
            }
            for(int i = 1; i < 10; i++) {
                strcpy(prevCmd[i], tempCopy[i-1]);
            }
            strcpy(prevCmd[0], cmd);   
            if(prevCmdNum < 10) {
                prevCmdNum++;
            }
            proccessCmd(cmd);
            cmd[0] = '\0';
            cmdLen = 0;
            
            //adds char to current command
        } else {
            if(cmdFull) {
                werase(sw[4]);
                updateWin(4);
                cmdFull = false;
            }
            waddch(sw[4],currentCh);
            updateWin(4);
            cmd[cmdLen] = currentCh;
            cmdLen++;
        }
    }
    //update prevCH
    if (!modeChange)
        prevCh = currentCh;
    else
        prevCh = '\0';
}

//Finds max of 2 given values and returns
int max(int x, int y) {
    return (x > y) ? x : y;
}
// Creates "head", "tail", or "middle" connection based on given type, attaches to given port and address if needed/asked to
void createConnection() {
    int sd, sd2, sd3 = -1; /* socket descriptors */
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
    createWindows();

    //gets local ip address
    char hostBuf[100];
    int localName = gethostname(hostBuf, sizeof(hostBuf));
    struct hostent * host_entry = gethostbyname(hostBuf);
    strcpy(localIP, inet_ntoa( * ((struct in_addr * ) host_entry -> h_addr_list[0])));

    //if head type
    if ((strcmp(type, "head") == 0)) {
        sd = clientSocket(rport, raddr);
        if (src[0] != '\0') {
            readCmdFile(src);
        }
        rightSd = sd;
        leftSd = -1;
        FD_ZERO( & rfds);
        FD_SET(STDIN_FILENO, & rfds);
        FD_SET(sd, & rfds);
        while (1) {
            loopRfds = rfds;
            retval = select(sd + 1, & loopRfds, NULL, NULL, NULL);
            if (retval == -1) {
                //printf("Error: Select error\n"); -- temp
            } else if (FD_ISSET(STDIN_FILENO, & loopRfds)) {
                readInput(wgetch(sw[4]));
            } else if (FD_ISSET(sd, & loopRfds)) {
                if ((n = recv(sd, buf, sizeof(buf), 0)) == 0) {
                    closesocket(rightSd);
                    FD_CLR(rightSd, & rfds);
                    rightSd = -1;
                    strcpy(rightStatus, "DISCONNECTED");
                    //reconects if persistant
                    if (prsr) {
                        strcpy(rightStatus, "LISTENING");
                        rightSd = clientSocket(rport, raddr);
                        FD_SET(rightSd, & rfds);
                        strcpy(rightStatus, "CONNECTED");
                    }
                } else {
                    if (buf[0] == 10) {
                        //printf("\r\n"); -- temp
                    } else {
                        //addch(buf[0]); -- temp insert
                        //updateAll();
                    }
                    if (lpl) {
                        send(sd, buf, n, 0);
                    }
                }
            }
        }
        closesocket(sd);
        exit(0);
        //if tail type
    } else if ((strcmp(type, "tail") == 0)) {
        sd = serverSocket(lport);
        sd1 = sd; //temp
        /* accepts and handles requests */
        while (1) {
            rightSd = -1;
            FD_ZERO( & rfds);
            FD_SET(STDIN_FILENO, & rfds);
            FD_SET(sd, & rfds);
            while(1) {
                loopRfds = rfds;
                retval = select(max(sd, sd2) + 1, & loopRfds, NULL, NULL, NULL);
                if (retval == -1) {
                    //printf("Error: Select error\n"); -- temp
                } else if (FD_ISSET(STDIN_FILENO, & loopRfds)) {
                    readInput(wgetch(sw[4]));
                } else if (FD_ISSET(sd, &loopRfds)) {
                    strcpy(leftStatus, "LISTENING");
                    alen = sizeof(cad);
                    if ((sd2 = accept(sd, (struct sockaddr * ) & cad, & alen)) < 0) {
                        fprintf(stderr, "Error: Accept failed\n");
                        nocbreak();
                        endwin();
                        exit(EXIT_FAILURE);
                    }
                    leftSd = sd2;
                    if (sd > 0) {
                        FD_SET(sd2, &rfds);
                        strcpy(leftStatus, "CONNECTED");
                        leftSd = sd2;
                    }
                    if (src[0] != '\0') {
                        readCmdFile(src);
                    }
                } else if (sd2 > -1 && FD_ISSET(sd2, & loopRfds)) {
                    if ((n = recv(sd2, buf, sizeof(buf), 0)) == 0) {
                        closesocket(sd2);
                        FD_CLR(sd2, & rfds);
                        leftSd = -1;
                        strcpy(leftStatus, "DISCONNECTED");
                        if (prsl) {
                            strcpy(leftStatus, "LISTENING");
                            alen = sizeof(cad);
                            if ((sd2 = accept(sd, (struct sockaddr * ) & cad, & alen)) < 0) {
                                fprintf(stderr, "Error: Accept failed\n");
                                nocbreak();
                                endwin();
                                exit(EXIT_FAILURE);
                            }
                            FD_SET(sd2,& rfds);
                            strcpy(leftStatus, "CONNECTED");
                        }
                    } else {
                        if (buf[0] == 10) {
                            //printf("\r\n"); -- temp
                        } else {
                            //addch(buf[0]); -- temp insert
                            //updateAll();
                        }
                        if (lpr) {
                            send(sd2, buf, n, 0);
                        }
                    }
                }
            }
            closesocket(sd2);
        }
        closesocket(sd);
        closesocket(sd2);
        //If middle type
    } else if (strcmp(type, "middle") == 0) {
        sd3 = clientSocket(rport, raddr);
        sd = serverSocket(lport);
        sd1 = sd; // <---- temp
        /* accept and handle requests */
        while(1) {
            rightSd = sd3;
            FD_ZERO( &rfds);
            FD_SET(sd3, &rfds);
            FD_SET(sd, &rfds);
            FD_SET(STDIN_FILENO, & rfds);
            if (src[0] != '\0') {
                readCmdFile(src);
            }
            while (1) {
                loopRfds = rfds;
                retval = select(max(sd3, max(sd, sd2)) + 1, & loopRfds, NULL, NULL, NULL);
                if (retval == -1) {
                    //printf("Error: Select error\n"); -printf
                } else if (FD_ISSET(STDIN_FILENO, & loopRfds)) {
                    readInput(wgetch(sw[4]));
                } else if (FD_ISSET(sd, & loopRfds)) {
                    strcpy(leftStatus, "LISTENING");
                    alen = sizeof(cad);
                    if ((sd2 = accept(sd, (struct sockaddr * ) & cad, & alen)) < 0) {
                        fprintf(stderr, "Error: Accept failed\n");
                        nocbreak();
                        endwin();
                        exit(EXIT_FAILURE);
                    }
                    leftSd = sd2;
                    //connects to server
                    if (sd > 0) {
                        FD_SET(sd2, & rfds);
                        strcpy(leftStatus, "CONNECTED");
                        leftSd = sd2;
                    }
                    
                    if (src[0] != '\0') {
                        readCmdFile(src);
                    }
                } else if (rightSd > -1 && FD_ISSET(sd3, & loopRfds)) {
                    if ((n = recv(rightSd, buf, sizeof(buf), 0)) == 0) {
                        closesocket(rightSd);
                        FD_CLR(rightSd, & rfds);
                        rightSd = -1;
                        strcpy(rightStatus, "DISCONNECTED");
                        //reconects if persistant
                        if (prsr) {
                            strcpy(rightStatus, "LISTENING");
                            rightSd = clientSocket(rport, raddr);
                            FD_SET(rightSd, & rfds);
                            strcpy(rightStatus, "CONNECTED");
                        }
                    } else {
                        //prints if rl to left
                        if (strcmp(displayDir, "rl") == 0) {
                            if (buf[0] == 10) {
                                //printf("\r\n"); --tenp
                            } else {
                                //addch(buf[0]); -- temp insert
                                //updateAll();
                            }
                        }
                        if (lpl) {
                            send(rightSd, buf, n, 0);
                        }
                        send(leftSd, buf, n, 0);
                    }
                } else if (leftSd > -1 && FD_ISSET(sd2, & loopRfds)) {
                    if ((n = recv(leftSd, buf, sizeof(buf), 0)) == 0) {
                        closesocket(sd2);
                        FD_CLR(sd2, & rfds);
                        leftSd = -1;
                        strcpy(leftStatus, "DISCONNECTED");
                        //reconects if persistant
                        if (prsl) {
                            strcpy(leftStatus, "LISTENING");
                            alen = sizeof(cad);
                            if ((sd2 = accept(sd, (struct sockaddr * ) & cad, & alen)) < 0) {
                                fprintf(stderr, "Error: Accept failed\n");
                                nocbreak();
                                endwin();
                                exit(EXIT_FAILURE);
                            }
                            FD_SET(sd2,& rfds);
                            strcpy(leftStatus, "CONNECTED");
                        }
                    } else {
                        if (strcmp(displayDir, "lr") == 0) {
                            if (buf[0] == 10) {
                                //printf("\r\n"); -- temp
                            } else {
                                //addch(buf[0]); -temp insert
                                //updateAll();
                            }
                        }
                        if (lpr) {
                            send(leftSd, buf, n, 0);
                        }
                        send(rightSd, buf, n, 0);
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
            "src",
            optional_argument,
            NULL,
            5
        },
        {
            "prsl",
            no_argument,
            NULL,
            6
        },
        {
            "prsr",
            no_argument,
            NULL,
            7
        },
        {
            "dsplr",
            no_argument,
            NULL,
            8
        },
        {
            "dsprl",
            no_argument,
            NULL,
            9
        },
        {
            "lpr",
            no_argument,
            NULL,
            10
        },
        {
            "lpl",
            no_argument,
            NULL,
            11
        },
        {
            NULL,
            0,
            NULL,
            0
        }
    };
    char tempDisDir[5] = "";
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
            strcpy(displayDir, "rl");
            break;
        case 1:
            if (type[0] != '\0') {
                printf("Error: Both -head and -tail roles requested\n");
                exit(EXIT_FAILURE);
            }
            strcpy(type, "tail");
            strcpy(outputDir, "left");
            strcpy(displayDir, "lr");
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
            if (optarg)
                strcpy(src, optarg); //This only works with = -- is this fixable?
            else
                strcpy(src, "scriptin");
            break;
        case 6:
            prsl = true;
            break;
        case 7:
            prsr = true;
            break;
        case 8:
            if (tempDisDir[0] != '\0') {
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
            lpr = true;
            break;
        case 11:
            lpl = true;
            break;
        default:
            exit(EXIT_FAILURE);
        }
    }

    if (type[0] == '\0') {
        strcpy(type, "middle");
        strcpy(outputDir, "right");
        if (tempDisDir[0] != '\0')
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
    createConnection();
}




