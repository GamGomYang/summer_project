#ifndef DAVINCI_H
#define DAVINCI_H

#define MAX_TILES 12
#define MAX_PLAYERS 2
#define TOTAL_TILES (MAX_TILES * 2)

typedef struct {
    int number;
    char color; 
    int revealed;
} Tile;

typedef struct {
    Tile tiles[MAX_TILES * 2];
    int num_tiles;
} Player;


void initialize_game(Player players[]);
void draw_tile(Player *player);
int guess_tile(Player *opponent, int index, char color, int number);
int check_win(Player *opponent);
int compare_tiles(const void *a, const void *b);

#endif 


