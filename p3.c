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
WINDOW * w[NUMWINS];
WINDOW * sw[NUMWINS];

//ncurses cmd helper
bool cmdFull = true;
//for up
char prevCmd[10][20];
int prevCmdNum = 0;
int viewPrevCmd = 0;

//for commands/arguments
char type[10] = ""; //Type of connection (tail,head,middle)
char outputDir[10]; //Direction of output
char displayDir[10]; //Direction of display
char src[100]; //SRC file
bool lpl, lpr; //loopers
bool prsl, prsr; //persistant

//Socket info
int leftPassive, rightPassive, leftSd, rightSd = -1; //left/right sd, -1 if doesn't exist
fd_set rfds; //select set
struct sockaddr_in cad; /* structure to hold client's address */
char raddr[1000] = ""; //rraddr
char laddr[1000] = ""; //rraddr
int lport = PROTOPORT; //llport
int rport = PROTOPORT; //rrport
int lrport = -1;
int bindLeft, bindRight = -1;

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
    for (int i = 0; i < NUMWINS; i++) updateWin(i);
}

char *localInfo(int socket, bool left) {
    char portString[100];
    char* localInfoOut = malloc(100);
    
    //gets local address
    char hostBuf[100];
    int localName = gethostname(hostBuf, sizeof(hostBuf));
    struct hostent *host = gethostbyname(hostBuf);
    strcpy(localInfoOut, inet_ntoa( * ((struct in_addr * ) host -> h_addr_list[0])));
    strcat(localInfoOut,":");
    
    //gets local port
    struct sockaddr_in sin;
    int addrlen = sizeof(sin);
    if(getsockname(socket, (struct sockaddr *) &sin, &addrlen) == 0 && sin.sin_family == AF_INET && addrlen == sizeof(sin)) {
        int port = ntohs(sin.sin_port);
        if(port > 0) {
            snprintf(portString, 100, "%d", port);
            strcat(localInfoOut,portString);
        }
    } else {
        if(left) {
            snprintf(portString, 100, "%d", bindLeft);
        } else {
            snprintf(portString, 100, "%d", bindRight);
        }
        strcat(localInfoOut,portString);
    }
    
    return localInfoOut;
}

char *socketInfo(int socket) {
    char socketPort[100];
    char socketIp[INET_ADDRSTRLEN];
    char* socketInfoOut = malloc(100);
    
    if(socket > 0) {
        struct sockaddr_in sad;
	    socklen_t sad_len = sizeof(sad);
	    
        getpeername(socket,(struct sockaddr*)&sad,&sad_len);
		inet_ntop(AF_INET, &sad.sin_addr,socketIp, sizeof(socketIp));	
		sprintf(socketPort,"%d",ntohs(sad.sin_port));
		strcpy(socketInfoOut, socketIp);
		strcat(socketInfoOut, ":");
        strcat(socketInfoOut, socketPort);
    } else {
        return "*:*";
    }
}

void createWindows() {
    char response[RES_BUF_SIZE];
    int WPOS[NUMWINS][4] = {
        {
            16,
            66,
            0,
            0
        },
        {
            16,
            66,
            0,
            66
        },
        {
            16,
            66,
            16,
            0
        },
        {
            16,
            66,
            16,
            66
        },
        {
            3,
            132,
            32,
            0
        },
        {
            5,
            132,
            35,
            0
        },
        {
            3,
            132,
            40,
            0
        }
    };
    setlocale(LC_ALL, "");
    initscr();
    refresh();
    cbreak();
    noecho();
    nl();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
    clear();

    if (!(LINES == 43) || !(COLS == 132)) {
        move(0, 0);
        addstr("P3 requires a screen size of 132 columns and 43 rows");
        move(1, 0);
        addstr("Set screen size to 132 by 43 and try again");
        move(2, 0);
        addstr("Press enter to terminate program");
        refresh();
        getstr(response); // Pause so we can see the screen 
        endwin();
        exit(EXIT_FAILURE);
    }

    // create the 7 windows and the seven subwindows
    for (int i = 0; i < NUMWINS; i++) {
        w[i] = newwin(WPOS[i][0], WPOS[i][1], WPOS[i][2], WPOS[i][3]);
        scrollok(w[i], TRUE);
        sw[i] = subwin(w[i], WPOS[i][0] - 2, WPOS[i][1] - 2, WPOS[i][2] + 1, WPOS[i][3] + 1);
        scrollok(sw[i], TRUE);
        wborder(w[i], 0, 0, 0, 0, 0, 0, 0, 0);
        touchwin(w[i]);
        wrefresh(w[i]);
        wrefresh(sw[i]);
    }
    wmove(sw[4], 0, 0);
    waddstr(sw[4], "Commands");
    wmove(sw[5], 0, 0);
    waddstr(sw[5], "Input");
    wmove(sw[6], 0, 0);
    waddstr(sw[6], "Errors");

    // Place cursor at commands
    updateAll();
    wmove(sw[4], 0, 0);
    updateWin(4);
}

/* Bind a local port to the socket */
void bindSocket(int socket, int port) {
    struct sockaddr_in sad;
    memset((char * ) & sad, 0, sizeof(sad)); /* clear sockaddr structure */
    sad.sin_family = AF_INET;
    sad.sin_port = htons((u_short) port);
    
    if (bind(socket, (struct sockaddr * ) &sad, sizeof(sad)) < 0) {
        fprintf(stderr, "Error: Bind failed\n");
        exit(EXIT_FAILURE);
    }
}
// Makes and returns socket for client (which sends information) from given port and host
int clientSocket(int port, int bindPort, char * host) {
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
    
    if(bindSocket > 0) {
        bindSocket(sd,bindPort);
    }
    /* Connect the socket to the specified server. */
    while (1) {
        if (connect(sd, (struct sockaddr * ) & sad, sizeof(sad)) < 0) {
            if (!prsr) {
                fprintf(stderr, "Error: Failed to make connection to %s:%d\n", host, port);
                nocbreak();
                endwin();
                exit(EXIT_FAILURE);
            }
        } else {
            break;
        }
    }
    return sd;
}

// Makes and returns socket for server (which recieves information) from given port
int serverSocket(int port, int bindPort) {
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
    
    if(bindPort > 0) {
        bindSocket(sd,bindPort);
    } else {
        bindSocket(sd,port);
    }
    
    /* Specify size of request queue */
    if (listen(sd, QLEN) < 0) {
        fprintf(stderr, "Error: Listen failed\n");
        exit(EXIT_FAILURE);
    }
    return sd;
}

void insertData(char currentCh) {
    waddch(sw[5], currentCh);
    if ((strcmp(outputDir, "left") == 0) && (leftSd != -1)) {
        waddch(sw[3], currentCh);
        send(leftSd, & currentCh, 1, 0);
        updateWin(3);
    } else if ((strcmp(outputDir, "right") == 0) && (rightSd != -1)) {
            waddch(sw[1], currentCh);
            send(rightSd, & currentCh, 1, 0);
            updateWin(1);
    }
    updateWin(5);
}

char* split(char cmd[], int index) {
    int i = 0;
    char *p = strtok (cmd, " ");
    char *array[100];
    for(int j; j < 100; j++) {
        array[j] = malloc(100);
    }
    while (p != NULL){
        array[i++] = p;
        p = strtok (NULL, " ");
    }
    if(index >= i) {
        return NULL;
    } 
    return array[index];
}

void readInsertFile(char* fileName) {
    FILE *fp = fopen(fileName, "r");
    int c;
    if(fp == NULL) {
        wprintw(sw[6],"\r\nFile %s does not exist", fileName);
        updateWin(6);
    } else {
        while((c = fgetc(fp)) != EOF) {
            insertData(c);
        }
    }
    updateWin(5);
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
        closesocket(leftPassive);
        nocbreak();
        endwin();
        exit(0);
        //src
    } else if (strcmp(cmd, "src") == 0) {
        readCmdFile("scriptin");
    } else if (strstr(cmd, "src")) {
        char * optCdm = split(cmd, 1);
        //incase you have src with extra spaces/enters
        if (optCdm != NULL) {
            readCmdFile(optCdm);
        }
    } else if (strstr(cmd, "read")) {
        char* fileName = split(cmd, 1);
        if(fileName != NULL) {
            werase(sw[5]);
            readInsertFile(fileName);
            updateWin(4);
        } else {
            waddstr(sw[6], "Error: Read command must include file to read");
            updateWin(6);
        }
    //binding of ports
    } else if (strstr(cmd, "llport")) {
        char* port = split(cmd, 1);
        if(port != NULL) {
            bindLeft = atoi(port);
        } else {
            bindLeft = PROTOPORT;
        }
        if(leftPassive > 0) {
            bindSocket(leftPassive,bindLeft);
        } else if (leftSd > 0) {
            bindSocket(leftSd,bindLeft);
        }
    } else if (strstr(cmd, "rlport")) {
        char* port = split(cmd, 1);
        if(port != NULL) {
            bindRight = atoi(port);
        } else {
            bindRight = PROTOPORT;
        }
        if(rightPassive > 0) {
            bindSocket(rightPassive, bindRight);
        } else if (rightSd > 0) {
            bindSocket(rightSd, bindRight);
        }
    //Accept ports
    } else if (strstr(cmd, "lrport")) {
        char* port = split(cmd, 1);
        if(port != NULL) {
            lrport = atoi(port);
        } else {
           lrport = PROTOPORT;
        }
    //Set IP
    } else if (strstr(cmd, "lraddr")) {
        char* addr = split(cmd, 1);
        if(addr != NULL) {
            strcpy(laddr, addr);
        } else {
            waddstr(sw[6], "\r\nError: lraddr requires an IP Address");
            updateWin(6);
        }
    } else if (strstr(cmd, "rraddr")) {
        char* addr = split(cmd, 1);
        if(addr != NULL) {
            strcpy(raddr, addr);
        } else {
            waddstr(sw[6], "\r\nError: rraddr requires an IP Address");
            updateWin(6);
        }
    //output cmds
    } else if (strcmp(cmd, "outl") == 0) {
        if (strcmp(type, "tail") == 0 || strcmp(type, "middle") == 0)
            strcpy(outputDir, "left");
        else
            waddstr(sw[6], "\r\nError: Can't change output direction to left, connection is head");
        updateWin(6);
    } else if (strcmp(cmd, "outr") == 0) {
        if (strcmp(type, "head") == 0 || strcmp(type, "middle") == 0) {
            strcpy(outputDir, "right");
        } else {
            waddstr(sw[6], "\r\nError: Can't change output direction to right, connection is tail");
            updateWin(6);
        }
    } else if (strcmp(cmd, "out") == 0) {
        wprintw(sw[4], "\r\nOutput direction is %s", outputDir);
        cmdFull = true;
        updateWin(4);
        //loop cmds
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
        if(rightPassive > 0) {
            closesocket(rightPassive);
            closesocket(rightSd);
            FD_CLR(rightPassive, & rfds);
            FD_CLR(rightSd, & rfds);
            rightPassive = -1;
            rightSd = -1;
            if (prsr) {
                rightPassive = serverSocket(rport, bindRight);
                FD_SET(rightPassive, &rfds);
            }
        } else if (rightSd > 0) {
            closesocket(rightSd);
            FD_CLR(rightSd, & rfds);
            rightSd = -1;
            if (prsr) {
                rightSd = clientSocket(rport, bindRight, raddr);
                FD_SET(rightSd, & rfds);
            }
        } else {
            waddstr(sw[6], "\r\nError: Right connection doesn't exist");
            updateWin(6); 
        }
    } else if (strcmp(cmd, "drpl") == 0) {
        if(leftPassive > 0) {
            closesocket(leftPassive);
            closesocket(leftSd);
            FD_CLR(leftPassive, & rfds);
            FD_CLR(leftSd, & rfds);
            leftPassive = -1;
            leftSd = -1;
            if (prsl) {
                leftPassive = serverSocket(lport, bindLeft);
                FD_SET(leftPassive, &rfds);
            }
        } else if (leftSd > 0) {
            closesocket(leftSd);
            FD_CLR(leftSd, & rfds);
            leftSd = -1;
            if (prsl) {
                leftSd = clientSocket(lport, bindLeft, laddr);
                FD_SET(leftSd, &rfds);
            }
        } else {
            waddstr(sw[6], "\r\nError: Left connection doesn't exist");
            updateWin(6); 
        }
        //show connection STDIN_FILENO
    } else if (strcmp(cmd, "rght") == 0) {
        if(rightPassive > 0)
            wprintw(sw[4], "%s:%s ",localInfo(rightPassive,false),socketInfo(rightSd));
        else
            wprintw(sw[4], "%s:%s ",localInfo(rightSd,false),socketInfo(rightSd));
        if(rightSd > 0) {
            waddstr(sw[4], "CONNECTED");
        } else if (rightPassive > 0) {
            waddstr(sw[4], "LISTENING");
        } else {
            waddstr(sw[4], "DISCONNECTED");
        }
        cmdFull = true;
        updateWin(4);
    } else if (strcmp(cmd, "lft") == 0) {
        if (leftPassive > 0)
            wprintw(sw[4], "%s:%s ",socketInfo(leftSd), localInfo(leftPassive,true));
        else
            wprintw(sw[4], "%s:%s ",socketInfo(leftSd), localInfo(leftSd,true));
        if(leftSd > 0) {
            waddstr(sw[4], "CONNECTED");
        } else if (leftPassive > 0) {
            waddstr(sw[4], "LISTENING");
        } else {
            waddstr(sw[4], "DISCONNECTED");
        }
        cmdFull = true;
        updateWin(4);
    //Not current command
    } else if (strcmp(cmd, "prsr") == 0) {
        prsr = true;
    } else if (strcmp(cmd, "prsl") == 0) {
        prsl = true;
    } else {
        waddstr(sw[6], "\r\nError: Command not recognised");
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

    //backspace
    if (currentCh == 127 || currentCh == 263) {
        if (!(x == 0)) {
            wmove(sw[4], y, x - 1);
            wdelch(sw[4]);
            cmd[cmdLen] = '\0';
            if (cmdLen > 0)
                cmdLen--;
        }
    }

    //deals esc commands cause I could'nt find a better way
    if (!insertMode && currentCh == 27) {
        currentCh = wgetch(sw[4]);
        if (currentCh == '[') {
            currentCh = wgetch(sw[4]);
            //moveleft
            if (currentCh == 68) {
                if (x != 0)
                    wmove(sw[4], y, x - 1);
                //del
            } else if (currentCh == 51) {
                wdelch(sw[4]);
                //home
            } else if (currentCh == 72) {
                wmove(sw[4], y, 0);
                //end    
            } else if (currentCh == 70) {
                wmove(sw[4], y, cmdLen);
                //up
            } else if (currentCh == 65) {
                werase(sw[4]);
                cmd[cmdLen] = '\0';
                if (prevCmdNum == 0) {
                    werase(sw[4]);
                } else if (viewPrevCmd < prevCmdNum) {
                    strcpy(cmd, prevCmd[viewPrevCmd]);
                    cmdLen = strlen(cmd);
                    waddstr(sw[4], cmd);
                    viewPrevCmd++;
                } else {
                    strcpy(cmd, prevCmd[prevCmdNum - 1]);
                    cmdLen = strlen(cmd);
                    waddstr(sw[4], cmd);
                }

            }
            updateWin(4);
            currentCh = 127;
        }
    }

    //prints slash without any meta        
    if (currentCh == '\\' && prevCh != '\\') {
        if (insertMode) {
            waddch(sw[5], currentCh);
            updateWin(5);
        } else {
            waddch(sw[4], currentCh);
            updateWin(4);
        }
        //ignores some things
    } else if (currentCh == 127 || currentCh > 255 || currentCh == 126) {
        //exits insert mode
    } else if (insertMode) {
        if (currentCh == 27 && prevCh != '\\') {
            modeChange = true;
            insertMode = false;
            cmd[0] = '\0';
            cmdLen = 0;
            viewPrevCmd = 0;
            wmove(sw[4], 0, 0);
            updateWin(4);
        } else {
            insertData(currentCh);
        }
        // command mode
    } else {
        //enters insert mode
        if (currentCh == 'i' && prevCh != '\\') {
            modeChange = true;
            insertMode = true;
            werase(sw[5]); // said okay in assignment page
            wmove(sw[5], 0, 0);
            updateWin(5);
            //proccess word/command on enter
        } else if (currentCh == 10) {
            //prints enter
            waddch(sw[4], currentCh);
            updateWin(4);
            viewPrevCmd = 0;
            //proccess cmd
            cmd[cmdLen] = '\0';
            char tempCopy[10][20];
            for (int i = 0; i < 10; i++) {
                strcpy(tempCopy[i], prevCmd[i]);
            }
            for (int i = 1; i < 10; i++) {
                strcpy(prevCmd[i], tempCopy[i - 1]);
            }
            strcpy(prevCmd[0], cmd);
            if (prevCmdNum < 10) {
                prevCmdNum++;
            }
            proccessCmd(cmd);
            cmd[0] = '\0';
            cmdLen = 0;

            //adds char to current command
        } else {
            if (cmdFull) {
                werase(sw[4]);
                updateWin(4);
                cmdFull = false;
            }
            waddch(sw[4], currentCh);
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

int acceptConnection(int passiveSd, char* ip, int port) {
    
    char addrStr[INET_ADDRSTRLEN];
    char strAddr[INET_ADDRSTRLEN];
    struct hostent *host = NULL;
	//struct sockaddr_in cad;
    int acceptSd = -1;
    int alen = sizeof(cad);
    if ((acceptSd = accept(passiveSd, (struct sockaddr * ) &cad, &alen)) < 0) {
        waddstr(sw[6],"\r\nError: Accept attempt failed");
        updateWin(6);
    }
    //checks given Ip valid & requested
    if (ip != NULL && (strcmp(ip,"localhost") != 0)){
		host = gethostbyname(ip);
		inet_ntop(AF_INET,host->h_addr_list[0],addrStr,sizeof(addrStr));
		if (host == NULL) {
			wprintw(sw[6],"\r\nError: Requested IP is not valid");
			updateWin(6);
			closesocket(acceptSd);
			return -1;
        }
	}
	
    //checks Ip matchs
    if (host != NULL) {
		if (strcmp(addrStr, inet_ntop(AF_INET, &cad.sin_addr,strAddr, sizeof(strAddr))) != 0){
			waddstr(sw[6],"\r\nError: Accept and requested IP do not patch");
			updateWin(6);
			closesocket(acceptSd);
			return -1;	
		}
	}
    
    //checks port matchs
    if (port > 0){
		if (port != ntohs(cad.sin_port)){
			waddstr(sw[6],"\r\nError: Accept and requested ports do not match");
			updateWin(6);
			closesocket(acceptSd);
			return -1;	
		}	
	}
    
    return acceptSd;
}
// Creates "head", "tail", or "middle" connection based on given type, attaches to given port and address if needed/asked to
void createConnection() {
    int n; /* number of characters read */
    char buf[1000]; /* buffer for string the server sends */
    fd_set loopRfds = rfds; //allows to drop
    int retval; //select return
    //for windows
    #ifdef WIN32
    WSADATA wsaData;
    WSAStartup(0x0101, & wsaData);
    #endif

    //ncurses set up
    createWindows();
    FD_ZERO(&rfds);
    
    if((strcmp(type,"head") == 0 ) || (strcmp(type,"middle") == 0 )) {
        rightSd = clientSocket(rport, bindRight, raddr);
        FD_SET(rightSd, & rfds);
    }
    if((strcmp(type,"tail") == 0 ) || (strcmp(type,"middle") == 0 )) {
        leftPassive = serverSocket(lport, bindLeft);
        FD_SET(leftPassive, & rfds);
    }
    
    //runs src if there are no passive connections
    if (src[0] != '\0' && leftPassive <= 0 && rightPassive <= 0) {
            readCmdFile(src);
    }
    /* accept and handle requests */
    while (1) {
        FD_SET(STDIN_FILENO, & rfds);
        while (1) {
            loopRfds = rfds;
            retval = select(max(rightSd, max(rightPassive, max(rightSd, max(leftPassive, leftSd)))) + 1, & loopRfds, NULL, NULL, NULL);
            //select error
            if (retval == -1) {
                wprintw(sw[6], "\r\nError: Select error");
                updateWin(6);
            //stnd input
            } else if (FD_ISSET(STDIN_FILENO, & loopRfds)) {
                readInput(wgetch(sw[4]));
            //rightPassive
            } else if(rightPassive > -1 && FD_ISSET(rightPassive, & loopRfds)) {
                rightSd = acceptConnection(rightPassive,raddr,-1); // temp?? No current way to set
                if(rightSd > 0)
                    FD_SET(rightSd, & rfds);
            //leftPassive
            } else if (leftPassive > -1 && FD_ISSET(leftPassive, & loopRfds)) {
                leftSd = acceptConnection(leftPassive,laddr,lrport);
                if(leftSd > 0)
                    FD_SET(leftSd, & rfds);
            //rightSd
            } else if (rightSd > -1 && FD_ISSET(rightSd, & loopRfds)) {
                if ((n = recv(rightSd, buf, sizeof(buf), 0)) == 0) {
                    if(rightPassive > 0) {
                        closesocket(rightPassive);
                        closesocket(rightSd);
                        FD_CLR(rightPassive, & rfds);
                        FD_CLR(rightSd, & rfds);
                        rightPassive = -1;
                        rightSd = -1;
                        if (prsr) {
                            rightPassive = serverSocket(rport, bindRight);
                            FD_SET(rightPassive, &rfds);
                        }
                    } else if (rightSd > 0) {
                        closesocket(rightSd);
                        FD_CLR(rightSd, & rfds);
                        rightSd = -1;
                        if (prsr) {
                            rightSd = clientSocket(rport, bindRight, raddr);
                            FD_SET(rightSd, & rfds);
                        }
                    }
                } else {
                    waddch(sw[2], buf[0]);
                    updateWin(2);
                    if (lpl) {
                        send(rightSd, buf, n, 0);
                        waddch(sw[1], buf[0]);
                        updateWin(1);
                    }
                    if(leftSd > 0) {
                        send(leftSd, buf, n, 0);
                        waddch(sw[3], buf[0]);
                        updateWin(3);
                    }
                }
            //leftSd
            } else if (leftSd > -1 && FD_ISSET(leftSd, & loopRfds)) {
                if ((n = recv(leftSd, buf, sizeof(buf), 0)) == 0) {
                    if(leftPassive > 0) {
                        closesocket(leftPassive);
                        closesocket(leftSd);
                        FD_CLR(leftPassive, & rfds);
                        FD_CLR(leftSd, & rfds);
                        leftPassive = -1;
                        leftSd = -1;
                        if (prsr) {
                            leftPassive = serverSocket(lport, bindLeft);
                            FD_SET(leftPassive, &rfds);
                        }
                    } else if (leftSd > 0) {
                        closesocket(leftSd);
                        FD_CLR(leftSd, & rfds);
                        leftSd = -1;
                        if (prsl) {
                            leftSd = clientSocket(lport, bindLeft, laddr);
                            FD_SET(leftSd, &rfds);
                        }
                    }
                } else {
                    waddch(sw[0], buf[0]);
                    updateWin(0);
                    if (lpr) {
                        send(leftSd, buf, n, 0);
                        waddch(sw[3], buf[0]);
                        updateWin(3);
                    }
                    if(rightSd > 0) {
                        send(rightSd, buf, n, 0);
                        waddch(sw[1], buf[0]);
                        updateWin(1);
                    }
                }
            }
        }
    }
    /* Closes and exits */
    closesocket(leftPassive);
    closesocket(rightPassive);
    closesocket(leftSd);
    closesocket(rightSd);
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
            "rlport",
            required_argument,
            NULL,
            8
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
            break;
        case 3:
            rport = atoi(optarg);
            if (rport < 1 || rport > 65535) {
                printf("Error: rrport port out of range %d\n", rport);
                exit(EXIT_FAILURE);
            }
            break;
        case 4:
            lport = atoi(optarg);
            if (lport < 1 || lport > 65535) {
                printf("Error: llport port out of range %d\n", lport);
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
            bindRight = atoi(optarg);
            if (bindRight < 1 || bindRight > 65535) {
                printf("Error: rlport port out of range %d\n", bindRight);
                exit(EXIT_FAILURE);
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
    
    bindLeft = lport;
    strcpy(laddr, "localhost");
    createConnection();
}

