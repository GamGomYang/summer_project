#ifndef GRID_H
#define GRID_H

#include "ship.h"

#define GRID_SIZE 10
// Represents the state of a cell
enum State
{
    SUNK = '#',
    HIT = 'X',
    MISS = 'O',
    UNSHOT = '~'
};

// Cells making up the grid. Each cell has a ship and a state
struct Cell
{
    enum Ship aShip;
    enum State aState;
};

char* gridToString(struct Cell grid[GRID_SIZE][GRID_SIZE]);

#endif // GRID_H
