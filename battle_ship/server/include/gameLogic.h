#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "grid.h"
#include "ship.h"
#include "tuple.h"
#include <netinet/in.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

void receiveGridFromClient(int sock_pipe, struct Cell grid[GRID_SIZE][GRID_SIZE]);
void handleClientCommunication(int sock_pipe, struct sockaddr_in client,
                               struct Cell grid[GRID_SIZE][GRID_SIZE], int *nbShipSunk, bool *win,
                               tuple direction[4], int nbShips, char *argv[]);

void initGrids(struct Cell grid1[GRID_SIZE][GRID_SIZE], struct Cell grid2[GRID_SIZE][GRID_SIZE]);
void placeShips(struct Cell grid1[GRID_SIZE][GRID_SIZE], struct Cell grid2[GRID_SIZE][GRID_SIZE]);
void printGrid(struct Cell grid[GRID_SIZE][GRID_SIZE]);
void gameLoop(tuple direction[4], int nbShips, char *argv[]);

#endif // GAME_LOGIC_H