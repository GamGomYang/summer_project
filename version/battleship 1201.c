// gcc -o battleship battleship.c -lncursesw -lpthread
//  코드 리팩토링 필요
//  멀티플레이 로직 생각 해보기

#include <arpa/inet.h>
#include <errno.h>
#include <locale.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

void handle_winch(int sig) {
    endwin();
    refresh();
    clear();
    display_game_status();
}

char player_board[GRID_SIZE][GRID_SIZE];
char enemy_board[GRID_SIZE][GRID_SIZE];
Ship player_ships[SHIP_NUM];
Ship enemy_ships[SHIP_NUM];

void init_boards();
void display_board(char board[GRID_SIZE][GRID_SIZE], int reveal, int start_y);
int place_ships(Ship ships[], char board[GRID_SIZE][GRID_SIZE]);
int attack(int x, int y, char board[GRID_SIZE][GRID_SIZE], Ship ships[]);
void start_singleplayer();
void start_multiplayer();
void *loading_animation(void *arg);
int check_game_over(Ship ships[]);
void clear_input_buffer();
void setup_ships(Ship ships[]);
void ai_place_ships(Ship ships[], char board[GRID_SIZE][GRID_SIZE]);
void ai_attack(char board[GRID_SIZE][GRID_SIZE], Ship ships[], int difficulty);
void display_game_status();
void display_error_message(const char *message);
void display_attack_result(int result);
void display_game_result(const char *result_message);

int main() {
    setlocale(LC_ALL, "");

    int choice;
    initscr();     // ncurses 모드 시작
    start_color(); // 색상 적용
    cbreak();      // 버퍼링 제거
    // noecho();      // 입력된 키를 화면에 표시하지 않음
    // noecho 함수에 대해 조금 더알아봐야겠음

    keypad(stdscr, TRUE); // 특수 키 입력 활성화

    init_pair(1, COLOR_WHITE, COLOR_BLACK); // 기본 색상

    // 배경색을 파란색으로 설정
    init_pair(2, COLOR_WHITE, COLOR_BLUE);
    bkgd(COLOR_PAIR(2));

    // 시작 페이지 출력
    attron(COLOR_PAIR(2)); // 파란색 배경색 설정

    // 아스키 아트가 안나옴
    // 해결해야함
    char *new_ascii_art[] =
        {
            "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠛⢛⠛⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
            "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡇⠀⢐⣮⡆⠀⢠⠀⠀⢠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
            "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡸⡿⣀⢨⠿⡇⣐⢿⢆⠀⢐⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
            "⠀⠀⠀⠀⠀⠀⠀⠀⠀⣤⢯⡯⡯⣯⢯⡯⣯⢯⡯⣄⠨⠀⢀⣐⣛⣃⣈⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
            "⠀⠀⣠⣀⣀⣀⣀⣒⣛⣛⣓⣛⣛⣋⣛⣋⣛⣛⣚⣛⣛⣛⣜⣻⣻⣻⣻⣤⣽⣿⣥⣤⣤⣤⣤⣴⡔⠀⠀",
            "⠀⠀⠈⠛⣻⣛⣟⣻⣛⣻⣛⣛⣟⣛⣟⣻⣛⣟⣛⣟⣻⣛⣛⣛⣛⣛⣛⣏⣟⣝⣛⣛⣛⣗⣜⡟⠀⠀⠀",
            "⠀⠀⠀⠀⠀⠉⠉⠁⠉⠉⠉⠉⠉⠉⠉⠉⠈⠉⠉⠁⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠁⠁⠀⠀⠀",
            "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
            NULL};

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

    int frame_delay = 200; // 프레임 간 딜레이 (밀리초)
    int i = 0;
    while (title_frames[i] || new_ascii_art[i]) {
        if (title_frames[i]) {
            mvprintw(LINES / 2 - 4 + i, (COLS - strlen(title_frames[i])) / 2, "%s", title_frames[i]);
        }
        if (new_ascii_art[i]) {
            mvprintw(LINES / 2 + 8 + i, (COLS - strlen(new_ascii_art[i])) / 2, "%s", new_ascii_art[i]);
        }
        refresh();
        usleep(frame_delay * 1000); // 밀리초를 마이크로초로 변환
        i++;
    }

    // 아스키 아트 출력
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

    i = 0;
    while (ascii_art[i]) {
        mvprintw(LINES / 2 + 8 + i, (COLS - strlen(ascii_art[i])) / 2, "%s", ascii_art[i]);
        i++;
    }

    // 오른쪽 상단에 프로젝트 정보 출력

    // 애니메이션이 끝난 후 제목을 고정
    i = 0;
    while (title_frames[i]) {
        mvprintw(LINES / 2 - 4 + i, (COLS - strlen(title_frames[i])) / 2, "%s", title_frames[i]);
        i++;
    }

    // 중앙 정렬을 위해 "<ENTER - 시작하기>"의 전체 길이를 사용
    int enter_msg_length = strlen("<ENTER - 시작하기>");
    mvprintw(LINES / 2 + 5, (COLS - enter_msg_length - 2) / 2, "< E N T E R - 시 작 하 기 >");
    refresh();

    // 엔터 키 입력 대기
    while (getch() != '\n')
        ;

    // 게임 모드 선택 함수 호출

    while (1) {
        clear();
        attron(COLOR_PAIR(1));

        // 중앙에 하얀색 박스 그리기
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
        mvprintw(box_start_y + 5, box_start_x + 2, "1. 싱글플레이");
        mvprintw(box_start_y + 6, box_start_x + 2, "2. 멀티플레이");
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
            mvprintw(box_start_y + 11, box_start_x + 2, "잘못된 선택입니다. 다시 시도해주세요.");
            refresh();
            sleep(2);
        }
    }

    endwin(); // ncurses 종료
    return 0;
}

/*
 * Summary:      Initializes the game boards for both player and enemy
 * Parameters:   None
 * Return:       None
 */
void init_boards() {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            player_board[i][j] = '~'; // 물을 '~'로 표시
            enemy_board[i][j] = '~';
        }
    }
}

/*
 * Summary:      Displays the game board
 * Parameters:   board[][GRID_SIZE], reveal, start_y
 * Return:       None
 */
void display_board(char board[GRID_SIZE][GRID_SIZE], int reveal, int start_y) {
    int mid_x = (COLS - (GRID_SIZE * 4 + 4)) / 2; // 좌우 여백을 고려하여 x 좌표 계산

    attron(COLOR_PAIR(2)); // 회색 배경색 설정

    // 상단 인덱스 출력
    mvprintw(start_y, mid_x + 4, "  ");
    for (int i = 0; i < GRID_SIZE; i++) {
        printw(" %2d ", i);
    }

    // 보드 출력
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

    attroff(COLOR_PAIR(2)); // 회색 배경색 해제
    refresh();
}

/*
 * Summary:      Sets up the ships with their names and sizes
 * Parameters:   ships[]
 * Return:       None
 */
void setup_ships(Ship ships[]) {
    strcpy(ships[0].name, "항공모함");
    ships[0].size = 5;
    ships[0].hits = 0;

    strcpy(ships[1].name, "전함");
    ships[1].size = 4;
    ships[1].hits = 0;

    strcpy(ships[2].name, "순양함");
    ships[2].size = 3;
    ships[2].hits = 0;

    strcpy(ships[3].name, "잠수함");
    ships[3].size = 3;
    ships[3].hits = 0;

    strcpy(ships[4].name, "구축함");
    ships[4].size = 2;
    ships[4].hits = 0;
}

/*
 * Summary:      Clears the input buffer
 * Parameters:   None
 * Return:       None
 */
void clear_input_buffer() {
    int ch;
    while ((ch = getch()) != '\n' && ch != EOF)
        ;
}

/*
 * Summary:      Places the ships on the board
 * Parameters:   ships[], board[][GRID_SIZE]
 * Return:       Integer
 */
int place_ships(Ship ships[], char board[GRID_SIZE][GRID_SIZE]) {
    int x, y, orientation;
    for (int i = 0; i < SHIP_NUM; i++) {
        while (1) {
            clear();
            display_board(board, 1, 0);
            int mid_x = (COLS - 20) / 2;             // 중앙 정렬을 위한 x 좌표 계산
            int mid_y = (LINES - GRID_SIZE - 6) / 2; // 중앙 정렬을 위한 y 좌표 계산
            mvprintw(mid_y + GRID_SIZE + 2, mid_x, "%s를 배치하세요. 크기: %d", ships[i].name, ships[i].size);
            mvprintw(mid_y + GRID_SIZE + 3, mid_x, "시작 위치 (x y): ");
            refresh();

            echo();
            if (scanw("%d %d", &x, &y) != 2) {
                mvprintw(mid_y + GRID_SIZE + 5, mid_x, "올바른 좌표를 입력하세요.");
                noecho();
                clear_input_buffer();
                refresh();
                sleep(1);
                continue;
            }
            noecho();

            if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) {
                mvprintw(mid_y + GRID_SIZE + 5, mid_x, "좌표가 범위를 벗어났습니다.");
                refresh();
                sleep(1);
                continue;
            }

            mvprintw(mid_y + GRID_SIZE + 4, mid_x, "방향 (0: 가로, 1: 세로): ");
            refresh();

            echo();
            if (scanw("%d", &orientation) != 1) {
                mvprintw(mid_y + GRID_SIZE + 5, mid_x, "올바른 방향을 입력하세요.");
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

            // 배치 가능 여부 확인
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
                mvprintw(mid_y + GRID_SIZE + 5, mid_x, "배치를 할 수 없습니다. 다른 위치를 선택하세요.");
                refresh();
                sleep(1);
            }
        }
    }
    return 0;
}

/*
 * Summary:      Handles the attack logic
 * Parameters:   x, y, board[][GRID_SIZE], ships[]
 * Return:       Integer
 */
int attack(int x, int y, char board[GRID_SIZE][GRID_SIZE], Ship ships[]) {
    if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) {
        return -2; // 범위 밖
    }

    if (board[y][x] == 'S') {
        board[y][x] = 'X'; // 명중 표시

        // 해당 배의 명중 횟수 증가
        for (int i = 0; i < SHIP_NUM; i++) {
            for (int j = 0; j < ships[i].size; j++) {
                if (ships[i].x[j] == x && ships[i].y[j] == y) {
                    ships[i].hits++;
                    break;
                }
            }
        }

        return 1; // 명중
    } else if (board[y][x] == '~') {
        board[y][x] = 'O'; // 빗나감 표시
        return 0;          // 빗나감
    } else {
        return -1; // 이미 공격한 위치
    }
}

/*
 * Summary:      Checks if the game is over
 * Parameters:   ships[]
 * Return:       Integer
 */
int check_game_over(Ship ships[]) {
    for (int i = 0; i < SHIP_NUM; i++) {
        if (ships[i].hits < ships[i].size) {
            return 0; // 아직 게임이 끝나지 않음
        }
    }
    return 1; // 모든 배가 격침됨
}

/*
 * Summary:      Places the ships for the AI
 * Parameters:   ships[], board[][GRID_SIZE]
 * Return:       None
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
 * Summary:      Handles the AI attack logic
 * Parameters:   board[][GRID_SIZE], ships[], difficulty
 * Return:       None
 */
void ai_attack(char board[GRID_SIZE][GRID_SIZE], Ship ships[], int difficulty) {
    static int hit_stack[GRID_SIZE * GRID_SIZE][2];
    static int stack_top = -1;

    int x, y, result;

    if (difficulty == 1) {
        // Easy: rand값을 받아서 무작위 배치
        do {
            x = rand() % GRID_SIZE;
            y = rand() % GRID_SIZE;
            result = attack(x, y, board, ships);
        } while (result == -1 || result == -2);
    } else if (difficulty == 2 || difficulty == 3) {
        // nomal hard mode 공격로직
        // 지금은 nomal 과 hard 모드가 같은 로직임
        // 추후 난이도에 따른 로직 추가 필요
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
                // 주변 좌표를 스택에 추가
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

/*
 * Summary:      Starts the singleplayer mode
 * Parameters:   None
 * Return:       None
 */
void start_singleplayer() {
    int difficulty;
    clear();
    int mid_y = LINES / 2 - 3;                               // 화면 중앙에 맞추기 위해 y 좌표 계산
    int mid_x = (COLS - strlen("난이도를 선택하세요:")) / 2; // x 좌표 계산

    mvprintw(mid_y, mid_x, "난이도를 선택하세요:");
    mvprintw(mid_y + 2, mid_x, "1. Easy");
    mvprintw(mid_y + 3, mid_x, "2. Normal");
    mvprintw(mid_y + 4, mid_x, "3. Hard");
    mvprintw(mid_y + 6, mid_x, "선택: ");
    refresh();

    // 잘못된 난이도 입력 처리

    echo();
    if (scanw("%d", &difficulty) != 1) {
        mvprintw(mid_y + 8, mid_x, "올바른 숫자를 입력하세요.");
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
        mvprintw(10, 0, "배치에 실패했습니다.");
        refresh();
        sleep(2);
        return;
    }

    ai_place_ships(enemy_ships, enemy_board);

    // 게임 플레이 루프
    int game_over = 0;
    int x, y, result;
    while (!game_over) {
        clear();
        display_game_status();

        echo();
        if (scanw("%d %d", &x, &y) != 2) {
            display_error_message("올바른 좌표를 입력하세요.");
            noecho();
            clear_input_buffer();
            sleep(1);
            continue;
        }
        noecho();

        result = attack(x, y, enemy_board, enemy_ships);
        display_attack_result(result);

        if (check_game_over(enemy_ships)) {
            display_game_result("승리했습니다!");
            break;
        }

        // 싱글모드 -> 컴퓨터 공격로직
        ai_attack(player_board, player_ships, difficulty);

        if (check_game_over(player_ships)) {
            display_game_result("패배했습니다.");
            break;
        }
    }
}

/*
 * Summary:      Displays the game status
 * Parameters:   None
 * Return:       None
 */
void display_game_status() {
    int mid_x = (COLS - strlen("당신의 보드:")) / 2;
    mvprintw(0, mid_x, "당신의 보드:");
    display_board(player_board, 1, 1);

    mid_x = (COLS - strlen("상대의 보드:")) / 2;
    mvprintw(2 * GRID_SIZE + 2, mid_x, "상대의 보드:");
    display_board(enemy_board, 0, 2 * GRID_SIZE + 3);

    mid_x = (COLS - strlen("공격할 위치를 입력하세요 (x y):")) / 2;
    mvprintw(4 * GRID_SIZE + 5, mid_x, "공격할 위치를 입력하세요 (x y): ");
    refresh();
}

/*
 * Summary:      Displays an error message
 * Parameters:   message
 * Return:       None
 */
void display_error_message(const char *message) {
    int mid_x = (COLS - strlen(message)) / 2;
    mvprintw(4 * GRID_SIZE + 7, mid_x, "%s", message);
    refresh();
}

/*
 * Summary:      Displays the result of an attack
 * Parameters:   result
 * Return:       None
 */
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

/*
 * Summary:      Displays the result of the game
 * Parameters:   result_message
 * Return:       None
 */
void display_game_result(const char *result_message) {
    int mid_x = (COLS - strlen(result_message)) / 2;
    mvprintw(4 * GRID_SIZE + 9, mid_x, "%s", result_message);
    refresh();
    sleep(3);
}

/*
 * Summary:      Displays a loading animation while waiting for the opponent
 * Parameters:   arg
 * Return:       None
 */
void *loading_animation(void *arg) {
    int i = 0;
    while (1) {
        mvprintw(5, 0, "상대방이 들어올 때까지 기다립니다");
        for (int j = 0; j < (i % 4); j++) {
            printw(".");
        }
        refresh();
        i++;
        sleep(1);
    }
    return NULL;
}

void start_multiplayer() {

    // 멀티플레이 프로그램 로직
}
