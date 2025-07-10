#include "../include/grid.h"

char* gridToString(struct Cell grid[GRID_SIZE][GRID_SIZE]) {
    static char gridStr[201]; // 100 cells + 100 spaces + 20 newline characters + 1 null terminator
    int index = 0;
    for(int i = 0; i < GRID_SIZE; i++) {
        for(int j = 0; j < GRID_SIZE; j++) {
            gridStr[index++] = grid[i][j].aState;
            gridStr[index++] = ' '; // Adding a space after each character
        }
        gridStr[index++] = '\n';
    }
    gridStr[index] = '\0';
    return gridStr;
}