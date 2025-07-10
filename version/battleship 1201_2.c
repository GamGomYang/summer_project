// gcc -o battleship battleship.c -lncursesw -lpthread

#include <arpa/inet.h>
#include <errno.h>
#include <locale.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define PORT 8080
#define GRID_SIZE 10
#define SHIP_NUM 5
#define MAX_BUFFER 1024

/* ========================== */
/*       Data Structures      */
/* ========================== */

typedef struct {
    char name[20];
    int size;
    int x[5];
    int y[5];
    int hits;
} Ship;

typedef struct {
    char player_board[GRID_SIZE][GRID_SIZE];
    char enemy_board[GRID_SIZE][GRID_SIZE];
} GameState;

/* Multiplayer Specific Structures */
typedef enum {
    UNSHOT,
    MISS,
    HIT,
    SUNK
} CellState;

typedef struct {
    int aShip;        // 0: NONE, 1: DESTROYER, etc.
    CellState aState; // UNSHOT, MISS, HIT, SUNK
} Cell;

typedef struct {
    int x;
    int y;
} Cursor;

/* ========================== */
/*         Globals            */
/* ========================== */

char player_board[GRID_SIZE][GRID_SIZE];
char enemy_board[GRID_SIZE][GRID_SIZE];
Ship player_ships[SHIP_NUM];
Ship enemy_ships[SHIP_NUM];

/* Multiplayer Globals */
char *id = 0;
short sport = 0;
int sock_fd = 0; /* Communication Socket */
FILE *log_file = NULL;

/* ========================== */
/*       Function Prototypes  */
/* ========================== */

/* Initialization and Display */
void init_boards();
void display_board(char board[GRID_SIZE][GRID_SIZE], int reveal, int start_y);
void display_game_status();
void display_error_message(const char *message);
void display_attack_result(int result);
void display_game_result(const char *result_message);

/* Ship Handling */
void setup_ships(Ship ships[]);
int place_ships(Ship ships[], char board[GRID_SIZE][GRID_SIZE]);
void ai_place_ships(Ship ships[], char board[GRID_SIZE][GRID_SIZE]);

/* Game Logic */
int attack(int x, int y, char board[GRID_SIZE][GRID_SIZE], Ship ships[]);
int check_game_over(Ship ships[]);
void ai_attack(char board[GRID_SIZE][GRID_SIZE], Ship ships[], int difficulty);

/* Singleplayer and Multiplayer */
void start_singleplayer();
void start_multiplayer();

/* Utility */
void clear_input_buffer();
void handle_winch(int sig);

/* Multiplayer Specific */
void write_log(const char *format, ...);
void initGrid(Cell grid[GRID_SIZE][GRID_SIZE]);
ssize_t readLine(int sockfd, char *buffer, size_t maxlen);
void displayGrids(Cell own_grid[GRID_SIZE][GRID_SIZE], Cell opponent_grid[GRID_SIZE][GRID_SIZE], Cursor cursor, bool your_turn, bool attack_phase);
void placeShipsMultiplayer(Cell own_grid[GRID_SIZE][GRID_SIZE], Ship ships[SHIP_NUM]);
void sendGridToServer(Cell own_grid[GRID_SIZE][GRID_SIZE]);
void inputLoopMultiplayer(Cell own_grid[GRID_SIZE][GRID_SIZE], Cell opponent_grid[GRID_SIZE][GRID_SIZE]);
void gameLoopNcursesMultiplayer(Cell own_grid[GRID_SIZE][GRID_SIZE], Cell opponent_grid[GRID_SIZE][GRID_SIZE]);

/* ========================== */
/*        Implementation      */
/* ========================== */

/* ==========================
 * Initialization and Display Functions
 * ========================== */

/*
 * Initializes both player and enemy boards with water '~'
 */
void init_boards() {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            player_board[i][j] = '~'; // Water
            enemy_board[i][j] = '~';
        }
    }
}

/*
 * Displays a given board on the screen.
 * Parameters:
 *  - board: The game board to display.
 *  - reveal: If non-zero, reveal all ships.
 *  - start_y: The starting y-coordinate for display.
 */
void display_board(char board[GRID_SIZE][GRID_SIZE], int reveal, int start_y) {
    int mid_x = (COLS - (GRID_SIZE * 4 + 4)) / 2; // Center alignment

    attron(COLOR_PAIR(2)); // Background color

    // Display column indices
    mvprintw(start_y, mid_x + 4, "  ");
    for (int i = 0; i < GRID_SIZE; i++) {
        printw(" %2d ", i);
    }

    // Display board rows
    for (int i = 0; i < GRID_SIZE; i++) {
        mvprintw(start_y + i + 1, mid_x, "%2d |", i);
        for (int j = 0; j < GRID_SIZE; j++) {
            char c = board[i][j];
            if (c == 'S' && !reveal) {
                c = '~';
            }
            printw(" %c |", c);
        }
    }

    attroff(COLOR_PAIR(2)); // Reset background color
    refresh();
}

/*
 * Displays the current game status including both player and enemy boards.
 */
void display_game_status() {
    int mid_x = (COLS - strlen("Your Board:")) / 2;
    mvprintw(0, mid_x, "Your Board:");
    display_board(player_board, 1, 1);

    mid_x = (COLS - strlen("Enemy's Board:")) / 2;
    mvprintw(2 * GRID_SIZE + 2, mid_x, "Enemy's Board:");
    display_board(enemy_board, 0, 2 * GRID_SIZE + 3);

    mid_x = (COLS - strlen("Enter attack coordinates (x y):")) / 2;
    mvprintw(4 * GRID_SIZE + 5, mid_x, "Enter attack coordinates (x y): ");
    refresh();
}

/*
 * Displays an error message centered on the screen.
 */
void display_error_message(const char *message) {
    int mid_x = (COLS - strlen(message)) / 2;
    mvprintw(4 * GRID_SIZE + 7, mid_x, "%s", message);
    refresh();
}

/*
 * Displays the result of an attack (Hit, Miss, etc.)
 */
void display_attack_result(int result) {
    int mid_x;
    if (result == 1) {
        mid_x = (COLS - strlen("Hit!")) / 2;
        mvprintw(4 * GRID_SIZE + 7, mid_x, "Hit!");
    } else if (result == 0) {
        mid_x = (COLS - strlen("Miss!")) / 2;
        mvprintw(4 * GRID_SIZE + 7, mid_x, "Miss!");
    } else if (result == -1) {
        mid_x = (COLS - strlen("Already attacked this location.")) / 2;
        mvprintw(4 * GRID_SIZE + 7, mid_x, "Already attacked this location.");
    } else {
        mid_x = (COLS - strlen("Coordinates out of range.")) / 2;
        mvprintw(4 * GRID_SIZE + 7, mid_x, "Coordinates out of range.");
    }
    refresh();
    sleep(1);
}

/*
 * Displays the final game result (Win/Loss)
 */
void display_game_result(const char *result_message) {
    int mid_x = (COLS - strlen(result_message)) / 2;
    mvprintw(4 * GRID_SIZE + 9, mid_x, "%s", result_message);
    refresh();
    sleep(3);
}

/* ==========================
 * Ship Handling Functions
 * ========================== */

/*
 * Sets up the ships with their names and sizes.
 */
void setup_ships(Ship ships[]) {
    strcpy(ships[0].name, "Aircraft Carrier");
    ships[0].size = 5;
    ships[0].hits = 0;

    strcpy(ships[1].name, "Battleship");
    ships[1].size = 4;
    ships[1].hits = 0;

    strcpy(ships[2].name, "Cruiser");
    ships[2].size = 3;
    ships[2].hits = 0;

    strcpy(ships[3].name, "Submarine");
    ships[3].size = 3;
    ships[3].hits = 0;

    strcpy(ships[4].name, "Destroyer");
    ships[4].size = 2;
    ships[4].hits = 0;
}

/*
 * Places ships on the board based on user input.
 * Returns 0 on success.
 */
int place_ships(Ship ships[], char board[GRID_SIZE][GRID_SIZE]) {
    int x, y, orientation;
    for (int i = 0; i < SHIP_NUM; i++) {
        while (1) {
            clear();
            display_board(board, 1, 0);
            int mid_x = (COLS - 20) / 2;
            int mid_y = (LINES - GRID_SIZE - 6) / 2;
            mvprintw(mid_y + GRID_SIZE + 2, mid_x, "Place your ship: %s (Size: %d)", ships[i].name, ships[i].size);
            mvprintw(mid_y + GRID_SIZE + 3, mid_x, "Starting Position (x y): ");
            refresh();

            echo();
            if (scanw("%d %d", &x, &y) != 2) {
                mvprintw(mid_y + GRID_SIZE + 5, mid_x, "Invalid coordinates. Please try again.");
                noecho();
                clear_input_buffer();
                refresh();
                sleep(1);
                continue;
            }
            noecho();

            if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) {
                mvprintw(mid_y + GRID_SIZE + 5, mid_x, "Coordinates out of range.");
                refresh();
                sleep(1);
                continue;
            }

            mvprintw(mid_y + GRID_SIZE + 4, mid_x, "Orientation (0: Horizontal, 1: Vertical): ");
            refresh();

            echo();
            if (scanw("%d", &orientation) != 1) {
                mvprintw(mid_y + GRID_SIZE + 5, mid_x, "Invalid orientation. Please try again.");
                noecho();
                clear_input_buffer();
                refresh();
                sleep(1);
                continue;
            }
            noecho();

            if (orientation != 0 && orientation != 1) {
                mvprintw(mid_y + GRID_SIZE + 5, mid_x, "Orientation must be 0 or 1.");
                refresh();
                sleep(1);
                continue;
            }

            // Check if placement is possible
            int can_place = 1;
            for (int j = 0; j < ships[i].size; j++) {
                int nx = x + (orientation == 0 ? j : 0);
                int ny = y + (orientation == 1 ? j : 0);

                if (nx >= GRID_SIZE || ny >= GRID_SIZE) {
                    can_place = 0;
                    break;
                }

                if (board[ny][nx] == 'S') {
                    can_place = 0;
                    break;
                }
            }

            if (can_place) {
                for (int j = 0; j < ships[i].size; j++) {
                    int nx = x + (orientation == 0 ? j : 0);
                    int ny = y + (orientation == 1 ? j : 0);
                    board[ny][nx] = 'S';
                    ships[i].x[j] = nx;
                    ships[i].y[j] = ny;
                }
                break;
            } else {
                mvprintw(mid_y + GRID_SIZE + 5, mid_x, "Cannot place ship here. Choose a different location.");
                refresh();
                sleep(1);
            }
        }
    }
    return 0;
}

/*
 * Checks if all ships have been sunk.
 * Returns 1 if game is over, else 0.
 */
int check_game_over(Ship ships[]) {
    for (int i = 0; i < SHIP_NUM; i++) {
        if (ships[i].hits < ships[i].size) {
            return 0; // Game not over
        }
    }
    return 1; // All ships sunk
}

/* ==========================
 * AI Functions
 * ========================== */

/*
 * Places ships for the AI randomly.
 */
void ai_place_ships(Ship ships[], char board[GRID_SIZE][GRID_SIZE]) {
    srand(time(NULL));
    for (int i = 0; i < SHIP_NUM; i++) {
        while (1) {
            int x = rand() % GRID_SIZE;
            int y = rand() % GRID_SIZE;
            int orientation = rand() % 2;

            int can_place = 1;
            for (int j = 0; j < ships[i].size; j++) {
                int nx = x + (orientation == 0 ? j : 0);
                int ny = y + (orientation == 1 ? j : 0);

                if (nx >= GRID_SIZE || ny >= GRID_SIZE) {
                    can_place = 0;
                    break;
                }

                if (board[ny][nx] == 'S') {
                    can_place = 0;
                    break;
                }
            }

            if (can_place) {
                for (int j = 0; j < ships[i].size; j++) {
                    int nx = x + (orientation == 0 ? j : 0);
                    int ny = y + (orientation == 1 ? j : 0);
                    board[ny][nx] = 'S';
                    ships[i].x[j] = nx;
                    ships[i].y[j] = ny;
                }
                break;
            }
        }
    }
}

/*
 * AI attack logic based on difficulty.
 * Currently supports Easy, Normal, and Hard (same as Normal).
 */
void ai_attack(char board[GRID_SIZE][GRID_SIZE], Ship ships[], int difficulty) {
    static int hit_stack[GRID_SIZE * GRID_SIZE][2];
    static int stack_top = -1;

    int x, y, result;

    if (difficulty == 1) {
        // Easy: Random attack
        do {
            x = rand() % GRID_SIZE;
            y = rand() % GRID_SIZE;
            result = attack(x, y, board, ships);
        } while (result == -1 || result == -2);
    } else if (difficulty == 2 || difficulty == 3) {
        // Normal and Hard: Basic strategy
        if (stack_top >= 0) {
            x = hit_stack[stack_top][0];
            y = hit_stack[stack_top][1];
            stack_top--;
            result = attack(x, y, board, ships);
            if (result == -1 || result == -2) {
                ai_attack(board, ships, difficulty);
                return;
            }
        } else {
            do {
                x = rand() % GRID_SIZE;
                y = rand() % GRID_SIZE;
                result = attack(x, y, board, ships);
            } while (result == -1 || result == -2);

            if (result == 1) {
                // Add surrounding coordinates to stack
                if (x > 0) {
                    hit_stack[++stack_top][0] = x - 1;
                    hit_stack[stack_top][1] = y;
                }
                if (x < GRID_SIZE - 1) {
                    hit_stack[++stack_top][0] = x + 1;
                    hit_stack[stack_top][1] = y;
                }
                if (y > 0) {
                    hit_stack[++stack_top][0] = x;
                    hit_stack[stack_top][1] = y - 1;
                }
                if (y < GRID_SIZE - 1) {
                    hit_stack[++stack_top][0] = x;
                    hit_stack[stack_top][1] = y + 1;
                }
            }
        }
    }
}

/* ==========================
 * Game Logic Functions
 * ========================== */

/*
 * Handles the attack logic.
 * Returns:
 *  1  -> Hit
 *  0  -> Miss
 * -1  -> Already attacked
 * -2  -> Out of range
 */
int attack(int x, int y, char board[GRID_SIZE][GRID_SIZE], Ship ships[]) {
    if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) {
        return -2; // Out of range
    }

    if (board[y][x] == 'S') {
        board[y][x] = 'X'; // Hit

        // Increment hit count for the ship
        for (int i = 0; i < SHIP_NUM; i++) {
            for (int j = 0; j < ships[i].size; j++) {
                if (ships[i].x[j] == x && ships[i].y[j] == y) {
                    ships[i].hits++;
                    break;
                }
            }
        }

        return 1; // Hit
    } else if (board[y][x] == '~') {
        board[y][x] = 'O'; // Miss
        return 0;          // Miss
    } else {
        return -1; // Already attacked
    }
}

/* ==========================
 * Utility Functions
 * ========================== */

/*
 * Clears the input buffer to handle invalid inputs.
 */
void clear_input_buffer() {
    int ch;
    while ((ch = getch()) != '\n' && ch != EOF)
        ;
}

/*
 * Handles window resize signals to adjust the display.
 */
void handle_winch(int sig) {
    endwin();
    refresh();
    clear();
    display_game_status();
}

/* ==========================
 * Singleplayer Functions
 * ========================== */

/*
 * Starts the singleplayer mode.
 */
void start_singleplayer() {
    int difficulty;
    clear();
    int mid_y = LINES / 2 - 3;
    int mid_x = (COLS - strlen("Select Difficulty:")) / 2;

    mvprintw(mid_y, mid_x, "Select Difficulty:");
    mvprintw(mid_y + 2, mid_x, "1. Easy");
    mvprintw(mid_y + 3, mid_x, "2. Normal");
    mvprintw(mid_y + 4, mid_x, "3. Hard");
    mvprintw(mid_y + 6, mid_x, "Choice: ");
    refresh();

    // Handle invalid difficulty input
    echo();
    if (scanw("%d", &difficulty) != 1) {
        mvprintw(mid_y + 8, mid_x, "Please enter a valid number.");
        noecho();
        clear_input_buffer();
        refresh();
        sleep(2);
        return;
    }
    noecho();

    if (difficulty < 1 || difficulty > 3) {
        mvprintw(mid_y + 8, mid_x, "Invalid difficulty selection.");
        refresh();
        sleep(2);
        return;
    }

    init_boards();

    setup_ships(player_ships);
    setup_ships(enemy_ships);

    if (place_ships(player_ships, player_board) != 0) {
        mvprintw(10, 0, "Failed to place ships.");
        refresh();
        sleep(2);
        return;
    }

    ai_place_ships(enemy_ships, enemy_board);

    // Game play loop
    int game_over = 0;
    int x, y, result;
    while (!game_over) {
        clear();
        display_game_status();

        echo();
        if (scanw("%d %d", &x, &y) != 2) {
            display_error_message("Please enter valid coordinates.");
            noecho();
            clear_input_buffer();
            sleep(1);
            continue;
        }
        noecho();

        result = attack(x, y, enemy_board, enemy_ships);
        display_attack_result(result);

        if (check_game_over(enemy_ships)) {
            display_game_result("You Win!");
            break;
        }

        // AI's turn to attack
        ai_attack(player_board, player_ships, difficulty);

        if (check_game_over(player_ships)) {
            display_game_result("You Lose.");
            break;
        }
    }
}

/* ==========================
 * Multiplayer Functions
 * ========================== */

/*
 * Logs messages to a file for debugging purposes.
 */
void write_log(const char *format, ...) {
    if (log_file == NULL) {
        log_file = fopen("client_log.txt", "a");
        if (log_file == NULL) {
            perror("Failed to open log file");
            return;
        }
    }

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fflush(log_file);
}

/*
 * Initializes the game grid for multiplayer.
 */
void initGrid(Cell grid[GRID_SIZE][GRID_SIZE]) {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            grid[i][j].aShip = 0; // 0: NONE
            grid[i][j].aState = UNSHOT;
        }
    }
}

/*
 * Reads a line from the socket.
 */
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

/*
 * Displays both own and opponent's grids side by side.
 */
void displayGrids(Cell own_grid[GRID_SIZE][GRID_SIZE], Cell opponent_grid[GRID_SIZE][GRID_SIZE], Cursor cursor, bool your_turn, bool attack_phase) {
    clear();

    /* Display Own Grid */
    mvprintw(0, 2, "Your Grid");
    /* Column Indices */
    mvprintw(1, 2, "  ");
    for (int i = 0; i < GRID_SIZE; i++) {
        printw(" %d", i + 1);
    }
    printw("\n");

    for (int i = 0; i < GRID_SIZE; i++) {
        /* Row Indices */
        mvprintw(2 + i, 2, "%d ", i + 1);
        for (int j = 0; j < GRID_SIZE; j++) {
            char display_char;
            if (own_grid[i][j].aShip > 0) {
                if (own_grid[i][j].aState == UNSHOT)
                    display_char = 'S'; // Ship
                else if (own_grid[i][j].aState == HIT)
                    display_char = 'H'; // Hit
                else if (own_grid[i][j].aState == SUNK)
                    display_char = 'X'; // Sunk
            } else {
                if (own_grid[i][j].aState == MISS)
                    display_char = 'M'; // Miss
                else
                    display_char = '~'; // Water
            }

            /* Apply Color */
            switch (display_char) {
            case 'S':
                attron(COLOR_PAIR(1)); // Ship Color
                break;
            case 'H':
                attron(COLOR_PAIR(2)); // Hit Color
                break;
            case 'M':
                attron(COLOR_PAIR(3)); // Miss Color
                break;
            case 'X':
                attron(COLOR_PAIR(4)); // Sunk Color
                break;
            default:
                attron(COLOR_PAIR(5)); // Water Color
                break;
            }

            printw("%c ", display_char);
            /* Reset Color */
            attroff(COLOR_PAIR(1) | COLOR_PAIR(2) | COLOR_PAIR(3) | COLOR_PAIR(4) | COLOR_PAIR(5));
        }
        printw("\n");
    }

    /* Display Opponent's Grid */
    mvprintw(0, 2 + (GRID_SIZE * 2 + 10), "Opponent's Grid");
    /* Column Indices */
    mvprintw(1, 2 + (GRID_SIZE * 2 + 10), "  ");
    for (int i = 0; i < GRID_SIZE; i++) {
        printw(" %d", i + 1);
    }
    printw("\n");

    for (int i = 0; i < GRID_SIZE; i++) {
        /* Row Indices */
        mvprintw(2 + i, 2 + (GRID_SIZE * 2 + 10), "%d ", i + 1);
        for (int j = 0; j < GRID_SIZE; j++) {
            char display_char = '~'; // Default to water

            if (opponent_grid != NULL) {
                if (opponent_grid[i][j].aState == HIT)
                    display_char = 'X'; // Hit
                else if (opponent_grid[i][j].aState == MISS)
                    display_char = 'O'; // Miss
            }

            /* Highlight Cursor Position */
            if (attack_phase && your_turn && i == cursor.y && j == cursor.x) {
                attron(A_REVERSE); // Highlight
                printw("%c ", display_char);
                attroff(A_REVERSE);
            } else {
                /* Apply Color */
                if (display_char == 'X')
                    attron(COLOR_PAIR(2)); // Hit Color
                else if (display_char == 'O')
                    attron(COLOR_PAIR(3)); // Miss Color
                else
                    attron(COLOR_PAIR(5)); // Water Color

                printw("%c ", display_char);
                /* Reset Color */
                attroff(COLOR_PAIR(2) | COLOR_PAIR(3) | COLOR_PAIR(5));
            }
        }
        printw("\n");
    }

    /* Display Turn Information */
    if (attack_phase) {
        if (your_turn) {
            mvprintw(GRID_SIZE + 3, 2, "Your turn: Use arrow keys to move the cursor and press Enter to attack.");
        } else {
            mvprintw(GRID_SIZE + 3, 2, "Opponent's turn: Waiting for their move...");
        }
    } else {
        mvprintw(GRID_SIZE + 3, 2, "Ship Placement Phase.");
    }

    refresh();
}

/*
 * Handles ship placement in multiplayer mode using cursor movement.
 */
void placeShipsMultiplayer(Cell own_grid[GRID_SIZE][GRID_SIZE], Ship ships[SHIP_NUM]) {
    Cursor cursor = {0, 0};
    int orientation = 0; // 0: Horizontal, 1: Vertical

    for (int i = 0; i < SHIP_NUM; i++) {
        bool placed = false;
        while (!placed) {
            /* Display Grids */
            displayGrids(own_grid, NULL, cursor, true, false);
            /* Display Ship Information */
            mvprintw(GRID_SIZE + 1, 0, "Placing Ship: %s (Size: %d)", ships[i].name, ships[i].size);
            mvprintw(GRID_SIZE + 2, 0, "Orientation: %s", orientation == 0 ? "Horizontal" : "Vertical");
            mvprintw(GRID_SIZE + 3, 0, "Change Orientation: 'o' = Horizontal, 'v' = Vertical");
            mvprintw(GRID_SIZE + 4, 0, "Use Arrow Keys to move, Enter to place ship.");

            int ch = getch();
            switch (ch) {
            case KEY_UP:
                if (cursor.y > 0)
                    cursor.y--;
                break;
            case KEY_DOWN:
                if (cursor.y < GRID_SIZE - 1)
                    cursor.y++;
                break;
            case KEY_LEFT:
                if (cursor.x > 0)
                    cursor.x--;
                break;
            case KEY_RIGHT:
                if (cursor.x < GRID_SIZE - 1)
                    cursor.x++;
                break;
            case 'o':
            case 'O':
                orientation = 0;
                break;
            case 'v':
            case 'V':
                orientation = 1;
                break;
            case '\n':
            case KEY_ENTER: {
                int x = cursor.x;
                int y = cursor.y;
                int dir = orientation;
                int canPlace = 1;

                for (int j = 0; j < ships[i].size; j++) {
                    int nx = x + (dir == 0 ? j : 0);
                    int ny = y + (dir == 1 ? j : 0);

                    if (nx >= GRID_SIZE || ny >= GRID_SIZE || own_grid[ny][nx].aShip != 0) {
                        canPlace = 0;
                        break;
                    }
                }

                if (canPlace) {
                    for (int j = 0; j < ships[i].size; j++) {
                        int nx = x + (dir == 0 ? j : 0);
                        int ny = y + (dir == 1 ? j : 0);
                        own_grid[ny][nx].aShip = ships[i].size; // Place ship
                    }
                    placed = true;
                    write_log("Placed ship: %s at (%d, %d) Orientation: %s\n", ships[i].name, cursor.x + 1, cursor.y + 1, dir == 0 ? "Horizontal" : "Vertical");
                } else {
                    mvprintw(GRID_SIZE + 5, 0, "Invalid placement. Choose a different location.");
                    write_log("Invalid placement attempt at (%d, %d)\n", cursor.x + 1, cursor.y + 1);
                    refresh();
                    sleep(2);
                }
            } break;
            default:
                break;
            }
        }
    }
}

/*
 * Sends the player's grid to the server.
 */
void sendGridToServer(Cell own_grid[GRID_SIZE][GRID_SIZE]) {
    char buffer[GRID_SIZE * GRID_SIZE + 1];
    int index = 0;

    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            buffer[index++] = own_grid[i][j].aShip > 0 ? 'S' : '~';
        }
    }
    buffer[index] = '\0'; // Null-terminate string

    int ret = send(sock_fd, buffer, strlen(buffer), 0);
    if (ret < 0) {
        mvprintw(GRID_SIZE + 6, 0, "Failed to send grid information: %s", strerror(errno));
        write_log("Failed to send grid information: %s\n", strerror(errno));
        refresh();
        sleep(2);
        return;
    }
    mvprintw(GRID_SIZE + 6, 0, "Sent grid information to server. Bytes sent: %d", ret);
    write_log("Sent grid information: %s, Bytes: %d\n", buffer, ret);
    mvprintw(GRID_SIZE + 7, 0, "Waiting for opponent to place their ships...");
    write_log("Waiting for opponent to place ships\n");
    refresh();
    sleep(2);
}

/*
 * Handles user input and game state updates in multiplayer mode.
 */
void inputLoopMultiplayer(Cell own_grid[GRID_SIZE][GRID_SIZE], Cell opponent_grid[GRID_SIZE][GRID_SIZE]) {
    Cursor cursor = {0, 0}; // Initial cursor position
    bool your_turn = false;
    bool attack_phase = true; // After ship placement
    bool running = true;
    fd_set read_fds;
    struct timeval tv;

    /* Initialize opponent grid to all UNSHOT */
    initGrid(opponent_grid);

    /* Set getch to non-blocking */
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    while (running) {
        /* Initialize fd_set */
        FD_ZERO(&read_fds);
        FD_SET(sock_fd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        /* Set timeout (0.1 seconds) */
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int activity = select(sock_fd + 1, &read_fds, NULL, NULL, &tv);

        if (activity < 0 && errno != EINTR) {
            mvprintw(GRID_SIZE + 8, 0, "Select error: %s", strerror(errno));
            write_log("Select error: %s\n", strerror(errno));
            refresh();
            break;
        }

        /* Handle data from server */
        if (FD_ISSET(sock_fd, &read_fds)) {
            /* Receive server message */
            char buf_read[256];
            ssize_t ret = readLine(sock_fd, buf_read, sizeof(buf_read));
            if (ret > 0) {
                write_log("Received from server: %s", buf_read);

                /* Handle turn-related messages */
                if (strcmp(buf_read, "YOUR_TURN\n") == 0) {
                    your_turn = true;
                    mvprintw(GRID_SIZE + 9, 0, "Your turn.");
                } else if (strcmp(buf_read, "OPPONENT_TURN\n") == 0) {
                    your_turn = false;
                    mvprintw(GRID_SIZE + 9, 0, "Opponent's turn.");
                }
                /* Handle attack results */
                else if (strncmp(buf_read, "Hit", 3) == 0 || strncmp(buf_read, "Miss", 4) == 0) {
                    int x, y;
                    sscanf(buf_read, "%*s (%d %d)", &x, &y);
                    x -= 1; // 0-based index
                    y -= 1;
                    if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
                        if (strncmp(buf_read, "Hit", 3) == 0) {
                            opponent_grid[y][x].aState = HIT;
                        } else if (strncmp(buf_read, "Miss", 4) == 0) {
                            opponent_grid[y][x].aState = MISS;
                        }
                    }
                    mvprintw(GRID_SIZE + 10, 0, "Attack result: %s", buf_read);
                    write_log("Updated opponent grid at (%d, %d): %s", x + 1, y + 1, buf_read);
                }
                /* Handle game over messages */
                else if (strncmp(buf_read, "You won", 7) == 0 || strncmp(buf_read, "You lost", 8) == 0) {
                    mvprintw(GRID_SIZE + 11, 0, "%s", buf_read);
                    write_log("Game over: %s", buf_read);
                    running = false;
                }
                /* Handle opponent's attack */
                else {
                    if (strncmp(buf_read, "Attack", 6) == 0) {
                        int x, y;
                        sscanf(buf_read, "Attack (%d %d)", &x, &y);
                        x -= 1;
                        y -= 1;
                        /* Receive attack result from server */
                        char attack_result[256];
                        ret = readLine(sock_fd, attack_result, sizeof(attack_result));
                        if (ret > 0) {
                            write_log("Opponent's attack result: %s", attack_result);
                            if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
                                if (strncmp(attack_result, "Hit", 3) == 0) {
                                    own_grid[y][x].aState = HIT;
                                } else if (strncmp(attack_result, "Miss", 4) == 0) {
                                    own_grid[y][x].aState = MISS;
                                }
                            }
                            mvprintw(GRID_SIZE + 12, 0, "Opponent's attack result: %s", attack_result);
                            write_log("Updated own grid at (%d, %d): %s", x + 1, y + 1, attack_result);
                        }
                    }
                }
                refresh();
            } else if (ret == 0) {
                /* Server disconnected */
                mvprintw(GRID_SIZE + 12, 0, "Server disconnected.");
                write_log("Server disconnected.\n");
                refresh();
                break;
            }
        }

        /* Handle user input */
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (attack_phase && your_turn) {
                int ch = getch();
                switch (ch) {
                case KEY_UP:
                    if (cursor.y > 0)
                        cursor.y--;
                    break;
                case KEY_DOWN:
                    if (cursor.y < GRID_SIZE - 1)
                        cursor.y++;
                    break;
                case KEY_LEFT:
                    if (cursor.x > 0)
                        cursor.x--;
                    break;
                case KEY_RIGHT:
                    if (cursor.x < GRID_SIZE - 1)
                        cursor.x++;
                    break;
                case '\n':
                case KEY_ENTER: {
                    int x = cursor.x + 1; // 1-based index
                    int y = cursor.y + 1;
                    /* Check if already attacked */
                    if (opponent_grid[y - 1][x - 1].aState != UNSHOT) {
                        mvprintw(GRID_SIZE + 13, 0, "Already attacked this location. Choose another.");
                        write_log("Attempted to attack already attacked location (%d, %d)\n", x, y);
                        refresh();
                        sleep(1);
                        break;
                    }
                    /* Send attack coordinates to server */
                    char buf_write[20];
                    sprintf(buf_write, "(%d %d)\n", x, y);
                    int write_ret = write(sock_fd, buf_write, strlen(buf_write));
                    if (write_ret < (int)strlen(buf_write)) {
                        mvprintw(GRID_SIZE + 13, 0, "Send error (Bytes sent=%d, Error=%s)", write_ret, strerror(errno));
                        write_log("Send error: %s\n", strerror(errno));
                        refresh();
                        sleep(2);
                        continue;
                    }
                    write_log("Sent attack coordinates: (%d, %d)\n", x, y);
                    your_turn = false;
                    mvprintw(GRID_SIZE + 14, 0, "Attack sent. Waiting for result...");
                    refresh();
                } break;
                case 'q':
                case 'Q':
                    running = false;
                    write_log("Game terminated by user.\n");
                    break;
                default:
                    break;
                }
            }
        }
    }

    /* Update and display grids */
    displayGrids(own_grid, opponent_grid, cursor, your_turn, attack_phase);
}

/*
 * Main game loop for multiplayer mode.
 */
void gameLoopNcursesMultiplayer(Cell own_grid[GRID_SIZE][GRID_SIZE], Cell opponent_grid[GRID_SIZE][GRID_SIZE]) {
    inputLoopMultiplayer(own_grid, opponent_grid);
}

/*
 * Handles ship placement in multiplayer mode with cursor.
 */
void placeShipsMultiplayer(Cell own_grid[GRID_SIZE][GRID_SIZE], Ship ships[SHIP_NUM]) {
    Cursor cursor = {0, 0};
    int orientation = 0; // 0: Horizontal, 1: Vertical

    for (int i = 0; i < SHIP_NUM; i++) {
        bool placed = false;
        while (!placed) {
            /* Display Grids */
            displayGrids(own_grid, NULL, cursor, true, false);
            /* Display Ship Information */
            mvprintw(GRID_SIZE + 1, 0, "Placing Ship: %s (Size: %d)", ships[i].name, ships[i].size);
            mvprintw(GRID_SIZE + 2, 0, "Orientation: %s", orientation == 0 ? "Horizontal" : "Vertical");
            mvprintw(GRID_SIZE + 3, 0, "Change Orientation: 'o' = Horizontal, 'v' = Vertical");
            mvprintw(GRID_SIZE + 4, 0, "Use Arrow Keys to move, Enter to place ship.");

            int ch = getch();
            switch (ch) {
            case KEY_UP:
                if (cursor.y > 0)
                    cursor.y--;
                break;
            case KEY_DOWN:
                if (cursor.y < GRID_SIZE - 1)
                    cursor.y++;
                break;
            case KEY_LEFT:
                if (cursor.x > 0)
                    cursor.x--;
                break;
            case KEY_RIGHT:
                if (cursor.x < GRID_SIZE - 1)
                    cursor.x++;
                break;
            case 'o':
            case 'O':
                orientation = 0;
                break;
            case 'v':
            case 'V':
                orientation = 1;
                break;
            case '\n':
            case KEY_ENTER: {
                int x = cursor.x;
                int y = cursor.y;
                int dir = orientation;
                int canPlace = 1;

                for (int j = 0; j < ships[i].size; j++) {
                    int nx = x + (dir == 0 ? j : 0);
                    int ny = y + (dir == 1 ? j : 0);

                    if (nx >= GRID_SIZE || ny >= GRID_SIZE || own_grid[ny][nx].aShip != 0) {
                        canPlace = 0;
                        break;
                    }
                }

                if (canPlace) {
                    for (int j = 0; j < ships[i].size; j++) {
                        int nx = x + (dir == 0 ? j : 0);
                        int ny = y + (dir == 1 ? j : 0);
                        own_grid[ny][nx].aShip = ships[i].size; // Place ship
                    }
                    placed = true;
                    write_log("Placed ship: %s at (%d, %d) Orientation: %s\n", ships[i].name, cursor.x + 1, cursor.y + 1, dir == 0 ? "Horizontal" : "Vertical");
                } else {
                    mvprintw(GRID_SIZE + 5, 0, "Invalid placement. Choose a different location.");
                    write_log("Invalid placement attempt at (%d, %d)\n", cursor.x + 1, cursor.y + 1);
                    refresh();
                    sleep(2);
                }
            } break;
            default:
                break;
            }
        }
    }
}

/*
 * Starts the multiplayer mode.
 */
void start_multiplayer() {
    // Prompt user for server details
    clear();
    int mid_y = LINES / 2 - 4;
    int mid_x = (COLS - strlen("Enter Server IP:")) / 2;

    char server_ip[100];
    int server_port;

    mvprintw(mid_y, mid_x, "Enter Server IP: ");
    echo();
    scanw("%s", server_ip);
    noecho();

    mvprintw(mid_y + 2, mid_x, "Enter Server Port: ");
    echo();
    scanw("%d", &server_port);
    noecho();

    /* Initialize socket */
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        endwin();
        fprintf(stderr, "Socket creation error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    /* Convert IP address */
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        endwin();
        fprintf(stderr, "Invalid address/ Address not supported\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    /* Connect to server */
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        endwin();
        fprintf(stderr, "Connection Failed: %s\n", strerror(errno));
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    /* Initialize ncurses colors */
    start_color();
    init_pair(1, COLOR_BLUE, COLOR_BLACK);    // Ship
    init_pair(2, COLOR_RED, COLOR_BLACK);     // Hit
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);  // Miss
    init_pair(4, COLOR_MAGENTA, COLOR_BLACK); // Sunk
    init_pair(5, COLOR_CYAN, COLOR_BLACK);    // Water

    /* Log connection */
    write_log("Connected to server %s:%d\n", server_ip, server_port);
    mvprintw(GRID_SIZE + 1, 0, "Successfully connected to server.");
    refresh();
    sleep(1);

    /* Initialize grids */
    Cell own_grid[GRID_SIZE][GRID_SIZE];
    Cell opponent_grid[GRID_SIZE][GRID_SIZE];
    initGrid(own_grid);
    initGrid(opponent_grid);

    /* Setup ships */
    Ship ships_multiplayer[SHIP_NUM] = {
        {"Aircraft Carrier", 5},
        {"Battleship", 4},
        {"Cruiser", 3},
        {"Submarine", 3},
        {"Destroyer", 2}};
    setup_ships(ships_multiplayer);

    /* Place ships */
    placeShipsMultiplayer(own_grid, ships_multiplayer);

    /* Send grid to server */
    sendGridToServer(own_grid);

    /* Enter game loop */
    gameLoopNcursesMultiplayer(own_grid, opponent_grid);

    /* Cleanup */
    endwin();
    write_log("Multiplayer game ended.\n");
    close(sock_fd);
    if (log_file != NULL) {
        fclose(log_file);
    }
}

/* ==========================
 * Main Function
 * ========================== */

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");

    /* Initialize ncurses */
    initscr();            // Start ncurses mode
    start_color();        // Enable color
    cbreak();             // Disable line buffering
    keypad(stdscr, TRUE); // Enable special keys

    /* Define color pairs */
    init_pair(1, COLOR_WHITE, COLOR_BLACK); // Default color
    init_pair(2, COLOR_WHITE, COLOR_BLUE);  // Background
    init_pair(3, COLOR_BLUE, COLOR_BLACK);  // Additional colors can be defined as needed

    /* Display startup screen */
    char *title_frames[] = {
        " _             _    _    _              _      _        ",
        "| |           | |  | |  | |            | |    (_)       ",
        "| |__    __ _ | |_ | |_ | |  ___   ___ | |__   _  _ __  ",
        "| '_ \\  / _` || __|| __|| | / _ \\ / __|| '_ \\ | || '_ \\ ",
        "| |_) || (_| || |_ | |_ | ||  __/ \\__ \\| | | || || |_) |",
        "|_.__/  \\__,_| \\__| \\__||_| \\___| |___/|_| |_||_|| .__/ ",
        "                                                 | |    ",
        "                                                 |_|    ",
        NULL};

    char *new_ascii_art[] =
        {
            "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠛⢛⠛⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
            "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡇⠀⢐⣮⡆⠀⢠⠀⠀⢠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
            "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡸⡿⣀⢨⠿⡇⣐⢿⢆⠀⢐⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
            "⠀⠀⠀⠀⠀⠀⠀⠀⠀⣤⢯⡯⡯⣯⢯⡯⣯⢯⡯⣄⠨⠀⢀⣐⣛⣃⣈⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
            "⠀⠀⣠⣀⣀⣀⣀⣒⣛⣛⣓⣛⣛⣋⣛⣋⣛⣛⣚⣛⣛⣛⣜⣻⣻⣻⣻⣤⣽⣿⣥⣤⣤⣤⣤⣴⡔⠀⠀",
            "⠀⠀⠈⠛⣻⣛⣟⣻⣛⣻⣛⣛⣟⣛⣟⣻⣛⣟⣛⣟⣻⣛⣛⣛⣛⣛⣛⣏⣟⣝⣛⣛⣛⣗⣜⡟⠀⠀⠀",
            "⠀⠀⠀⠀⠀⠉⠉⠁⠉⠉⠉⠉⠉⠉⠉⠉⠈⠉⠉⠁⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠁⠁⠀⠀⠀",
            "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
            NULL};

    int frame_delay = 200; // Delay between frames in milliseconds
    int i = 0;
    while (title_frames[i] || new_ascii_art[i]) {
        if (title_frames[i]) {
            mvprintw(LINES / 2 - 4 + i, (COLS - strlen(title_frames[i])) / 2, "%s", title_frames[i]);
        }
        if (new_ascii_art[i]) {
            mvprintw(LINES / 2 + 8 + i, (COLS - strlen(new_ascii_art[i])) / 2, "%s", new_ascii_art[i]);
        }
        refresh();
        usleep(frame_delay * 1000); // Convert to microseconds
        i++;
    }

    /* Final ASCII Art Display */
    i = 0;
    while (title_frames[i]) {
        mvprintw(LINES / 2 - 4 + i, (COLS - strlen(title_frames[i])) / 2, "%s", title_frames[i]);
        i++;
    }

    /* Prompt to start the game */
    int enter_msg_length = strlen("< PRESS ENTER TO START >");
    mvprintw(LINES / 2 + 5, (COLS - enter_msg_length - 2) / 2, "< PRESS ENTER TO START >");
    refresh();

    /* Wait for Enter key */
    while (getch() != '\n')
        ;

    /* Game mode selection loop */
    int choice;
    while (1) {
        clear();
        attron(COLOR_PAIR(1));

        /* Draw a white box in the center */
        int box_height = 10;
        int box_width = 25;
        int box_start_y = (LINES - box_height) / 2;
        int box_start_x = (COLS - box_width) / 2;

        for (int i = box_start_y; i < box_start_y + box_height; i++) {
            mvprintw(i, box_start_x, "|");
            mvprintw(i, box_start_x + box_width - 1, "|");
        }
        for (int i = box_start_x; i < box_start_x + box_width; i++) {
            mvprintw(box_start_y, i, "-");
            mvprintw(box_start_y + box_height - 1, i, "-");
        }
        mvprintw(box_start_y, box_start_x, "+");
        mvprintw(box_start_y, box_start_x + box_width - 1, "+");
        mvprintw(box_start_y + box_height - 1, box_start_x, "+");
        mvprintw(box_start_y + box_height - 1, box_start_x + box_width - 1, "+");

        /* Display menu options */
        mvprintw(box_start_y + 1, box_start_x + 2, "Battleship Game");
        mvprintw(box_start_y + 3, box_start_x + 2, "Select Mode");
        mvprintw(box_start_y + 5, box_start_x + 2, "1. Singleplayer");
        mvprintw(box_start_y + 6, box_start_x + 2, "2. Multiplayer");
        mvprintw(box_start_y + 7, box_start_x + 2, "3. Exit");
        mvprintw(box_start_y + 9, box_start_x + 2, "Choice: ");
        refresh();

        /* Get user choice */
        echo();
        scanw("%d", &choice);
        noecho();

        /* Handle user choice */
        if (choice == 1) {
            start_singleplayer();
        } else if (choice == 2) {
            start_multiplayer();
        } else if (choice == 3) {
            break;
        } else {
            mvprintw(box_start_y + 11, box_start_x + 2, "Invalid choice. Please try again.");
            refresh();
            sleep(2);
        }
    }

    endwin(); // End ncurses mode
    return 0;
}
