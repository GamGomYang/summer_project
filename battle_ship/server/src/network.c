#include "../include/network.h"
#include "../include/gameLogic.h"
#include "../include/grid.h"
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
            break; // 연결 종료
        } else {
            if (errno == EINTR)
                continue; // 인터럽트가 발생하면 재시도
            return -1;    // 에러 발생
        }
    }
    buffer[n] = '\0'; // 문자열 종료
    return n;
}

void handleClientCommunication(int sock_pipe, struct sockaddr_in client, struct Cell grid[GRID_SIZE][GRID_SIZE], int *nbShipSunk, bool *win, tuple direction[4], int nbShips, char *argv[]) {
    char buf_read[256], buf_write[256];
    int ret; // `ret` 변수 선언

    // 클라이언트로부터 좌표 읽기
    ret = readLine(sock_pipe, buf_read, sizeof(buf_read));
    if (ret <= 0) {
        printf("Error reading from client: %s\n", strerror(errno));
        return;
    }

    printf("server %s received from client (%s,%4d) : %s\n", id, inet_ntoa(client.sin_addr), ntohs(client.sin_port), buf_read);
    int tirX = 0, tirY = 0;
    sscanf(buf_read, "(%d %d)", &tirX, &tirY);
    tirX--;
    tirY--;

    if (!(tirX >= 10 || tirY >= 10 || tirX < 0 || tirY < 0)) {
        if (grid[tirX][tirY].aState == UNSHOT) {
            if (grid[tirX][tirY].aShip == NONE) {
                grid[tirX][tirY].aState = MISS;
                sprintf(buf_write, "Miss\n");
            } else {
                grid[tirX][tirY].aState = HIT;
                int orientation = 0;
                for (int i = 0; i < 4; i++) {
                    if (grid[tirX + direction[i].x][tirY + direction[i].y].aShip == grid[tirX][tirY].aShip) {
                        orientation = i;
                    }
                }
                int tirXBis = tirX, tirYBis = tirY, counter = 0;
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
                    if (*nbShipSunk == nbShips) {
                        *win = true;
                        sprintf(buf_write, "Hit, sunk\n You won!\n");
                    } else {
                        sprintf(buf_write, "Hit, sunk !\n");
                    }
                    tirXBis = tirX;
                    tirYBis = tirY;
                    for (int i = 0; i < grid[tirX][tirY].aShip; i++) {
                        grid[tirXBis][tirYBis].aState = SUNK;
                        tirXBis += direction[orientation].x;
                        tirYBis += direction[orientation].y;
                    }
                } else {
                    sprintf(buf_write, "Hit !\n");
                }
            }
        } else {
            sprintf(buf_write, "You already shot there\n");
        }
    } else {
        sprintf(buf_write, "Invalid Coordinates\n");
    }

    // 클라이언트로 응답 전송
    ret = write(sock_pipe, buf_write, strlen(buf_write));
    if (ret <= 0) {
        printf("Error writing to client: %s\n", strerror(errno));
        return;
    }

    // 그리드 데이터 문자열로 변환 후 클라이언트에 전송
    for (int i = 0; i < GRID_SIZE; i++) {
        char row[128] = {0};
        int idx = 0;
        for (int y = 0; y < GRID_SIZE; y++) {
            char cell_char;
            if (grid[i][y].aShip == NONE || grid[i][y].aState != UNSHOT) {
                cell_char = grid[i][y].aState;
            } else {
                cell_char = '0' + grid[i][y].aShip;
            }
            row[idx++] = cell_char;
            row[idx++] = ' ';
        }
        row[idx - 1] = '\n';
        row[idx] = '\0';
        ret = write(sock_pipe, row, strlen(row));
        if (ret <= 0) {
            printf("Error sending grid to client: %s\n", strerror(errno));
            return;
        }
    }

    // 서버 그리드 상태 출력
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int y = 0; y < GRID_SIZE; y++) {
            if (grid[i][y].aState != UNSHOT) {
                printf("%c ", grid[i][y].aState);
            } else {
                printf("%d ", grid[i][y].aShip);
            }
        }
        printf("\n");
    }
    printf("\n");
}
