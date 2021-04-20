#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

int main(int argc, char *argv[]) {
        static struct option args[] = {
                {"head", no_argument, NULL, 0},
                {"tail", no_argument, NULL, 1},
                {"rraddr", required_argument, NULL, 2},
                {"rrport", required_argument, NULL, 3},
                {"llport", required_argument, NULL, 4},
                {NULL, 0, NULL, 0}
        };
        char type[10] = "";
        char raddr[1000] = "";
        int lport = 0;
        int rport = 0;
        int opt = 0;
        while((opt = getopt_long_only(argc, argv, "", args, NULL)) != -1) {
                switch(opt) {
                        case 0:
                                if(type[0] != '\0') {
                                        printf("Error: Both -head and -tail roles requested");
                                        exit(EXIT_FAILURE);
                                }
                                strcpy(type, "head");
                                break;
                        case 1:
                                if(type[0] != '\0') {
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
                                if(rport < 1 || rport > 65535) {
                                        printf("Error: rrport address out of range \"value\"\n");
                                        exit(EXIT_FAILURE);
                                }
                                break;
                        case 4:
                                lport = atoi(optarg);
                                if(lport < 1 || lport > 65535) {
                                        printf("Error: llport address out of range \"value\"\n");
                                        exit(EXIT_FAILURE);
                                }
                                break;
                        default: exit(1);
                }
        }

        if(type[0] == '\0') {
                strcpy(type, "middle");
        }

        if(raddr[0] == '\0' && (strcmp(type, "middle") == 0)) {
                printf("Error: Middle role requested -rraddr required\n");
                exit(EXIT_FAILURE);
        }

        if(raddr[0] == '\0' && (strcmp(type, "head") == 0)) {
                printf("Error: Head role requested -rraddr required\n");
                exit(EXIT_FAILURE);
        }

        printf("type = %s\n", type);
        printf("radr = %s\n", raddr);
        printf("rport = %d\n", rport);
        printf("lport = %d\n", lport);
}
