// client.c

#define _XOPEN_SOURCE_EXTENDED // 한글깨짐현상 방지

#include <arpa/inet.h>
#include <ctype.h> // 한글깨짐현상 방지
#include <errno.h>
#include <locale.h>
#include <ncursesw/ncurses.h> // For wide-character support
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

// gcc client.c -o client -lncursesw -lpthread

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12345
#define BUFFER_SIZE 2048
#define INPUT_BUFFER_SIZE 1024 // 오버플로 방지
#define LOG_FILE "client.log"

int sock = 0;
pthread_t recv_thread;
int running = 1;

// 게임 설정 변수
char current_game_mode[50] = "산성비"; // 산성비 다른로직들을 추가하여야함
int current_time_limit = 90;           // 기본 시간 제한

// 게임 실행 중 여부를 나타내는 플래그
int game_running = 0;

// 함수 선언
void *recv_handler(void *arg);
void send_message_to_server(const char *message);
void cleanup();
void log_event(const char *format, ...);
void initialize_windows();
void run_game(int game_duration); // 게임 실행 함수 선언

// ncurses 창 선언
WINDOW *chat_win;
WINDOW *input_win;
pthread_mutex_t window_mutex = PTHREAD_MUTEX_INITIALIZER;

// 로그 파일 포인터
FILE *log_fp = NULL;

// 게임 관련 변수들
int game_GAME_DURATION = 90;    // 게임 시간 (초 단위)
#define GAME_MAX_WORD_LENGTH 30 // 단어의 최대 길이 증가
#define GAME_MAX_WORDS 100

// 시작화면 애니메이션

void show_start_page() {
    const char *title[] = {
        " _____           _    _                      ",
        "|_   _|         (_)  (_)                     ",
        "  | |    __ _    _    _   __ _  _ __    __ _ ",
        "  | |   / _` |  | |  | | / _` || '_ \\  / _` |",
        "  | |  | (_| |  | |  | || (_| || | | || (_| |",
        "  \\_/   \\__,_|  | |  | | \\__,_||_| |_| \\__, |",
        "               _/ | _/ |                __/ |",
        "              |__/ |__/                |___/ "};
    const int title_lines = sizeof(title) / sizeof(title[0]);
    const char *subtitle = "타이핑 게임 - 타짱";
    const char *prompt = "<시작하기 - ENTER>";

    // 화면 크기 가져오기
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // 제목의 위치 계산
    int start_y = (max_y - title_lines) / 2 - 2; // 제목을 화면 중앙에 위치
    int start_x = (max_x - strlen(title[0])) / 2;

    // 서브타이틀과 프롬프트의 위치 계산
    int subtitle_y = start_y + title_lines + 2;
    int subtitle_x = (max_x - strlen(subtitle)) / 2;
    int prompt_y = subtitle_y + 2;
    int prompt_x = (max_x - strlen(prompt)) / 2;

    // 색상 초기화
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);    // 빨간 글씨, 검은 배경
    init_pair(2, COLOR_WHITE, COLOR_BLACK);  // 서브타이틀용
    init_pair(3, COLOR_YELLOW, COLOR_BLACK); // 프롬프트용

    // 애니메이션 출력
    for (int i = 0; i < title_lines; i++) {
        attron(COLOR_PAIR(1)); // 빨간색 활성화
        mvprintw(start_y + i, start_x, "%s", title[i]);
        attroff(COLOR_PAIR(1)); // 색상 비활성화
        refresh();
        usleep(100000); // 0.1초 대기 (애니메이션 효과)
    }

    // 서브타이틀 출력
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(subtitle_y, subtitle_x, "%s", subtitle);
    attroff(COLOR_PAIR(2) | A_BOLD);

    // 프롬프트 출력
    attron(COLOR_PAIR(3) | A_BLINK);
    mvprintw(prompt_y, prompt_x, "%s", prompt);
    attroff(COLOR_PAIR(3) | A_BLINK);

    refresh();

    // ENTER 키 입력 대기
    while (1) {
        int ch = getch();
        if (ch == '\n') { // ENTER 키가 눌리면 루프 종료
            break;
        }
    }

    // 화면 초기화
    clear();
    refresh();
}

// 단어 데이터베이스
const char *game_wordDB[] = {
    "apple", "banana", "cherry", "dragon", "elephant",
    "flower", "guitar", "house", "island", "jungle",
    "keyboard", "lemon", "mountain", "notebook", "orange",
    "pumpkin", "queen", "river", "sunflower", "tree",
    "umbrella", "violin", "watermelon", "xylophone", "yacht", "zebra",
    "cloud", "horizon", "desert", "ocean", "valley",
    "forest", "planet", "galaxy", "comet", "asteroid",
    "rocket", "spaceship", "satellite", "meteor", "nebula",
    "castle", "kingdom", "village", "harbor", "mountain",
    "stream", "meadow", "canyon", "volcano", "waterfall",
    "island", "plateau", "cliff", "prairie", "peninsula"};
const int game_wordDB_size = sizeof(game_wordDB) / sizeof(game_wordDB[0]);

// 파워업 단어 리스트
const char *game_powerUpDB[] = {
    "power", "boost", "extra", "golden", "silver", "diamond",
    "energy", "shield", "rocket", "laser", "turbo", "invincible"};
const int game_powerUpDB_size = sizeof(game_powerUpDB) / sizeof(game_powerUpDB[0]);

// 단어 노드 구조체
typedef struct game_word_node {
    int row, col;                    // 위치
    char word[GAME_MAX_WORD_LENGTH]; // 내려오는 단어
    int is_power_up;                 // 파워업 여부 (0: 일반, 1: 파워업)
    struct game_word_node *next;     // 다음 노드
} GameWordNode;

// 플레이어 구조체
typedef struct {
    int row, col; // 위치
    char symbol;  // 플레이어 심볼
} GamePlayer;

// 게임 전역 변수
GameWordNode *game_word_head = NULL; // 단어 리스트 헤드
GamePlayer game_player;              // 플레이어
volatile int game_game_over = 0;     // 게임 종료 플래그
int game_score = 0;                  // 점수
int game_level = 1;                  // 레벨
int game_word_speed = 1;             // 단어 하강 속도
int game_word_interval = 2;          // 단어 생성 간격 (프레임 수)
int game_frame_delay = 1000000;      // 프레임 대기 시간 (500ms -> 500,000 us)

// 입력 처리 관련
char game_typingText[GAME_MAX_WORD_LENGTH] = {0};
int game_enter_position = 0;

// 타이머 관련
time_t game_start_time;

// 게임 함수 선언
void game_define_colors();
void game_init_player();
void game_draw_player();
void game_add_word();
void game_update_words();
void game_draw_words();
void game_word_Check(char *str);
void game_handle_input();
void game_cleanup();
void game_end_game_handler(int signum);
void *game_game_thread_func(void *arg);

// 로그 기록 함수
void log_event(const char *format, ...) {
    if (log_fp == NULL)
        return;

    va_list args;
    va_start(args, format);
    vfprintf(log_fp, format, args);
    va_end(args);
    fflush(log_fp);
}

// 게임 쓰레드 함수
void *game_thread_func(void *arg) {
    // 게임 실행 준비 로그
    log_event("게임 쓰레드 시작: 모드=%s, 제한 시간=%d초\n", current_game_mode, current_time_limit);

    // 게임 실행
    run_game(current_time_limit);

    // 게임 종료 후 점수 저장
    int score = game_score;

    // 서버에 게임 종료 메시지 전송
    char game_over_msg[50];
    snprintf(game_over_msg, sizeof(game_over_msg), "GAME_OVER %d\n", score);
    send_message_to_server(game_over_msg);

    // 게임 실행 종료 로그
    log_event("게임이 종료되었습니다. 점수: %d\n", score);

    pthread_mutex_lock(&window_mutex);
    wprintw(chat_win, "게임이 종료되었습니다. 최종 점수: %d\n", score);
    wrefresh(chat_win);
    pthread_mutex_unlock(&window_mutex);

    // 서버에 게임 종료 후 준비 완료 메시지 전송
    // 원하지 않으면 주석 처리하거나 제거
    // char ready_msg[] = "/ready\n";
    // send_message_to_server(ready_msg);
    // log_event("서버에 READY 메시지 전송: %s", ready_msg);

    return NULL;
}

// 창 초기화 함수
void initialize_windows() {
    // Start color mode
    start_color();

#ifndef COLOR_GRAY
#define COLOR_GRAY 8
#endif

    if (can_change_color() && COLORS >= 16) {
        init_color(COLOR_GRAY, 500, 500, 500); // 회색 정의
        init_color(COLOR_BLUE, 0, 0, 1000);
        init_pair(1, COLOR_BLACK, COLOR_GRAY); // 배경색
        init_pair(2, COLOR_BLUE, COLOR_GRAY);  // /help 출력용
    } else {
        init_pair(1, COLOR_BLACK, COLOR_WHITE); // 기본 색상으로 대체
        init_pair(2, COLOR_BLUE, COLOR_WHITE);
    }

    // 창 생성
    chat_win = newwin(LINES - 7, COLS, 0, 0);
    scrollok(chat_win, TRUE);
    wbkgd(chat_win, COLOR_PAIR(1)); // 배경색 설정
    // 출력창 이름입력 추가
    wrefresh(chat_win);

    input_win = newwin(5, COLS, LINES - 5, 0);
    box(input_win, 0, 0);
    wbkgd(input_win, COLOR_PAIR(1)); // 배경색 설정
    mvwprintw(input_win, 1, 1, "입력: ");
    mvwprintw(input_win, 2, 1, "사용 가능한 명령어는 /help를 입력하세요.");
    wrefresh(input_win);
}

// 메시지 수신 및 출력 함수
void *recv_handler(void *arg) {
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0 && running) {
        buffer[bytes_read] = '\0';

        // 로그 파일에 기록
        if (log_fp != NULL) {
            fprintf(log_fp, "수신된 메시지: %s", buffer);
            fflush(log_fp);
        }

        // 게임 실행 중일 때는 수신된 메시지를 처리하지 않음
        if (game_running) {
            // 게임 종료 메시지인 경우 처리

            if (strncmp(buffer, "GAME_OVER", 9) == 0) {
                // 게임 종료 플래그 설정
                game_game_over = 1;

                // 승자 정보 파싱
                char winner_message[BUFFER_SIZE];
                sscanf(buffer + 10, "%[^\n]", winner_message);

                // 게임 종료 후 메시지 출력
                pthread_mutex_lock(&window_mutex);
                wprintw(chat_win, "%s\n", winner_message);
                wrefresh(chat_win);
                pthread_mutex_unlock(&window_mutex);

                // 클라이언트 종료 플래그 설정을 제거하여 클라이언트가 종료되지 않도록 함
                // running = 0; // 이 줄을 주석 처리하거나 제거
            }
            continue;
        }

        // 메시지 출력
        pthread_mutex_lock(&window_mutex);
        if (strncmp(buffer, "CHAT ", 5) == 0) {
            wprintw(chat_win, "%s", buffer + 5); // "CHAT " 이후의 메시지만 출력
        } else if (strncmp(buffer, "WELCOME ", 8) == 0) {
            // 서버에서 보내는 환영 메시지 형식: "WELCOME <이름>"
            char user_name[50];
            sscanf(buffer + 8, "%49s", user_name);
            wprintw(chat_win, "어서오세요~~ 타이핑게임에 오신것을 환영합니다 %s님!\n", user_name);
        } else if (strncmp(buffer, "ROOM_CREATED ", 13) == 0) {
            // 서버에서 보내는 방 생성 메시지 형식: "ROOM_CREATED <방 ID> <방 이름>"
            int room_id;
            char room_name[100];
            sscanf(buffer + 13, "%d %99[^\n]", &room_id, room_name);
            wprintw(chat_win, "방이 생성되었습니다. 방 ID: %d, 방 이름: %s\n", room_id, room_name);
        } else if (strncmp(buffer, "USER_JOINED ", 12) == 0) {
            // 서버에서 보내는 사용자 입장 메시지 형식: "USER_JOINED <이름>"
            char user_name[50];
            sscanf(buffer + 12, "%49s", user_name);
            wprintw(chat_win, "%s님이 방에 입장하셨습니다.\n", user_name);
        } else if (strncmp(buffer, "GAME_SETTINGS ", 14) == 0) {
            char game_mode[50];
            int time_limit;
            sscanf(buffer + 14, "%49s %d", game_mode, &time_limit);

            // 게임 설정 저장
            strncpy(current_game_mode, game_mode, sizeof(current_game_mode) - 1);
            current_game_mode[sizeof(current_game_mode) - 1] = '\0';
            current_time_limit = time_limit;

            // 게임 설정 창 표시
            werase(chat_win);
            mvwprintw(chat_win, 1, 1, "게임 모드: %s, 제한 시간: %d초", current_game_mode, current_time_limit);
            wrefresh(chat_win);

            // 설정 완료 메시지
            pthread_mutex_lock(&window_mutex);
            werase(input_win);
            box(input_win, 0, 0);
            mvwprintw(input_win, 1, 1, "설정을 완료하려면 Enter를 누르세요.");
            wrefresh(input_win);
            pthread_mutex_unlock(&window_mutex);

            // 로그 파일에 기록
            if (log_fp != NULL) {
                fprintf(log_fp, "게임 설정 수신: 모드=%s, 시간 제한=%d\n", game_mode, time_limit);
                fflush(log_fp);
            }

            // Enter 키 대기
            wgetch(input_win); // Enter 키 대기

            // 서버에 설정 완료 메시지 전송
            char ready_msg[] = "/ready\n";
            send_message_to_server(ready_msg);
            log_event("서버에 READY 메시지 전송: %s", ready_msg);
        } else if (strncmp(buffer, "GAME_STARTED", 12) == 0) {
            // 게임 시작 메시지 수신 시 게임 모듈 실행
            wprintw(chat_win, "게임이 시작됩니다!\n");
            wrefresh(chat_win);

            // 게임 실행을 위한 쓰레드 생성
            pthread_t game_thread;
            if (pthread_create(&game_thread, NULL, game_thread_func, NULL) != 0) {
                wprintw(chat_win, "게임 쓰레드 생성 실패\n");
                wrefresh(chat_win);
                log_event("게임 쓰레드 생성 실패\n");
            } else {
                pthread_detach(game_thread);
            }
        } else if (strncmp(buffer, "GAME_OVER", 9) == 0) {
            // 이미 위에서 처리됨
            // 필요에 따라 추가적인 처리를 할 수 있음
        } else if (strncmp(buffer, "SERVER_SHUTDOWN", 15) == 0) {
            // 서버 종료 메시지 수신 시 클린업 및 종료
            wprintw(chat_win, "서버가 종료되었습니다.\n");
            wrefresh(chat_win);
            running = 0;
        } else if (strncmp(buffer, "ERROR ", 6) == 0) {
            // 오류 메시지 수신 시 표시
            wprintw(chat_win, "오류: %s", buffer + 6);
        } else if (strstr(buffer, "사용 가능한 명령어는") != NULL || strstr(buffer, "Available commands are") != NULL) {
            // /help 출력 처리
            wattron(chat_win, COLOR_PAIR(2));
            wprintw(chat_win, "%s", buffer);
            wattroff(chat_win, COLOR_PAIR(2));
        } else {
            // 그 외의 메시지 처리
            wprintw(chat_win, "%s\n", buffer);
        }

        wrefresh(chat_win);
        pthread_mutex_unlock(&window_mutex);
    }

    if (bytes_read == 0) {
        printf("서버가 연결을 종료했습니다.\n");
        log_event("서버가 연결을 종료했습니다.\n");
    } else if (bytes_read < 0) {
        perror("recv 실패");
        log_event("recv 실패: %s\n", strerror(errno));
    }

    //***************errorcheck****************
    running = 0;
    pthread_exit(NULL);
}

// 메시지 전송 함수
void send_message_to_server(const char *message) {
    // 메시지에 '\n'이 없으면 추가
    char msg_with_newline[BUFFER_SIZE];
    strncpy(msg_with_newline, message, sizeof(msg_with_newline) - 2);
    msg_with_newline[sizeof(msg_with_newline) - 2] = '\0';
    if (msg_with_newline[strlen(msg_with_newline) - 1] != '\n') {
        strncat(msg_with_newline, "\n", sizeof(msg_with_newline) - strlen(msg_with_newline) - 1);
    }

    if (send(sock, msg_with_newline, strlen(msg_with_newline), 0) < 0) {
        perror("메시지 전송 실패");
        // 로그 파일에 기록
        if (log_fp != NULL) {
            fprintf(log_fp, "메시지 전송 실패: %s\n", strerror(errno));
            fflush(log_fp);
        }
    } else {
        // 로그 파일에 기록
        if (log_fp != NULL) {
            fprintf(log_fp, "서버에 메시지 전송: %s", msg_with_newline);
            fflush(log_fp);
        }
    }
}

// 클린업 함수
void cleanup() {
    running = 0;
    close(sock);
    endwin();

    // 로그 파일 닫기
    if (log_fp != NULL) {
        fclose(log_fp);
    }
}

// 메인 함수
int main() {
    struct sockaddr_in serv_addr;
    char name[50];

    setlocale(LC_ALL, ""); // 로케일 설정

    // 로그 파일 열기
    log_fp = fopen(LOG_FILE, "a");
    if (log_fp == NULL) {
        perror("로그 파일 열기 실패");
        exit(EXIT_FAILURE);
    }

    // ncurses 초기화
    initscr();
    cbreak();
    noecho();
    curs_set(TRUE); // 커서 보이기
    keypad(stdscr, TRUE);
    nodelay(stdscr, FALSE); // 블로킹 모드로 설정

    // 시작화면 함수 호출
    show_start_page();

    // 창 초기화 함수 호출
    initialize_windows();

    // 소켓 생성
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("소켓 생성 실패");
        cleanup();
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    // 서버 IP 변환
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("잘못된 주소/주소 형식");
        cleanup();
        exit(EXIT_FAILURE);
    }

    // 서버에 연결
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("서버에 연결할 수 없습니다");
        cleanup();
        exit(EXIT_FAILURE);
    }

    // 사용자 이름 입력

    werase(input_win);
    box(input_win, 0, 0);
    mvwprintw(input_win, 1, 1, "입력: ");
    mvwprintw(input_win, 2, 1, "이름을 입력해주세요 ");
    wmove(input_win, 1, 8); // 커서를 '입력: ' 다음 칸으로 이동
    wrefresh(input_win);
    memset(name, 0, sizeof(name));               // 입력 버퍼 초기화
    echo();                                      // 입력한 글자가 보이게 설정
    wgetnstr(input_win, name, sizeof(name) - 1); // 이름 입력
    noecho();                                    // 다시 입력한 글자가 안 보이게 설정

    // 이름 전송
    send_message_to_server(name); // 서버로 이름 전송

    werase(input_win);

    // 로그: 이름 전송 완료
    log_event("이름 전송 완료: %s\n", name);

    // 입력창 초기화 후 명령어 안내 출력
    werase(input_win);
    box(input_win, 0, 0);
    mvwprintw(input_win, 1, 1, "입력: ");
    mvwprintw(input_win, 2, 1, "사용 가능한 명령어는 /help를 입력하세요.");
    wrefresh(input_win);

    // 리시브 스레드 생성
    if (pthread_create(&recv_thread, NULL, recv_handler, NULL) != 0) {
        perror("리시브 스레드 생성 실패");
        cleanup();
        exit(EXIT_FAILURE);
    }

    // 메인 루프: 사용자 입력 처리
    while (running) {
        // 게임 실행 중이면 입력을 받지 않음
        if (game_running) {
            sleep(1);
            continue;
        }

        // 입력 창 초기화
        pthread_mutex_lock(&window_mutex);
        werase(input_win);
        box(input_win, 0, 0);
        mvwprintw(input_win, 1, 1, "입력: ");
        mvwprintw(input_win, 3, 1, "사용 가능한 명령어는 /help를 입력하세요."); // 메뉴얼 안내
        wrefresh(input_win);
        pthread_mutex_unlock(&window_mutex);

        // 멀티바이트 문자 입력 처리
        wchar_t winput_buffer[INPUT_BUFFER_SIZE];
        memset(winput_buffer, 0, sizeof(winput_buffer));
        int wpos = 0;

        while (1) {
            wint_t wch;
            int wret = wget_wch(input_win, &wch);

            if (wret == ERR) {
                continue;
            }

            // 엔터 키로 입력 종료
            if (wch == L'\n') {
                winput_buffer[wpos] = L'\0'; // 입력된 문자열 종료
                break;
            }

            // 백스페이스 처리
            if ((wch == KEY_BACKSPACE || wch == 127 || wch == '\b') && wpos > 0) {
                wpos--;
                winput_buffer[wpos] = L'\0'; // 마지막 문자 제거
                pthread_mutex_lock(&window_mutex);
                mvwprintw(input_win, 1, 8, "%ls ", winput_buffer); // 입력창 갱신
                wclrtoeol(input_win);                              // 라인 끝 지우기
                wrefresh(input_win);                               // 입력창 업데이트
                pthread_mutex_unlock(&window_mutex);
            } else if (iswprint(wch) && wpos < (int)(sizeof(winput_buffer) / sizeof(wchar_t) - 1)) {
                winput_buffer[wpos++] = wch; // 입력 문자 추가
                winput_buffer[wpos] = L'\0';
                pthread_mutex_lock(&window_mutex);
                mvwprintw(input_win, 1, 8, "%ls", winput_buffer); // 입력창 갱신
                wrefresh(input_win);                              // 입력창 업데이트
                pthread_mutex_unlock(&window_mutex);
            }
        }

        // 입력 버퍼 정리 (앞뒤 공백 제거)
        wchar_t *wtrimmed_input = winput_buffer;
        while (iswspace(*wtrimmed_input))
            wtrimmed_input++;

        wchar_t *wend = wtrimmed_input + wcslen(wtrimmed_input) - 1;
        while (wend > wtrimmed_input && iswspace(*wend))
            wend--;
        *(wend + 1) = L'\0';

        // Wide char를 멀티바이트 문자열로 변환
        char input_buffer[INPUT_BUFFER_SIZE];
        wcstombs(input_buffer, wtrimmed_input, INPUT_BUFFER_SIZE - 1);
        input_buffer[INPUT_BUFFER_SIZE - 1] = '\0'; // Ensure null termination

        log_event("사용자 입력: '%s'\n", input_buffer);

        // 명령어 파싱 및 전송
        if (strncmp(input_buffer, "/", 1) == 0) {
            // 명령어인 경우 그대로 전송
            send_message_to_server(input_buffer);
        } else if (strlen(input_buffer) > 0) {
            // 기본 채팅 메시지로 간주
            char chat_msg[BUFFER_SIZE];
            snprintf(chat_msg, sizeof(chat_msg), "/chat %s\n", input_buffer);
            send_message_to_server(chat_msg);
        }
    }

    // 스레드 종료 대기
    pthread_join(recv_thread, NULL);

    // 클린업
    cleanup();
    return 0;
}

// 게임 실행 함수
void run_game(int game_duration) {
    // 게임 실행 중 플래그 설정
    game_running = 1;

    // 게임 관련 변수들 초기화
    game_GAME_DURATION = game_duration;
    game_game_over = 0;
    game_score = 0;
    game_level = 1;
    game_word_speed = 1;
    game_word_interval = 2;
    game_frame_delay = 1000000;

    // 입력 처리 관련
    memset(game_typingText, 0, sizeof(game_typingText));
    game_enter_position = 0;

    // 타이머 관련
    time(&game_start_time);

    // 게임 UI 초기화
    pthread_mutex_lock(&window_mutex);
    clear();
    refresh();
    curs_set(FALSE); // 게임 중에는 커서 숨기기
    game_define_colors();
    pthread_mutex_unlock(&window_mutex);

    // 플레이어 초기화
    game_init_player();

    // 게임 쓰레드 생성
    pthread_t game_thread_id;
    if (pthread_create(&game_thread_id, NULL, game_game_thread_func, NULL) != 0) {
        perror("게임 쓰레드 생성 실패");
        return;
    }

    // 메인 쓰레드는 사용자 입력 처리
    while (!game_game_over) {
        game_handle_input();
        // 현재 시간 체크
        time_t current_time;
        time(&current_time);
        if (difftime(current_time, game_start_time) >= game_GAME_DURATION) {
            game_game_over = 1;
            break;
        }
        usleep(10000); // 10ms 대기
    }

    // 쓰레드 종료 대기
    pthread_join(game_thread_id, NULL);

    // 게임 종료 처리
    pthread_mutex_lock(&window_mutex);
    game_cleanup();
    clear();
    refresh();
    initialize_windows(); // 창 재초기화
    curs_set(TRUE);       // 커서 보이기
    pthread_mutex_unlock(&window_mutex);

    // 게임 실행 중 플래그 해제
    game_running = 0;
}

// 게임 색상 정의 함수
void game_define_colors() {
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_CYAN); // 일반 단어 색상: 하얀색 글자, 하늘색 배경
    init_pair(2, COLOR_BLUE, COLOR_CYAN);  // 파워업 단어 색상
    init_pair(3, COLOR_RED, COLOR_CYAN);   // 파워업 단어 색상
    init_pair(4, COLOR_GREEN, COLOR_CYAN); // 플레이어 색상
    init_pair(5, COLOR_BLACK, COLOR_CYAN); // 입력 창 색상
    init_pair(6, COLOR_WHITE, COLOR_CYAN); // 배경색
}

// 플레이어 초기화 함수
void game_init_player() {
    game_player.row = LINES - 2; // 화면 하단에서 2번째 줄
    game_player.col = COLS / 2;  // 화면 중앙
    game_player.symbol = 'A';    // 플레이어 심볼
}

// 플레이어 그리기 함수
void game_draw_player() {
    attron(COLOR_PAIR(4));
    mvaddch(game_player.row, game_player.col, game_player.symbol);
    attroff(COLOR_PAIR(4));
}

// 단어 추가 함수
void game_add_word() {
    GameWordNode *new_word = (GameWordNode *)malloc(sizeof(GameWordNode));
    if (!new_word) {
        perror("메모리 할당 실패");
        exit(EXIT_FAILURE);
    }
    memset(new_word, 0, sizeof(GameWordNode)); // 메모리 초기화

    // 단어 선택
    int is_power_up = 0;
    if (rand() % 100 < 20) { // 20% 확률로 파워업 단어
        is_power_up = 1;
        strcpy(new_word->word, game_powerUpDB[rand() % game_powerUpDB_size]);
    } else {
        strcpy(new_word->word, game_wordDB[rand() % game_wordDB_size]);
    }
    new_word->is_power_up = is_power_up;

    new_word->row = 1;

    // 단어 길이를 고려하여 최대 컬럼 값을 계산
    int max_col = COLS - (int)strlen(new_word->word) - 1;
    if (max_col < 1)
        max_col = 1;

    new_word->col = rand() % max_col;

    new_word->next = NULL;

    // 리스트에 추가
    if (game_word_head == NULL) {
        game_word_head = new_word;
    } else {
        GameWordNode *temp = game_word_head;
        while (temp->next != NULL)
            temp = temp->next;
        temp->next = new_word;
    }
}

// 단어 업데이트 함수
void game_update_words() {
    GameWordNode *current = game_word_head;
    GameWordNode *prev = NULL;

    while (current != NULL) {
        // 기존 위치를 지움 (좌표 값 검사 추가)
        if (current->row >= 0 && current->row < LINES && current->col >= 0 && current->col < COLS) {
            move(current->row, current->col);
            clrtoeol();
        }

        // 단어의 위치를 업데이트
        current->row += game_word_speed;

        // 화면을 벗어난 단어 처리
        if (current->row >= LINES - 2) {
            // 점수 감점
            game_score -= strlen(current->word);

            // 리스트에서 제거
            GameWordNode *temp = current;
            if (prev == NULL) { // 첫 노드인 경우
                game_word_head = current->next;
                current = game_word_head;
            } else {
                prev->next = current->next;
                current = prev->next;
            }
            free(temp);

            continue;
        }

        prev = current;
        current = current->next;
    }
}

// 단어 그리기 함수
void game_draw_words() {
    GameWordNode *current = game_word_head;

    while (current != NULL) {
        // 좌표 값 검사 추가
        if (current->row >= 0 && current->row < LINES && current->col >= 0 && current->col < COLS - (int)strlen(current->word)) {
            if (current->is_power_up) {
                // 파워업 단어 색상
                attron(COLOR_PAIR(3));
                mvprintw(current->row, current->col, "%s", current->word);
                attroff(COLOR_PAIR(3));
            } else {
                // 일반 단어 색상
                attron(COLOR_PAIR(1));
                mvprintw(current->row, current->col, "%s", current->word);
                attroff(COLOR_PAIR(1));
            }
        }
        current = current->next;
    }
}

// 단어 체크 함수
void game_word_Check(char *str) {
    GameWordNode *temp = game_word_head;
    GameWordNode *prev = NULL;

    while (temp != NULL) {
        if (strcmp(temp->word, str) == 0) {
            // 단어 일치 시 점수 증가
            if (temp->is_power_up) {
                game_score += strlen(temp->word) * 2; // 파워업 단어는 추가 점수
            } else {
                game_score += strlen(temp->word);
            }

            // 화면에서 단어 지우기 (좌표 값 검사 추가)
            if (temp->row >= 0 && temp->row < LINES && temp->col >= 0 && temp->col < COLS) {
                move(temp->row, temp->col);
                clrtoeol();
            }

            // 단어 제거
            if (prev == NULL) {
                game_word_head = temp->next;
            } else {
                prev->next = temp->next;
            }
            free(temp);

            // 여기서 점수 체크 및 게임 종료 메시지 전송
            if (game_score >= 150 && !game_game_over) {
                // 게임 종료 메시지 전송
                char game_over_msg[50];
                snprintf(game_over_msg, sizeof(game_over_msg), "GAME_OVER %d\n", game_score);
                send_message_to_server(game_over_msg);
                // 게임 종료 플래그 설정
                game_game_over = 1;
            }

            return;
        }
        prev = temp;
        temp = temp->next;
    }
}

// 사용자 입력 처리 함수
void game_handle_input() {
    int c = wgetch(input_win); // input_win에서 입력 받기
    if (c == ERR)
        return;

    if (c == '\n') {
        game_typingText[game_enter_position] = '\0';
        game_word_Check(game_typingText);

        // 입력 초기화
        memset(game_typingText, 0, sizeof(game_typingText));
        game_enter_position = 0;

        // 입력창 업데이트
        werase(input_win);
        box(input_win, 0, 0);
        mvwprintw(input_win, 1, 1, "Enter Word: ");
        wrefresh(input_win);
    } else if (c == KEY_BACKSPACE || c == 127) {
        if (game_enter_position > 0) {
            game_enter_position--;
            game_typingText[game_enter_position] = '\0';
            mvwprintw(input_win, 1, 13, "%s ", game_typingText);
            wmove(input_win, 1, 13 + game_enter_position);
            wrefresh(input_win);
        }
    } else if (isprint(c) && game_enter_position < (int)(sizeof(game_typingText) - 1)) {
        game_typingText[game_enter_position++] = c;
        game_typingText[game_enter_position] = '\0';
        mvwprintw(input_win, 1, 13, "%s", game_typingText);
        wmove(input_win, 1, 13 + game_enter_position);
        wrefresh(input_win);
    }
}

// 게임 쓰레드 함수
void *game_game_thread_func(void *arg) {
    int frame = 0;

    // 입력창 초기화
    attron(COLOR_PAIR(5));
    mvprintw(LINES - 1, 0, "Enter Word: ");
    attroff(COLOR_PAIR(5));
    refresh();

    while (!game_game_over) {
        frame++;

        // 단어 생성 간격에 도달하면 단어 추가
        if (frame % game_word_interval == 0) {
            game_add_word();
        }

        // 단어 업데이트 (위치 변경 및 그리기)
        game_update_words();

        // 단어 그리기
        game_draw_words();

        // 플레이어 그리기
        game_draw_player();

        // 점수 및 상태 표시
        attron(COLOR_PAIR(5));
        mvprintw(0, 0, "Score: %d  Level: %d", game_score, game_level);
        attroff(COLOR_PAIR(5));

        // 화면 업데이트
        refresh();

        // 레벨 업 로직
        if (game_score >= game_level * 20 && game_level < 10) {
            game_level++;
            if (game_level % 2 == 0) { // 레벨이 짝수일 때만 속도 증가
                game_word_speed++;     // 단어 하강 속도 증가
            }
            if (game_word_interval > 5)
                game_word_interval -= 1;
        }

        // 속도 조절 (프레임당 대기 시간)
        usleep(game_frame_delay); // 500,000 us (0.5초)
    }

    return NULL;
}

// 게임 종료 및 메모리 정리 함수
void game_cleanup() {
    // 단어 리스트 메모리 해제
    GameWordNode *current = game_word_head;
    while (current != NULL) {
        GameWordNode *temp = current;
        current = current->next;
        free(temp);
    }
    game_word_head = NULL;
}
