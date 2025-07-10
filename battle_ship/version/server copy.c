#include "../include/gameLogic.h"
#include "../include/grid.h"
#include "../include/network.h"
#include "../include/ship.h"
#include "../include/tuple.h"
#include <arpa/inet.h>
#include <errno.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

extern char *id;
extern short port;
extern int sock;

int main(int argc, char **argv) {

    tuple right;
    right.x = 1;
    right.y = 0;
    tuple left;
    left.x = -1;
    left.y = 0;
    tuple up;
    up.x = 0;
    up.y = -1;
    tuple down;
    down.x = 0;
    down.y = 1;
    tuple direction[4] = {right, left, up, down};

    int nbShips = 8;

    struct sockaddr_in server; // server SAP

    if (argc != 3) {
        fprintf(stderr, "usage: %s id port\n", argv[0]);
        exit(1);
    }
    id = argv[1];
    port = atoi(argv[2]);

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "%s: socket %s\n", argv[0], strerror(errno));
        exit(1);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;

    // Bind socket
    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        fprintf(stderr, "%s: bind %s\n", argv[0], strerror(errno));
        exit(1);
    }

    // Listen on socket
    if (listen(sock, 5) != 0) {
        fprintf(stderr, "%s: listen %s\n", argv[0], strerror(errno));
        exit(1);
    }

    while (1) {
        gameLoop(direction, nbShips, argv);
    }
}
