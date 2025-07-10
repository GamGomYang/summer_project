#include <arpa/inet.h>
#include <errno.h>
#include <locale.h>
#include <ncursesw/ncurses.h>
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

typedef enum {
    UNSHOT,
    MISS,
    HIT,
    SUNK
} CellState;

typedef struct {
    int aShip;
    CellState aState;
} Cell;

typedef struct {
    int x;
    int y;
} Cursor;

char player_board[GRID_SIZE][GRID_SIZE];
char enemy_board[GRID_SIZE][GRID_SIZE];
Ship player_ships[SHIP_NUM];
Ship enemy_ships[SHIP_NUM];

char *id = 0;
short sport = 0;
int sock_fd = 0;
FILE *log_file = NULL;
void init_boards();
void display_board(char board[GRID_SIZE][GRID_SIZE], int reveal, int start_y);
void display_game_status();
void display_error_message(const char *message);
void display_attack_result(int result);
void display_game_result(const char *result_message);

// 한글 문자열의 실제 화면 표시 길이 계산
int get_display_width(const char *str);

void setup_ships(Ship ships[]);
int place_ships(Ship ships[], char board[GRID_SIZE][GRID_SIZE]);
void ai_place_ships(Ship ships[], char board[GRID_SIZE][GRID_SIZE]);

int attack(int x, int y, char board[GRID_SIZE][GRID_SIZE], Ship ships[]);
int check_game_over(Ship ships[]);
void ai_attack(char board[GRID_SIZE][GRID_SIZE], Ship ships[], int difficulty);

void start_singleplayer();
void start_multiplayer();

void clear_input_buffer();
void handle_winch(int sig);

void write_log(const char *format, ...);
void initGrid(Cell grid[GRID_SIZE][GRID_SIZE]);
ssize_t readLine(int sockfd, char *buffer, size_t maxlen);
void displayGrids(Cell own_grid[GRID_SIZE][GRID_SIZE], Cell opponent_grid[GRID_SIZE][GRID_SIZE], Cursor cursor, bool your_turn, bool attack_phase);
void placeShipsMultiplayer(Cell own_grid[GRID_SIZE][GRID_SIZE], Ship ships[SHIP_NUM]);
void sendGridToServer(Cell own_grid[GRID_SIZE][GRID_SIZE]);
void inputLoopMultiplayer(Cell own_grid[GRID_SIZE][GRID_SIZE], Cell opponent_grid[GRID_SIZE][GRID_SIZE]);
void gameLoopNcursesMultiplayer(Cell own_grid[GRID_SIZE][GRID_SIZE], Cell opponent_grid[GRID_SIZE][GRID_SIZE]);

int get_display_width(const char *str) {
    int width = 0;
    while (*str) {
        unsigned char c = (unsigned char)*str;
        if (c < 0x80) {
            // ASCII 문자
            width += 1;
        } else if (c >= 0xE0) {
            // 한글 문자 (UTF-8 3바이트)
            width += 2;
            str += 2; // 3바이트 건너뛰기
        } else {
            // 기타 멀티바이트 문자
            width += 1;
        }
        str++;
    }
    return width;
}

void init_boards() {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            player_board[i][j] = '~';
            enemy_board[i][j] = '~';
        }
    }
}
void display_board(char board[GRID_SIZE][GRID_SIZE], int reveal, int start_y) {
    // 박스 형태로 보드 출력, 행/열 번호 1부터 시작
    int board_width = GRID_SIZE * 4 + 5; // 각 셀: | X | = 4칸, 좌측 여백 포함
    int board_height = GRID_SIZE * 2 + 2; // 각 행: 셀+구분선, 헤더+구분선

    // 화면 크기 검증
    if (COLS < board_width) {
        mvprintw(start_y, 0, "Screen too narrow! Need at least %d columns", board_width);
        refresh();
        return;
    }
    if (LINES < start_y + board_height) {
        mvprintw(start_y, 0, "Screen too short! Need at least %d lines", start_y + board_height);
        refresh();
        return;
    }

    int start_x = (COLS - board_width) / 2;
    if (start_x < 0) start_x = 0;

    attron(COLOR_PAIR(2));

    // 열 번호 (1~10)
    mvprintw(start_y, start_x + 5, "");
    for (int i = 0; i < GRID_SIZE; i++) {
        printw(" %2d ", i + 1);
    }
    // 윗줄 구분선
    mvprintw(start_y + 1, start_x + 4, "+");
    for (int i = 0; i < GRID_SIZE; i++) {
        printw("---+");
    }

    // 각 행 출력
    for (int i = 0; i < GRID_SIZE; i++) {
        // 행 번호 (1~10)
        mvprintw(start_y + 2 + i * 2, start_x, "%2d |", i + 1);
        for (int j = 0; j < GRID_SIZE; j++) {
            char c = board[i][j];
            if (c == 'S' && !reveal) c = '~';
            if (c == 'X') {
                attron(COLOR_PAIR(2)); // 빨간색
                printw(" %c |", c);
                attroff(COLOR_PAIR(2));
            } else {
                printw(" %c |", c);
            }
        }
        // 행 구분선
        mvprintw(start_y + 3 + i * 2, start_x + 4, "+");
        for (int k = 0; k < GRID_SIZE; k++) {
            printw("---+");
        }
    }
    attroff(COLOR_PAIR(2));
    refresh();
}

void display_game_status() {
    clear();

    int board_height = GRID_SIZE + 1; // 헤더 + 각 행
    int gap = 2;                      // 두 보드 사이 간격을 3으로 증가
    int total_height = board_height * 2 + gap;

    // 화면 크기 검증
    if (LINES < total_height + 2) {
        mvprintw(LINES / 2, (COLS - 30) / 2, "화면이 너무 작아서 보드를 표시할 수 없습니다.");
        refresh();
        return;
    }

    // 중앙 정렬
    int start_y = (LINES - total_height) / 2;
    if (start_y < 0)
        start_y = 0;
    int player_board_y = start_y;
    int enemy_board_y = start_y + board_height + gap;

    // 제목 중앙 정렬
    int title1_x = (COLS - get_display_width("당신의 보드:")) / 2;
    int title2_x = (COLS - get_display_width("적의 보드:")) / 2;

    mvprintw(player_board_y, title1_x, "당신의 보드:");
    display_board(player_board, 1, player_board_y + 1);

    mvprintw(enemy_board_y, title2_x, "적의 보드:");
    display_board(enemy_board, 0, enemy_board_y + 1);

    // 입력 안내 메시지 (보드 바로 아래)
    int msg_y = enemy_board_y + board_height + 1;
    if (msg_y < LINES - 1) {
        const char *msg = "공격 좌표를 입력하세요 (x y):";
        int msg_x = (COLS - get_display_width(msg)) / 2;
        mvprintw(msg_y, msg_x, "%s", msg);
    }

    refresh();
}

void display_error_message(const char *message) {
    int mid_x = (COLS - strlen(message)) / 2;
    mvprintw(4 * GRID_SIZE + 7, mid_x, "%s", message);
    refresh();
}

void display_attack_result(int result) {
    int mid_x;
    if (result == 1) {
        mid_x = (COLS - strlen("명중!")) / 2;
        mvprintw(4 * GRID_SIZE + 7, mid_x, "명중!");
    } else if (result == 0) {
        mid_x = (COLS - strlen("빗나감!")) / 2;
        mvprintw(4 * GRID_SIZE + 7, mid_x, "빗나감!");
    } else if (result == -1) {
        mid_x = (COLS - strlen("이미 공격한 위치입니다.")) / 2;
        mvprintw(4 * GRID_SIZE + 7, mid_x, "이미 공격한 위치입니다.");
    } else {
        mid_x = (COLS - strlen("좌표가 범위를 벗어났습니다.")) / 2;
        mvprintw(4 * GRID_SIZE + 7, mid_x, "좌표가 범위를 벗어났습니다.");
    }
    refresh();
    sleep(1);
}

void display_game_result(const char *result_message) {
    int mid_x = (COLS - strlen(result_message)) / 2;
    mvprintw(4 * GRID_SIZE + 9, mid_x, "%s", result_message);
    refresh();
    sleep(3);
}

void setup_ships(Ship ships[]) {
    strcpy(ships[0].name, "Carrier");
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

int place_ships(Ship ships[], char board[GRID_SIZE][GRID_SIZE]) {
    int x, y, orientation;
    for (int i = 0; i < SHIP_NUM; i++) {
        while (1) {
            clear();
            display_board(board, 1, 0);
            int mid_x = (COLS - 20) / 2;
            int mid_y = (LINES - GRID_SIZE - 6) / 2;
            mvprintw(mid_y + GRID_SIZE + 2, mid_x, "배를 배치하세요: %s (크기: %d)", ships[i].name, ships[i].size);
            mvprintw(mid_y + GRID_SIZE + 3, mid_x, "시작 위치 (x y): ");
            refresh();

            echo();
            if (scanw("%d %d", &x, &y) != 2) {
                mvprintw(mid_y + GRID_SIZE + 5, mid_x, "유효하지 않은 좌표입니다. 다시 시도하세요.");
                noecho();
                clear_input_buffer();
                refresh();
                sleep(1);
                continue;
            }
            noecho();
            // 1부터 입력받으므로 내부 인덱스 변환
            x -= 1;
            y -= 1;

            if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) {
                mvprintw(mid_y + GRID_SIZE + 5, mid_x, "좌표가 범위를 벗어났습니다.");
                refresh();
                sleep(1);
                continue;
            }

            mvprintw(mid_y + GRID_SIZE + 4, mid_x, "방향 (0: 수평, 1: 수직): ");
            refresh();

            echo();
            if (scanw("%d", &orientation) != 1) {
                mvprintw(mid_y + GRID_SIZE + 5, mid_x, "유효하지 않은 방향입니다. 다시 시도하세요.");
                noecho();
                clear_input_buffer();
                refresh();
                sleep(1);
                continue;
            }
            noecho();

            if (orientation != 0 && orientation != 1) {
                mvprintw(mid_y + GRID_SIZE + 5, mid_x, "방향은 0 또는 1이어야 합니다.");
                refresh();
                sleep(1);
                continue;
            }

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
                mvprintw(mid_y + GRID_SIZE + 5, mid_x, "여기에 배를 배치할 수 없습니다. 다른 위치를 선택하세요.");
                refresh();
                sleep(1);
            }
        }
    }
    return 0;
}

int check_game_over(Ship ships[]) {
    for (int i = 0; i < SHIP_NUM; i++) {
        if (ships[i].hits < ships[i].size) {
            return 0; // Game not over
        }
    }
    return 1; // All ships sunk
}

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

void ai_attack(char board[GRID_SIZE][GRID_SIZE], Ship ships[], int difficulty) {
    static int hit_stack[GRID_SIZE * GRID_SIZE][2];
    static int stack_top = -1;

    int x, y, result;

    if (difficulty == 1) {
        do {
            x = rand() % GRID_SIZE;
            y = rand() % GRID_SIZE;
            result = attack(x, y, board, ships);
        } while (result == -1 || result == -2);
    } else if (difficulty == 2 || difficulty == 3) {
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

int attack(int x, int y, char board[GRID_SIZE][GRID_SIZE], Ship ships[]) {
    if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) {
        return -2; // Out of range
    }

    if (board[y][x] == 'S') {
        board[y][x] = 'X';

        for (int i = 0; i < SHIP_NUM; i++) {
            for (int j = 0; j < ships[i].size; j++) {
                if (ships[i].x[j] == x && ships[i].y[j] == y) {
                    ships[i].hits++;
                    break;
                }
            }
        }

        return 1;
    } else if (board[y][x] == '~') {
        board[y][x] = 'O';
        return 0;
    } else {
        return -1;
    }
}

void clear_input_buffer() {
    int ch;
    while ((ch = getch()) != '\n' && ch != EOF)
        ;
}

void handle_winch(int sig) {
    endwin();
    refresh();
    clear();
    display_game_status();
}

void start_singleplayer() {
    int difficulty;
    clear();
    int mid_y = LINES / 2 - 3;
    int mid_x = (COLS - strlen("난이도 선택:")) / 2;

    mvprintw(mid_y, mid_x, "난이도 선택:");
    mvprintw(mid_y + 2, mid_x, "1. 쉬움");
    mvprintw(mid_y + 3, mid_x, "2. 보통");
    mvprintw(mid_y + 4, mid_x, "3. 어려움");
    mvprintw(mid_y + 6, mid_x, "선택: ");
    refresh();

    echo();
    if (scanw("%d", &difficulty) != 1) {
        mvprintw(mid_y + 8, mid_x, "유효한 번호를 입력하세요.");
        noecho();
        clear_input_buffer();
        refresh();
        sleep(2);
        return;
    }
    noecho();

    if (difficulty < 1 || difficulty > 3) {
        mvprintw(mid_y + 8, mid_x, "잘못된 난이도 선택입니다.");
        refresh();
        sleep(2);
        return;
    }

    init_boards();

    setup_ships(player_ships);
    setup_ships(enemy_ships);

    if (place_ships(player_ships, player_board) != 0) {
        mvprintw(10, 0, "배를 배치하는 데 실패했습니다.");
        refresh();
        sleep(2);
        return;
    }

    ai_place_ships(enemy_ships, enemy_board);

    int game_over = 0;
    int x, y, result;
    while (!game_over) {
        clear();
        display_game_status();

        echo();
        if (scanw("%d %d", &x, &y) != 2) {
            display_error_message("유효한 좌표를 입력하세요.");
            noecho();
            clear_input_buffer();
            sleep(1);
            continue;
        }
        noecho();
        // 1부터 입력받으므로 내부 인덱스 변환
        x -= 1;
        y -= 1;

        result = attack(x, y, enemy_board, enemy_ships);
        display_attack_result(result);

        if (check_game_over(enemy_ships)) {
            display_game_result("당신이 이겼습니다!");
            break;
        }

        ai_attack(player_board, player_ships, difficulty);

        if (check_game_over(player_ships)) {
            display_game_result("당신이 졌습니다.");
            break;
        }
    }
}

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

void initGrid(Cell grid[GRID_SIZE][GRID_SIZE]) {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            grid[i][j].aShip = 0;
            grid[i][j].aState = UNSHOT;
        }
    }
}

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

void displayGrids(Cell own_grid[GRID_SIZE][GRID_SIZE], Cell opponent_grid[GRID_SIZE][GRID_SIZE], Cursor cursor, bool your_turn, bool attack_phase) {
    clear();

    // 화면 크기 계산
    int grid_width = GRID_SIZE * 2 + 3;    // 각 셀: "X " = 2칸 + 행 번호 + 공백
    int grid_height = GRID_SIZE + 2;       // +2는 헤더와 여유 공간
    int total_width = grid_width * 2 + 10; // 두 그리드 + 간격
    int total_height = grid_height + 5;    // +5는 상태 메시지용

    // 화면 크기 검증
    if (COLS < total_width) {
        mvprintw(LINES / 2, 0, "화면이 너무 좁습니다! 최소 %d 열이 필요합니다.", total_width);
        refresh();
        return;
    }

    if (LINES < total_height) {
        mvprintw(LINES / 2, 0, "화면이 너무 짧습니다! 최소 %d 줄이 필요합니다.", total_height);
        refresh();
        return;
    }

    // 그리드 위치 계산 (중앙 정렬)
    int left_grid_x = (COLS - total_width) / 2;
    int right_grid_x = left_grid_x + grid_width + 10;
    int grid_y = 1;

    // 왼쪽 그리드 (내 그리드)
    mvprintw(grid_y, left_grid_x, "당신의 그리드");
    mvprintw(grid_y + 1, left_grid_x, "  ");
    for (int i = 0; i < GRID_SIZE; i++) {
        printw(" %d", i + 1);
    }

    for (int i = 0; i < GRID_SIZE; i++) {
        mvprintw(grid_y + 2 + i, left_grid_x, "%d ", i + 1);
        for (int j = 0; j < GRID_SIZE; j++) {
            char display_char;
            if (own_grid[i][j].aShip > 0) {
                if (own_grid[i][j].aState == UNSHOT)
                    display_char = 'S';
                else if (own_grid[i][j].aState == HIT)
                    display_char = 'H';
                else if (own_grid[i][j].aState == SUNK)
                    display_char = 'X';
            } else {
                if (own_grid[i][j].aState == MISS)
                    display_char = 'M';
                else
                    display_char = '~';
            }

            switch (display_char) {
            case 'S':
                attron(COLOR_PAIR(1));
                break;
            case 'H':
                attron(COLOR_PAIR(2));
                break;
            case 'M':
                attron(COLOR_PAIR(3));
                break;
            case 'X':
                attron(COLOR_PAIR(4));
                break;
            default:
                attron(COLOR_PAIR(5));
                break;
            }

            printw("%c ", display_char);
            attroff(COLOR_PAIR(1) | COLOR_PAIR(2) | COLOR_PAIR(3) | COLOR_PAIR(4) | COLOR_PAIR(5));
        }
    }

    // 오른쪽 그리드 (상대 그리드)
    mvprintw(grid_y, right_grid_x, "상대의 그리드");
    mvprintw(grid_y + 1, right_grid_x, "  ");
    for (int i = 0; i < GRID_SIZE; i++) {
        printw(" %d", i + 1);
    }

    for (int i = 0; i < GRID_SIZE; i++) {
        mvprintw(grid_y + 2 + i, right_grid_x, "%d ", i + 1);
        for (int j = 0; j < GRID_SIZE; j++) {
            char display_char = '~';

            if (opponent_grid != NULL) {
                if (opponent_grid[i][j].aState == HIT) {
                    display_char = 'X';
                    write_log("좌표 (%d, %d)에서 Hit로 처리됨\n", i + 1, j + 1);
                } else if (opponent_grid[i][j].aState == MISS) {
                    display_char = 'O';
                    write_log("좌표 (%d, %d)에서 Miss로 처리됨\n", i + 1, j + 1);
                }
            }

            if (attack_phase && your_turn && i == cursor.y && j == cursor.x) {
                attron(A_REVERSE);
                printw("%c ", display_char);
                attroff(A_REVERSE);
            } else {
                if (display_char == 'X')
                    attron(COLOR_PAIR(2));
                else if (display_char == 'O')
                    attron(COLOR_PAIR(3));
                else
                    attron(COLOR_PAIR(5));

                printw("%c ", display_char);
                attroff(COLOR_PAIR(2) | COLOR_PAIR(3) | COLOR_PAIR(5));
            }
            write_log("상대의 그리드 (%d, %d): %c\n", i + 1, j + 1, display_char);
        }
    }
    write_log("상대의 그리드 출력 완료\n");

    // 상태 메시지 (화면 하단에 배치)
    int status_y = grid_y + grid_height + 2;
    if (attack_phase) {
        if (your_turn) {
            mvprintw(status_y, left_grid_x, "당신의 턴: 화살표 키로 커서를 이동하고 엔터를 눌러 공격하세요.");
        } else {
            mvprintw(status_y, left_grid_x, "상대의 턴: 상대의 움직임을 기다리는 중...");
        }
    } else {
        mvprintw(status_y, left_grid_x, "배치 단계입니다.");
    }

    refresh();
}

void placeShipsMultiplayer(Cell own_grid[GRID_SIZE][GRID_SIZE], Ship ships[SHIP_NUM]) {
    Cursor cursor = {0, 0};
    int orientation = 0; // 0: Horizontal, 1: Vertical

    for (int i = 0; i < SHIP_NUM; i++) {
        bool placed = false;
        while (!placed) {
            displayGrids(own_grid, NULL, cursor, true, false);

            // 화면 크기 계산
            int grid_width = GRID_SIZE * 2 + 3;
            int grid_height = GRID_SIZE + 2;
            int total_width = grid_width * 2 + 10;
            int left_grid_x = (COLS - total_width) / 2;
            int status_y = grid_height + 3;

            // 상태 메시지들을 적절한 위치에 배치
            mvprintw(status_y, left_grid_x, "배 배치 중: %s (크기: %d)", ships[i].name, ships[i].size);
            mvprintw(status_y + 1, left_grid_x, "방향: %s", orientation == 0 ? "수평" : "수직");
            mvprintw(status_y + 2, left_grid_x, "방향 변경: 'o' = 수평, 'v' = 수직");
            mvprintw(status_y + 3, left_grid_x, "화살표 키로 이동하고 엔터를 눌러 배를 배치하세요.");

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
                    mvprintw(status_y + 4, left_grid_x, "유효하지 않은 배치입니다. 다른 위치를 선택하세요.");
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

void sendGridToServer(Cell own_grid[GRID_SIZE][GRID_SIZE]) {
    char buffer[GRID_SIZE * GRID_SIZE + 1];
    int index = 0;

    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            buffer[index++] = own_grid[i][j].aShip > 0 ? 'S' : '~';
        }
    }
    buffer[index] = '\0';

    int ret = send(sock_fd, buffer, strlen(buffer), 0);
    if (ret < 0) {
        mvprintw(LINES - 2, 0, "그리드 정보를 전송하지 못했습니다: %s", strerror(errno));
        write_log("Failed to send grid information: %s\n", strerror(errno));
        refresh();
        sleep(2);
        return;
    }
    mvprintw(LINES - 2, 0, "그리드 정보를 서버에 전송했습니다. 전송된 바이트: %d", ret);
    write_log("Sent grid information: %s, Bytes: %d\n", buffer, ret);
    mvprintw(LINES - 1, 0, "상대의 배 배치를 기다리는 중...");
    write_log("Waiting for opponent to place ships\n");
    refresh();
    sleep(2);
}

void inputLoopMultiplayer(Cell own_grid[GRID_SIZE][GRID_SIZE], Cell opponent_grid[GRID_SIZE][GRID_SIZE]) {
    Cursor cursor = {0, 0};
    bool your_turn = false;
    bool attack_phase = true;
    bool running = true;
    fd_set read_fds;
    struct timeval tv;

    int last_attack_x = -1;
    int last_attack_y = -1;

    initGrid(opponent_grid);

    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    while (running) {
        FD_ZERO(&read_fds);
        FD_SET(sock_fd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int activity = select(sock_fd + 1, &read_fds, NULL, NULL, &tv);

        if (activity < 0 && errno != EINTR) {
            mvprintw(LINES - 4, 0, "선택 오류: %s", strerror(errno));
            write_log("Select error: %s\n", strerror(errno));
            refresh();
            break;
        }

        if (FD_ISSET(sock_fd, &read_fds)) {
            char buf_read[256];
            ssize_t ret = readLine(sock_fd, buf_read, sizeof(buf_read));
            if (ret > 0) {
                write_log("Received from server: %s", buf_read);

                if (strcmp(buf_read, "YOUR_TURN\n") == 0) {
                    your_turn = true;
                    mvprintw(LINES - 3, 0, "당신의 턴.");
                } else if (strcmp(buf_read, "OPPONENT_TURN\n") == 0) {
                    your_turn = false;
                    mvprintw(LINES - 3, 0, "상대의 턴.");
                } else if (strncmp(buf_read, "Hit", 3) == 0 || strncmp(buf_read, "Miss", 4) == 0) {
                    int x = last_attack_x;
                    int y = last_attack_y;
                    if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
                        if (strncmp(buf_read, "Hit", 3) == 0) {
                            opponent_grid[y][x].aState = HIT;
                        } else if (strncmp(buf_read, "Miss", 4) == 0) {
                            opponent_grid[y][x].aState = MISS;
                        }

                        last_attack_x = -1;
                        last_attack_y = -1;

                        displayGrids(own_grid, opponent_grid, cursor, your_turn, attack_phase);
                        refresh();
                    }
                    mvprintw(LINES - 2, 0, "공격 결과: %s", buf_read);
                    write_log("Updated opponent grid at (%d, %d): %s", x + 1, y + 1, buf_read);
                } else if (strncmp(buf_read, "You won", 7) == 0 || strncmp(buf_read, "You lost", 8) == 0) {
                    mvprintw(LINES - 1, 0, "%s", buf_read);
                    write_log("Game over: %s", buf_read);
                    running = false;
                } else {
                    if (strncmp(buf_read, "Attack", 6) == 0) {
                        int x, y;
                        sscanf(buf_read, "Attack (%d %d)", &x, &y);
                        x -= 1;
                        y -= 1;
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
                            mvprintw(LINES - 1, 0, "상대의 공격 결과: %s", attack_result);
                            write_log("Updated own grid at (%d, %d): %s", x + 1, y + 1, attack_result);
                        }
                    }
                }
                refresh();
            } else if (ret == 0) {
                mvprintw(GRID_SIZE + 12, 0, "서버가 연결을 끊었습니다.");
                write_log("Server disconnected.\n");
                refresh();
                break;
            }
        }

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
                    int x = cursor.x + 1;
                    int y = cursor.y + 1;
                    if (opponent_grid[y - 1][x - 1].aState != UNSHOT) {
                        mvprintw(LINES - 2, 0, "이미 공격한 위치입니다. 다른 위치를 선택하세요.");
                        write_log("Attempted to attack already attacked location (%d, %d)\n", x, y);
                        refresh();
                        sleep(1);
                        break;
                    }
                    last_attack_x = x - 1;
                    last_attack_y = y - 1;

                    char buf_write[20];
                    sprintf(buf_write, "(%d %d)\n", x, y);
                    int write_ret = write(sock_fd, buf_write, strlen(buf_write));
                    if (write_ret < (int)strlen(buf_write)) {
                        mvprintw(LINES - 2, 0, "전송 오류 (전송된 바이트=%d, 오류=%s)", write_ret, strerror(errno));
                        write_log("Send error: %s\n", strerror(errno));
                        refresh();
                        sleep(2);
                        continue;
                    }
                    write_log("Sent attack coordinates: (%d, %d)\n", x, y);
                    your_turn = false;
                    mvprintw(LINES - 1, 0, "공격을 보냈습니다. 결과를 기다리는 중...");
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

    displayGrids(own_grid, opponent_grid, cursor, your_turn, attack_phase);
}

void gameLoopNcursesMultiplayer(Cell own_grid[GRID_SIZE][GRID_SIZE], Cell opponent_grid[GRID_SIZE][GRID_SIZE]) {
    inputLoopMultiplayer(own_grid, opponent_grid);
}

void display_ascii_art_animation() {

    char *ascii_art[] = {
        "______   ___   _____  _____  _      _____     _   _  _____  _____ ",
        "| ___ \\ / _ \\ |_   _||_   _|| |    |  ___|   | \\ | ||  ___||_   _|",
        "| |_/ // /_\\ \\  | |    | |  | |    | |__     |  \\| || |__    | |  ",
        "| ___ \\|  _  |  | |    | |  | |    |  __|    | . ` ||  __|   | |  ",
        "| |_/ /| | | |  | |    | |  | |____| |___  _ | |\\  || |___   | |  ",
        "\\____/ \\_| |_/  \\_/    \\_/  \\_____/\\____/ (_)|_| \\_/\\____/   \\_/  ",
        "                                                                 ",
        "                                                                 ",
        NULL};

    int num_lines = 0;
    while (ascii_art[num_lines] != NULL) {
        num_lines++;
    }

    int start_y = (LINES - num_lines) / 2;
    int start_x = (COLS - strlen(ascii_art[0])) / 2;

    for (int i = 0; i < num_lines; i++) {
        mvprintw(start_y + i, start_x, "%s", ascii_art[i]);
        refresh();      // Update the screen
        usleep(200000); // Delay for 200ms
    }

    usleep(2000000); // 2-second pause
    clear();         // Clear the screen for the next stage
}

void start_multiplayer() {

    clear(); // 메뉴 출력이 겹처나와서 한번 clear하고 아스키 아트 출력

    start_color();
    init_pair(1, COLOR_BLUE, COLOR_BLACK);    // Ship
    init_pair(2, COLOR_RED, COLOR_BLACK);     // Hit
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);  // Miss
    init_pair(4, COLOR_MAGENTA, COLOR_BLACK); // Sunk
    init_pair(5, COLOR_CYAN, COLOR_BLACK);    // Water

    display_ascii_art_animation();

    clear();

    clear();
    int mid_y = LINES / 2 - 4;
    int mid_x = (COLS - strlen("서버 IP를 입력하세요:")) / 2;

    char server_ip[100];
    int server_port;

    mvprintw(mid_y, mid_x, "서버 IP를 입력하세요: ");
    echo();
    scanw("%s", server_ip);
    noecho();

    mvprintw(mid_y + 2, mid_x, "서버 포트를 입력하세요: ");
    echo();
    scanw("%d", &server_port);
    noecho();

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        endwin();
        fprintf(stderr, "소켓 생성 오류: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        endwin();
        fprintf(stderr, "유효하지 않은 주소/ 지원되지 않는 주소입니다.\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        endwin();
        fprintf(stderr, "연결 실패: %s\n", strerror(errno));
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    write_log("Connected to server %s:%d\n", server_ip, server_port);
    mvprintw(LINES - 1, 0, "서버에 성공적으로 연결되었습니다.");
    refresh();
    sleep(1);

    Cell own_grid[GRID_SIZE][GRID_SIZE];
    Cell opponent_grid[GRID_SIZE][GRID_SIZE];
    initGrid(own_grid);
    initGrid(opponent_grid);

    Ship ships_multiplayer[SHIP_NUM] = {
        {"Aircraft Carrier", 5},
        {"Battleship", 4},
        {"Cruiser", 3},
        {"Submarine", 3},
        {"Destroyer", 2}};
    setup_ships(ships_multiplayer);

    placeShipsMultiplayer(own_grid, ships_multiplayer);

    sendGridToServer(own_grid);

    gameLoopNcursesMultiplayer(own_grid, opponent_grid);

    endwin();
    write_log("Multiplayer game ended.\n");
    close(sock_fd);
    if (log_file != NULL) {
        fclose(log_file);
    }
}

int main(int argc, char **argv) {

    setlocale(LC_ALL, "");

    setenv("LANG", "ko_KR.UTF-8", 1);
    setenv("LC_ALL", "ko_KR.UTF-8", 1);
    setenv("LC_CTYPE", "ko_KR.UTF-8", 1);

    initscr();
    start_color();
    cbreak();
    keypad(stdscr, TRUE);

    init_pair(1, COLOR_WHITE, COLOR_BLUE);
    init_pair(2, COLOR_WHITE, COLOR_BLUE);
    init_pair(3, COLOR_BLUE, COLOR_BLACK);

    bkgd(COLOR_PAIR(1));

    char *title_frames[] = {
        " _             _    _    _              _      _        ",
        "| |           | |  | |  | |            | |    (_)       ",
        "| |__    __ _ | |_ | |_ | |  ___   ___ | |__   _  _ __  ",
        "| '_ \\  / _ || __|| __|| | / _ \\ / __|| '_ \\ | || '_ \\ ",
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

    char *ascii_art[] = {
        "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣆⣀⠀⠀⠀⠀⠀⠀",
        "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡇⠁⠀⠀⠀⠀⠀⠀",
        "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣄⢧⠀⠀⠀⠀⠀⠀⠀",
        "⠀⠀⠀⠀⠀⠀⠀⠀⢀⠀⣀⠀⣀⢀⡉⢈⡉⣀⠀⠀⠀⠀⠀⠀",
        "⠀⠀⠀⠀⠀⠀⠀⠄⠢⠠⠡⠠⠡⠨⠠⠨⠠⠨⠠⠠⠀⠀⠀⠀",
        "⠀⠀⣀⣀⣀⡠⣁⣀⡃⠘⠁⠘⠁⠋⠈⢃⢜⣅⢝⣄⣄⣄⠀⠀",
        "⠀⠀⠈⢮⡺⡬⣳⢔⡯⣫⢯⡫⡯⣫⢯⡳⣳⢲⡳⣲⡲⣪⠂⠀",
        "⠀⠀⠀⠀⠙⠮⠳⠝⠮⠳⠵⠝⠮⠳⠣⠯⠮⠳⠝⠮⠺⠅⠀⠀",
        "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        NULL};

    int art = 0;
    while (ascii_art[art]) {
        mvprintw(LINES / 2 + 8 + art, (COLS - strlen(ascii_art[art])) / 2, "%s", ascii_art[art]);
        art++;
    }

    int frame_delay = 200;
    int i = 0;
    while (title_frames[i] || new_ascii_art[i]) {
        if (title_frames[i]) {
            mvprintw(LINES / 2 - 4 + i, (COLS - strlen(title_frames[i])) / 2, "%s", title_frames[i]);
        }
        if (new_ascii_art[i]) {
            mvprintw(LINES / 2 - 11 + i, (COLS - strlen(new_ascii_art[i])) / 2, "%s", new_ascii_art[i]);
        }
        refresh();
        usleep(frame_delay * 1000);
        i++;
    }

    i = 0;
    while (title_frames[i]) {
        mvprintw(LINES / 2 - 4 + i, (COLS - strlen(title_frames[i])) / 2, "%s", title_frames[i]);
        i++;
    }

    int enter_msg_length = strlen("< 게임 시작하기 - ENTER >");
    mvprintw(LINES / 2 + 5, (COLS - enter_msg_length - 2) / 2, "< 게임 시작하기 - ENTER  >");
    refresh();

    while (getch() != '\n')
        ;

    int choice;
    while (1) {
        clear();
        attron(COLOR_PAIR(1));

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

        mvprintw(box_start_y + 1, box_start_x + 2, "배틀쉽 게임");
        mvprintw(box_start_y + 3, box_start_x + 2, "모드 선택");
        mvprintw(box_start_y + 5, box_start_x + 2, "1. 싱글 플레이어");
        mvprintw(box_start_y + 6, box_start_x + 2, "2. 멀티게임- 배틀넷");
        mvprintw(box_start_y + 7, box_start_x + 2, "3. 종료");
        mvprintw(box_start_y + 9, box_start_x + 2, "선택: ");
        refresh();

        echo();
        scanw("%d", &choice);
        noecho();

        if (choice == 1) {
            start_singleplayer();
        } else if (choice == 2) {
            start_multiplayer();
        } else if (choice == 3) {
            break;
        } else {
            mvprintw(box_start_y + 11, box_start_x + 2, "잘못된 선택입니다. 다시 시도하세요.");
            refresh();
            sleep(2);
        }
    }

    endwin();
    return 0;
}
