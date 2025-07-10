#include "../include/network.h"
#include "../include/gameLogic.h"
#include "../include/grid.h"
#include "../include/network.h"
#include "../include/ship.h"
#include "../include/tuple.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

char *id = 0;
short port = 0;
int sock = 0;

ssize_t readLine(int sockfd, char *buffer, size_t maxlen) {
    ssize_t n, rc;
    char c;
    for (n = 0; n < maxlen - 1; n++) {
        rc = read(sockfd, &c, 1);
        if (rc == 1) {
            buffer[n] = c;
            if (c == '\n') {
                n++;
                break;
            }
        } else if (rc == 0) {
            break;
        } else {
            if (errno == EINTR)
                continue;
            return -1;
        }
    }
    buffer[n] = '\0';
    return n;
}

void handleClientCommunication(int sock_pipe, struct sockaddr_in client, struct Cell grid[GRID_SIZE][GRID_SIZE], int *nbShipSunk, bool *win, tuple direction[4], int nbShips, char *argv[]) {
    char buf_read[256], buf_write[256];
    int ret;

    // coordinates from client
    ret = readLine(sock_pipe, buf_read, sizeof(buf_read));
    if (ret <= 0) {
        printf("%s: read=%d: %s\n", argv[0], ret, strerror(errno));
        return;
    }

    printf("server %s received from client (%s,%4d) : %s\n", id, inet_ntoa(client.sin_addr), ntohs(client.sin_port), buf_read);
    int tirX = 0;
    int tirY = 0;
    sscanf(buf_read, "(%d %d)", &tirX, &tirY);
    tirX--;
    tirY--;

    if (!(tirX >= 10 || tirY >= 10 || tirX < 0 || tirY < 0)) {
        if (grid[tirX][tirY].aState == UNSHOT) {
            if (grid[tirX][tirY].aShip == NONE) {
                grid[tirX][tirY].aState = MISS;
                // responding to client
                sprintf(buf_write, "Miss\n");
            } else {
                grid[tirX][tirY].aState = HIT;

                // checking if boat is sunk
                int orientation = 0;
                for (int i = 0; i < 4; i++) {
                    if (grid[tirX + direction[i].x][tirY + direction[i].y].aShip == grid[tirX][tirY].aShip) {
                        orientation = i;
                    }
                }
                int tirXBis = tirX;
                int tirYBis = tirY;
                int counter = 0;
                while (grid[tirXBis - direction[orientation].x][tirYBis - direction[orientation].y].aShip == grid[tirX][tirY].aShip) {
                    tirXBis -= direction[orientation].x;
                    tirYBis -= direction[orientation].y;
                }
                for (int i = 0; i < grid[tirX][tirY].aShip; i++) {
                    if (grid[tirXBis][tirYBis].aState == HIT) {
                        counter++;
                    }
                    tirXBis += direction[orientation].x;
                    tirYBis += direction[orientation].y;
                }
                if (counter == grid[tirX][tirY].aShip) {
                    (*nbShipSunk)++;
                    // testing if all ships are sunk
                    if (*nbShipSunk == nbShips) {
                        *win = true;
                        sprintf(buf_write, "Hit, sunk\n You won!\n");
                    } else {
                        // responding to client
                        sprintf(buf_write, "Hit, sunk !\n");
                    }

                    // change the state of cells to SUNK where a boat is sunk
                    tirXBis = tirX;
                    tirYBis = tirY;
                    for (int i = 0; i < grid[tirX][tirY].aShip; i++) {
                        grid[tirXBis][tirYBis].aState = SUNK;
                        tirXBis += direction[orientation].x;
                        tirYBis += direction[orientation].y;
                    }
                } else {
                    // responding to client
                    sprintf(buf_write, "Hit !\n");
                }
            }
        } else {
            // responding to client
            sprintf(buf_write, "%s", "You already shot there\n");
        }
    } else {
        // responding to client
        sprintf(buf_write, "%s", "Invalid Coordinates\n");
    }

    // sending response to client
    ret = write(sock_pipe, buf_write, strlen(buf_write));

    char confirm_buf[1];
    // Wait for confirmation from client
    ret = read(sock_pipe, confirm_buf, 1);
    if (ret <= 0) {
        printf("%s: read=%d: %s\n", argv[0], ret,
               strerror(errno));
        return;
    }

    // Check if anything was received from the client
    if (ret > 0) {
        printf("Received confirmation from client\n");
    } else {
        printf("Did not receive confirmation from client\n");
        return;
    }

    // sending grid to client
    char *gridStr = gridToString(grid);
    ret = write(sock_pipe, gridStr, strlen(gridStr));

    if (ret <= 0) {
        printf("%s: write=%d: %s\n", argv[0], ret,
               strerror(errno));
        return;
    }

    // printing grid
    for (int i = 0; i < 10; i++) {
        for (int y = 0; y < 10; y++) {
            if (grid[i][y].aShip == NONE || grid[i][y].aState != UNSHOT) {
                printf("%c ", grid[i][y].aState);
            } else
                printf("%d ", grid[i][y].aShip);
        }
        printf("\n");
    }
    printf("\n");
}
