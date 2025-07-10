#include <arpa/inet.h>
#include <errno.h>
#include <locale.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#define GRID_SIZE 10
#define SHIP_NUM 5

char *id = 0;
short sport = 0;
int sock = 0; /* 통신 소켓 */

// 배 정보
typedef struct {
    char name[20];
    int size;
} Ship;

// 커서 구조체
typedef struct {
    int x;
    int y;
} Cursor;

// 셀 상태
typedef enum {
    UNSHOT,
    MISS,
    HIT,
    SUNK
} CellState;

// 셀 구조체
typedef struct {
    int aShip;        // 0: NONE, 1: DESTROYER, etc.
    CellState aState; // UNSHOT, MISS, HIT, SUNK
} Cell;

// 로그 파일 핸들러
FILE *log_file = NULL;

// 로그 기록 함수
void write_log(const char *format, ...) {
    if (log_file == NULL) {
        log_file = fopen("client_log.txt", "a");
        if (log_file == NULL) {
            // 로그 파일 열기 실패 시 표준 에러에 출력
            perror("로그 파일 열기 실패");
            return;
        }
    }

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fflush(log_file);
}

// 초기화 관련 함수
void initGrid(Cell grid[GRID_SIZE][GRID_SIZE]) {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            grid[i][j].aShip = 0; // 0: NONE
            grid[i][j].aState = UNSHOT;
        }
    }
}

// readLine 함수는 기존과 동일
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

// 두 개의 그리드를 나란히 표시하는 함수
void displayGrids(Cell own_grid[GRID_SIZE][GRID_SIZE], Cell opponent_grid[GRID_SIZE][GRID_SIZE], Cursor cursor, bool your_turn, bool attack_phase) {
    clear();

    // 자신의 그리드 표시
    mvprintw(0, 2, "Your Grid");
    // 열 번호 출력
    mvprintw(1, 2, "  ");
    for (int i = 0; i < GRID_SIZE; i++) {
        printw(" %d", i + 1);
    }
    printw("\n");

    for (int i = 0; i < GRID_SIZE; i++) {
        // 행 번호 출력
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

            if (display_char == 'S')
                attron(COLOR_PAIR(1)); // 배는 파란색
            else if (display_char == 'H')
                attron(COLOR_PAIR(2)); // Hit은 빨간색
            else if (display_char == 'M')
                attron(COLOR_PAIR(3)); // Miss는 노란색
            else if (display_char == 'X')
                attron(COLOR_PAIR(4)); // Sunk는 보라색

            // 배치 단계에서 커서 표시
            if (!attack_phase && your_turn && i == cursor.y && j == cursor.x) {
                attron(A_REVERSE); // 커서 위치 강조
                printw("X ");
                attroff(A_REVERSE);
            } else {
                printw("%c ", display_char);
            }
            attroff(COLOR_PAIR(1) | COLOR_PAIR(2) | COLOR_PAIR(3) | COLOR_PAIR(4));
        }
        printw("\n");
    }

    // 상대의 그리드 표시 (attack_phase일 때만)
    if (attack_phase && opponent_grid != NULL) {
        mvprintw(0, 2 + (GRID_SIZE * 2 + 10), "Opponent's Grid");
        // 열 번호 출력
        mvprintw(1, 2 + (GRID_SIZE * 2 + 10), "  ");
        for (int i = 0; i < GRID_SIZE; i++) {
            printw(" %d", i + 1);
        }
        printw("\n");

        for (int i = 0; i < GRID_SIZE; i++) {
            // 행 번호 출력
            mvprintw(2 + i, 2 + (GRID_SIZE * 2 + 10), "%d ", i + 1);
            for (int j = 0; j < GRID_SIZE; j++) {
                char display_char = '~'; // 기본은 물

                if (opponent_grid != NULL) {
                    if (opponent_grid[i][j].aState == HIT)
                        display_char = 'H';
                    else if (opponent_grid[i][j].aState == MISS)
                        display_char = 'M';
                }

                if (attack_phase && your_turn && i == cursor.y && j == cursor.x) {
                    attron(A_REVERSE); // 커서 위치 강조
                    printw("X ");
                    attroff(A_REVERSE);
                } else {
                    if (display_char == 'H')
                        attron(COLOR_PAIR(2)); // Hit은 빨간색
                    else if (display_char == 'M')
                        attron(COLOR_PAIR(3)); // Miss는 노란색
                    printw("%c ", display_char);
                    attroff(COLOR_PAIR(2) | COLOR_PAIR(3));
                }
            }
            printw("\n");
        }
    }

    // 턴 정보 표시
    if (attack_phase) {
        if (your_turn) {
            mvprintw(GRID_SIZE + 3, 2, "Your turn: Select attack coordinates.");
        } else {
            mvprintw(GRID_SIZE + 3, 2, "Opponent's turn: Waiting for their move...");
        }
    } else {
        mvprintw(GRID_SIZE + 3, 2, "Ship Placement Phase.");
    }

    refresh();
}

// 배치 함수 수정
void placeShips(Cell own_grid[GRID_SIZE][GRID_SIZE], Ship ships[SHIP_NUM]) {
    Cursor cursor = {0, 0};
    int orientation = 0; // 0: 가로, 1: 세로

    for (int i = 0; i < SHIP_NUM; i++) {
        bool placed = false;
        while (!placed) {
            // 그리드 표시
            displayGrids(own_grid, NULL, cursor, true, false);
            // 배치할 배 정보 표시
            mvprintw(GRID_SIZE + 1, 0, "배치할 배: %s (크기: %d)", ships[i].name, ships[i].size);
            mvprintw(GRID_SIZE + 2, 0, "방향: %s", orientation == 0 ? "가로" : "세로");
            mvprintw(GRID_SIZE + 3, 0, "방향 변경: 'o' = 가로, 'v' = 세로");
            mvprintw(GRID_SIZE + 4, 0, "화살표 키로 위치 이동, 엔터 키로 배치 시작");

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
                        own_grid[ny][nx].aShip = ships[i].size; // 배를 배치
                    }
                    placed = true;
                    write_log("배치 완료: %s at (%d, %d) 방향 %s\n", ships[i].name, cursor.x + 1, cursor.y + 1, dir == 0 ? "가로" : "세로");
                } else {
                    mvprintw(GRID_SIZE + 5, 0, "유효하지 않은 위치입니다. 다른 위치를 선택하세요.");
                    write_log("유효하지 않은 위치 시도: (%d, %d)\n", cursor.x + 1, cursor.y + 1);
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

// 서버에 그리드 전송 함수 수정
void sendGridToServer(Cell own_grid[GRID_SIZE][GRID_SIZE]) {
    char buffer[GRID_SIZE * GRID_SIZE + 1];
    int index = 0;

    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            buffer[index++] = own_grid[i][j].aShip > 0 ? 'S' : '~';
        }
    }
    buffer[index] = '\0'; // 문자열 종료

    int ret = send(sock, buffer, strlen(buffer), 0);
    if (ret < 0) {
        mvprintw(GRID_SIZE + 6, 0, "배치 정보 전송 실패: %s", strerror(errno));
        write_log("배치 정보 전송 실패: %s\n", strerror(errno));
        refresh();
        sleep(2);
        return;
    }
    mvprintw(GRID_SIZE + 6, 0, "배치 정보를 서버에 전송했습니다. 전송 크기: %d 바이트", ret);
    write_log("배치 정보 전송: %s, 크기: %d\n", buffer, ret);
    mvprintw(GRID_SIZE + 7, 0, "상대방이 배치중입니다. 잠시만 기다려 주세요...");
    write_log("상대방 배치 대기\n");
    refresh();
    sleep(2);
}

// 사용자 입력을 처리하여 공격 및 턴 변경 처리
void inputLoop(Cell own_grid[GRID_SIZE][GRID_SIZE], Cell opponent_grid[GRID_SIZE][GRID_SIZE]) {
    Cursor cursor = {0, 0}; // 초기 커서 위치 (0,0)
    bool your_turn = false;
    bool attack_phase = true; // 배치 이후 공격 단계
    bool running = true;
    fd_set read_fds;
    struct timeval tv;

    // 초기 opponent_grid는 모두 UNSHOT으로 설정
    initGrid(opponent_grid);

    // 설정: getch를 논블로킹으로 설정
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    while (running) {
        // 초기화 fd_set
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        // 타임아웃 설정 (0.1초)
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int activity = select(sock + 1, &read_fds, NULL, NULL, &tv);

        if (activity < 0 && errno != EINTR) {
            mvprintw(GRID_SIZE + 8, 0, "select 오류: %s", strerror(errno));
            write_log("select 오류: %s\n", strerror(errno));
            refresh();
            break;
        }

        if (FD_ISSET(sock, &read_fds)) {
            // 서버 메시지 수신
            char buf_read[256];
            ssize_t ret = readLine(sock, buf_read, sizeof(buf_read));
            if (ret > 0) {
                write_log("서버로부터 수신한 메시지: %s\n", buf_read);

                if (strcmp(buf_read, "YOUR_TURN\n") == 0) {
                    your_turn = true;
                    mvprintw(GRID_SIZE + 9, 0, "당신의 턴입니다.");
                } else if (strcmp(buf_read, "OPPONENT_TURN\n") == 0) {
                    your_turn = false;
                    mvprintw(GRID_SIZE + 9, 0, "상대의 턴입니다.");
                } else if (strncmp(buf_read, "Hit", 3) == 0 || strncmp(buf_read, "Miss", 4) == 0) {
                    // 공격 결과 처리
                    // 예: "Hit (x y)\n" 또는 "Miss (x y)\n"
                    int x, y;
                    sscanf(buf_read, "%*s (%d %d)", &x, &y);
                    x -= 1; // 0 기반 인덱스
                    y -= 1;
                    if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
                        if (strncmp(buf_read, "Hit", 3) == 0) {
                            opponent_grid[y][x].aState = HIT;
                        } else if (strncmp(buf_read, "Miss", 4) == 0) {
                            opponent_grid[y][x].aState = MISS;
                        }
                    }
                    mvprintw(GRID_SIZE + 10, 0, "공격 결과: %s", buf_read);
                    write_log("공격 결과 업데이트: %s\n", buf_read);
                } else if (strncmp(buf_read, "You won", 7) == 0 || strncmp(buf_read, "You lost", 8) == 0) {
                    mvprintw(GRID_SIZE + 11, 0, "%s", buf_read);
                    write_log("게임 종료 메시지: %s\n", buf_read);
                    running = false;
                } else {
                    // 상대방의 공격 처리
                    if (strncmp(buf_read, "Attack", 6) == 0) {
                        int x, y;
                        sscanf(buf_read, "Attack (%d %d)", &x, &y);
                        x -= 1;
                        y -= 1;
                        // 서버로부터 추가 메시지 (Hit/Miss)
                        char attack_result[256];
                        ret = readLine(sock, attack_result, sizeof(attack_result));
                        if (ret > 0) {
                            write_log("상대방의 공격 결과 메시지: %s\n", attack_result);
                            if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
                                if (strncmp(attack_result, "Hit", 3) == 0) {
                                    own_grid[y][x].aState = HIT;
                                } else if (strncmp(attack_result, "Miss", 4) == 0) {
                                    own_grid[y][x].aState = MISS;
                                }
                            }
                            mvprintw(GRID_SIZE + 12, 0, "상대방 공격 결과: %s", attack_result);
                            write_log("상대방 공격 결과 업데이트: %s\n", attack_result);
                        }
                    }
                }
                refresh();
            } else if (ret == 0) {
                // 서버 연결 종료
                mvprintw(GRID_SIZE + 12, 0, "서버 연결이 종료되었습니다.");
                write_log("서버 연결이 종료되었습니다.\n");
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
                    int x = cursor.x + 1; // 1 기반 인덱스
                    int y = cursor.y + 1;
                    // 이미 공격한 위치는 다시 공격할 수 없도록 체크
                    if (opponent_grid[y - 1][x - 1].aState != UNSHOT) {
                        mvprintw(GRID_SIZE + 13, 0, "이미 공격한 위치입니다. 다른 위치를 선택하세요.");
                        write_log("이미 공격한 위치 시도: (%d, %d)\n", x, y);
                        refresh();
                        sleep(1);
                        break;
                    }
                    char buf_write[20];
                    sprintf(buf_write, "(%d %d)\n", x, y);
                    int write_ret = write(sock, buf_write, strlen(buf_write));
                    if (write_ret < (int)strlen(buf_write)) {
                        mvprintw(GRID_SIZE + 13, 0, "전송 오류 (전송된 바이트=%d, 오류=%s)", write_ret, strerror(errno));
                        write_log("전송 오류: %s\n", strerror(errno));
                        refresh();
                        sleep(2);
                        continue;
                    }
                    write_log("공격 좌표 전송: (%d, %d)\n", x, y);
                    your_turn = false;
                    mvprintw(GRID_SIZE + 14, 0, "공격을 보냈습니다. 결과를 기다리는 중...");
                    refresh();
                } break;
                case 'q':
                case 'Q':
                    running = false;
                    write_log("사용자에 의해 게임 종료\n");
                    break;
                default:
                    break;
                }
            }
        }
    }

    // 그리드 업데이트 및 표시
    displayGrids(own_grid, opponent_grid, cursor, your_turn, attack_phase);
}

// 게임 루프 함수 (ncurses 기반 입력으로 대체)
void gameLoopNcurses(Cell own_grid[GRID_SIZE][GRID_SIZE], Cell opponent_grid[GRID_SIZE][GRID_SIZE]) {
    inputLoop(own_grid, opponent_grid);
}

// main 함수 수정
int main(int argc, char **argv) {
    struct sockaddr_in server;

    setlocale(LC_ALL, "");

    // 인자와 관련된 오류 처리
    if (argc != 4) {
        fprintf(stderr, "사용법: %s id 서버주소 포트\n", argv[0]);
        exit(1);
    }
    id = argv[1];
    sport = atoi(argv[3]);

    // 로그 파일 초기화
    write_log("클라이언트 시작: ID=%s, 서버주소=%s, 포트=%d\n", id, argv[2], sport);

    // ncurses 초기화
    initscr();
    start_color();
    init_pair(1, COLOR_BLUE, COLOR_BLACK);    // 배
    init_pair(2, COLOR_RED, COLOR_BLACK);     // Hit
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);  // Miss
    init_pair(4, COLOR_MAGENTA, COLOR_BLACK); // Sunk
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    // 소켓 생성
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        endwin();
        fprintf(stderr, "소켓 생성 오류: %s\n", strerror(errno));
        write_log("소켓 생성 오류: %s\n", strerror(errno));
        exit(1);
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(sport);
    if (inet_aton(argv[2], &server.sin_addr) == 0) {
        endwin();
        fprintf(stderr, "유효하지 않은 서버 주소: %s\n", argv[2]);
        write_log("유효하지 않은 서버 주소: %s\n", argv[2]);
        close(sock);
        exit(1);
    }

    // 서버에 연결
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        endwin();
        fprintf(stderr, "연결 실패: %s\n", strerror(errno));
        write_log("연결 실패: %s\n", strerror(errno));
        close(sock);
        exit(1);
    }

    write_log("서버에 성공적으로 연결됨: %s:%d\n", inet_ntoa(server.sin_addr), sport);
    mvprintw(GRID_SIZE + 1, 0, "서버에 성공적으로 연결되었습니다.");
    refresh();

    // 그리드 초기화 및 배치
    Cell own_grid[GRID_SIZE][GRID_SIZE];
    Cell opponent_grid[GRID_SIZE][GRID_SIZE];
    initGrid(own_grid);
    initGrid(opponent_grid);
    Ship ships[SHIP_NUM] = {
        {"항공모함", 5},
        {"전함", 4},
        {"순양함", 3},
        {"잠수함", 3},
        {"구축함", 2}};
    placeShips(own_grid, ships);

    // 배치 정보 서버에 전송
    sendGridToServer(own_grid);

    // 게임 루프로 이동 (ncurses 기반)
    gameLoopNcurses(own_grid, opponent_grid);

    // ncurses 종료
    endwin();
    write_log("클라이언트 종료\n");
    close(sock);
    if (log_file != NULL) {
        fclose(log_file);
    }
    return 0;
}
