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

// gamelogic.h 에 넣었다가 오류 발생
void sendMessage(int sockfd, const char *message) {
    write(sockfd, message, strlen(message));
}
void initGrids(struct Cell grid1[GRID_SIZE][GRID_SIZE], struct Cell grid2[GRID_SIZE][GRID_SIZE]) {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            grid1[i][j].aShip = NONE;
            grid1[i][j].aState = UNSHOT;
            grid2[i][j].aShip = NONE;
            grid2[i][j].aState = UNSHOT;
        }
    }
}

void placeShips(struct Cell grid1[GRID_SIZE][GRID_SIZE], struct Cell grid2[GRID_SIZE][GRID_SIZE]) {
    for (int gridIndex = 0; gridIndex < 2; gridIndex++) {
        struct Cell(*grid)[GRID_SIZE][GRID_SIZE] = gridIndex == 0 ? &grid1 : &grid2;
        for (int shipType = CARRIER; shipType >= DESTROYER; shipType--) {
            for (int shipCount = 0; shipCount < (shipType == SUBMARINE ? 2 : 1); shipCount++) {
                bool placed = false;
                while (!placed) {
                    int x = rand() % GRID_SIZE;
                    int y = rand() % GRID_SIZE;
                    int direction = rand() % 4; // 0: right, 1: down, 2: left, 3: up
                    placed = true;
                    for (int i = 0; i < shipType; i++) {
                        int shipX = x + (direction == 0 ? i : direction == 2 ? -i
                                                                             : 0);
                        int shipY = y + (direction == 1 ? i : direction == 3 ? -i
                                                                             : 0);
                        if (shipX < 0 || shipX >= GRID_SIZE || shipY < 0 || shipY >= GRID_SIZE || (*grid)[shipX][shipY].aShip != NONE) {
                            placed = false;
                            break;
                        }
                    }
                    if (placed) {
                        for (int i = 0; i < shipType; i++) {
                            int shipX = x + (direction == 0 ? i : direction == 2 ? -i
                                                                                 : 0);
                            int shipY = y + (direction == 1 ? i : direction == 3 ? -i
                                                                                 : 0);
                            (*grid)[shipX][shipY].aShip = shipType;
                        }
                    }
                }
            }
        }
    }
}

void printGrid(struct Cell grid[GRID_SIZE][GRID_SIZE]) {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int y = 0; y < GRID_SIZE; y++) {
            if (grid[i][y].aShip == NONE && grid[i][y].aState == UNSHOT) {
                printf("%c ", grid[i][y].aState);
            } else
                printf("%d ", grid[i][y].aShip);
        }
        printf("\n");
    }
    printf("\n");
}

void gameLoop(tuple direction[4], int nbShips, char *argv[]) {
    struct sockaddr_in client1, client2;
    socklen_t len1 = sizeof(client1), len2 = sizeof(client2);
    int sock_pipe1, sock_pipe2;

    sock_pipe1 = accept(sock, (struct sockaddr *)&client1, &len1);
    sock_pipe2 = accept(sock, (struct sockaddr *)&client2, &len2);

    struct Cell grid1[GRID_SIZE][GRID_SIZE], grid2[GRID_SIZE][GRID_SIZE];
    initGrids(grid1, grid2);

    printf("클라이언트 1의 그리드 수신 중...\n");
    receiveGridFromClient(sock_pipe1, grid1);
    printf("클라이언트 2의 그리드 수신 중...\n");
    receiveGridFromClient(sock_pipe2, grid2);

    bool win = false;
    int nbShipSunk1 = 0, nbShipSunk2 = 0;
    int current_turn = 0; // 0: client1, 1: client2

    while (!win) {
        if (current_turn == 0) {
            sendMessage(sock_pipe1, "YOUR_TURN\n");
            sendMessage(sock_pipe2, "OPPONENT_TURN\n");
            handleClientCommunication(sock_pipe1, client1, grid2, &nbShipSunk2, &win, direction, nbShips, argv);
        } else {
            sendMessage(sock_pipe2, "YOUR_TURN\n");
            sendMessage(sock_pipe1, "OPPONENT_TURN\n");
            handleClientCommunication(sock_pipe2, client2, grid1, &nbShipSunk1, &win, direction, nbShips, argv);
        }
        current_turn = 1 - current_turn; // 턴 변경
        sleep(2);
    }

    close(sock_pipe1);
    close(sock_pipe2);
}
void receiveGridFromClient(int sock_pipe, struct Cell grid[GRID_SIZE][GRID_SIZE]) {
    char buffer[GRID_SIZE * GRID_SIZE + 1];
    int ret = read(sock_pipe, buffer, GRID_SIZE * GRID_SIZE);
    if (ret <= 0) {
        perror("그리드 데이터 수신 실패");
        exit(1);
    }

    // 수신된 데이터 확인
    buffer[ret] = '\0'; // 널 문자 추가

    // 문자열 데이터를 Cell 구조로 변환
    int index = 0;
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            grid[i][j].aShip = (buffer[index] == 'S') ? CARRIER : NONE;
            grid[i][j].aState = UNSHOT; // 초기 상태는 미공격
            index++;
        }
    }
}
