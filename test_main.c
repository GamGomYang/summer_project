#include <locale.h>
#include <ncursesw/ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

// 게임 선택 화면 구현
void show_game_selection() {
    // 한국어 로케일 설정
    setlocale(LC_ALL, "");
    
    // ncurses 초기화
    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    // 색상 초기화
    init_pair(1, COLOR_MAGENTA, COLOR_BLACK);  // 게임 선택 표시 (주황색 대신 마젠타 사용)
    init_pair(2, COLOR_WHITE, COLOR_BLACK);    // 선택되지 않은 게임
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);   // 프롬프트
    init_pair(4, COLOR_BLUE, COLOR_BLACK);     // 배틀쉽 색상
    init_pair(5, COLOR_RED, COLOR_BLACK);      // 타이핑 게임 색상
    init_pair(6, COLOR_YELLOW, COLOR_BLACK);   // coda 색상
    init_pair(7, COLOR_CYAN, COLOR_BLACK);     // 하늘색 (시작화면 아스키 아트)
    
    // 배틀쉽 아스키 아트
    char *battleship_art[] = {
        " _             _    _    _              _      _        ",
        "| |           | |  | |  | |            | |    (_)       ",
        "| |__    __ _ | |_ | |_ | |  ___   ___ | |__   _  _ __  ",
        "| '_ \\  / _ || __|| __|| | / _ \\ / __|| '_ \\ | || '_ \\ ",
        "| |_) || (_| || |_ | |_ | ||  __/ \\__ \\| | | || || |_) |",
        "|_.__/  \\__,_| \\__| \\__||_| \\___| |___/|_| |_||_|| .__/ ",
        "                                                 | |    ",
        "                                                 |_|    ",
        NULL
    };
    
    // 타이핑 게임 아스키 아트
    char *typing_art[] = {
        " _____           _    _                      ",
        "|_   _|         (_)  (_)                     ",
        "  | |    __ _    _    _   __ _  _ __    __ _ ",
        "  | |   / _` |  | |  | | / _` || '_ \\  / _` |",
        "  | |  | (_| |  | |  | || (_| || | | || (_| |",
        "  \\_/   \\__,_|  | |  | | \\__,_||_| |_| \\__, |",
        "               _/ | _/ |                __/ |",
        "              |__/ |__/                |___/ ",
        NULL
    };
    
    // coda 게임 아스키 아트
    char *coda_art[] = {
        " _____             _        ",
        "/  __ \\           | |       ",
        "| /  \\/  ___    __| |  __ _ ",
        "| |     / _ \\  / _` | / _` |",
        "| \\__/\\| (_) || (_| || (_| |",
        " \\____/ \\___/  \\__,_| \\__,_|",
        "                            ",
        "                            ",
        NULL
    };
    
    // 게임 정보 구조체
    typedef struct {
        char *name;
        char **art;
        char *description;
    } GameInfo;
    
    GameInfo games[] = {
        {
            "BATTLE SHIP",
            battleship_art,
            "전함을 배치하고 상대방의 함대를 찾아 파괴하는 게임"
        },
        {
            "TYPING GAME",
            typing_art,
            "떨어지는 단어를 빠르게 타이핑하여 점수를 얻는 게임"
        },
        {
            "CODA",
            coda_art,
            "색상과 숫자를 맞추는 추리 게임"
        }
    };
    
    int num_games = sizeof(games) / sizeof(games[0]);
    int selected_game = 0;
    int max_y, max_x;
    
    while (1) {
        getmaxyx(stdscr, max_y, max_x);
        clear();
        
        // 제목 출력
        const char *title = "게임 선택";
        int title_x = (max_x - strlen(title)) / 2;
        mvprintw(2, title_x, "%s", title);
        
        // 게임들 출력
        for (int i = 0; i < num_games; i++) {
            int start_y = 5 + i * 15; // 각 게임 사이 간격
            
            // 선택 표시
            if (i == selected_game) {
                attron(COLOR_PAIR(1) | A_BOLD);
                mvprintw(start_y - 1, 2, "▶ ");
                attroff(COLOR_PAIR(1) | A_BOLD);
            } else {
                attron(COLOR_PAIR(2));
                mvprintw(start_y - 1, 2, "  ");
                attroff(COLOR_PAIR(2));
            }
            
            // 게임 이름
            if (i == selected_game) {
                attron(COLOR_PAIR(1) | A_BOLD);
            } else {
                attron(COLOR_PAIR(2));
            }
            
            int name_x = (max_x - strlen(games[i].name)) / 2;
            mvprintw(start_y, name_x, "%s", games[i].name);
            
            if (i == selected_game) {
                attroff(COLOR_PAIR(1) | A_BOLD);
            } else {
                attroff(COLOR_PAIR(2));
            }
            
            // 아스키 아트 출력
            char **art = games[i].art;
            int art_y = start_y + 1;
            int art_x = (max_x - strlen(art[0])) / 2;
            
            for (int j = 0; art[j] != NULL; j++) {
                if (i == selected_game) {
                    // 선택된 게임은 주황색으로 표시
                    attron(COLOR_PAIR(1));
                } else {
                    // 선택되지 않은 게임은 각각의 고유 색상으로 표시
                    if (i == 0) { // 배틀쉽
                        attron(COLOR_PAIR(4));
                    } else if (i == 1) { // 타이핑 게임
                        attron(COLOR_PAIR(5));
                    } else if (i == 2) { // coda
                        attron(COLOR_PAIR(6));
                    } else {
                        attron(COLOR_PAIR(2));
                    }
                }
                mvprintw(art_y + j, art_x, "%s", art[j]);
                if (i == selected_game) {
                    attroff(COLOR_PAIR(1));
                } else {
                    if (i == 0) {
                        attroff(COLOR_PAIR(4));
                    } else if (i == 1) {
                        attroff(COLOR_PAIR(5));
                    } else if (i == 2) {
                        attroff(COLOR_PAIR(6));
                    } else {
                        attroff(COLOR_PAIR(2));
                    }
                }
            }
            
            // 게임 설명
            int desc_y = art_y + 8; // 아스키 아트 아래
            int desc_x = (max_x - strlen(games[i].description)) / 2;
            mvprintw(desc_y, desc_x, "%s", games[i].description);
        }
        
        // 조작 안내
        int help_y = max_y - 5;
        mvprintw(help_y, 2, "↑ ↓ ← → : 게임 선택");
        mvprintw(help_y + 1, 2, "ENTER : 게임 시작");
        mvprintw(help_y + 2, 2, "Q : 종료");
        
        // 프롬프트
        attron(COLOR_PAIR(3) | A_BLINK);
        int prompt_x = (max_x - strlen("< ENTER - 게임 시작하기 >")) / 2;
        mvprintw(max_y - 2, prompt_x, "< ENTER - 게임 시작하기 >");
        attroff(COLOR_PAIR(3) | A_BLINK);
        
        refresh();
        
        // 키 입력 처리
        int ch = getch();
        switch (ch) {
            case KEY_UP:
                selected_game = (selected_game - 1 + num_games) % num_games;
                break;
            case KEY_DOWN:
                selected_game = (selected_game + 1) % num_games;
                break;
            case KEY_LEFT:
                selected_game = (selected_game - 1 + num_games) % num_games;
                break;
            case KEY_RIGHT:
                selected_game = (selected_game + 1) % num_games;
                break;
            case '\n':
            case KEY_ENTER:
                // 게임 시작 처리
                clear();
                mvprintw(max_y / 2, (max_x - 20) / 2, "%s 게임을 시작합니다...", games[selected_game].name);
                refresh();
                sleep(2);
                
                // 여기서 실제 게임 실행 함수 호출
                if (selected_game == 0) {
                    // 배틀쉽 게임 실행
                    mvprintw(max_y / 2 + 1, (max_x - 30) / 2, "배틀쉽 게임 모듈을 실행합니다.");
                } else if (selected_game == 1) {
                    // 타이핑 게임 실행
                    mvprintw(max_y / 2 + 1, (max_x - 30) / 2, "타이핑 게임 모듈을 실행합니다.");
                } else if (selected_game == 2) {
                    // coda 게임 실행
                    mvprintw(max_y / 2 + 1, (max_x - 30) / 2, "coda 게임 모듈을 실행합니다.");
                }
                refresh();
                sleep(2);
                break;
            case 'q':
            case 'Q':
                endwin();
                return;
            default:
                break;
        }
    }
}

// 게임 실행 함수들 (실제 구현 시 각 게임 모듈 호출)
void run_battleship_game() {
    // 배틀쉽 게임 실행 로직
    printf("배틀쉽 게임을 시작합니다.\n");
    // system("./battle_ship/include/battleship");
}

void run_typing_game() {
    // 타이핑 게임 실행 로직
    printf("타이핑 게임을 시작합니다.\n");
    // system("./typing_game/client");
}

void run_coda_game() {
    // coda 게임 실행 로직
    printf("coda 게임을 시작합니다.\n");
    // system("./coda_module/client");
}

int main() {
    setlocale(LC_ALL, "");
    
    // ncurses 초기화
    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    // 색상 초기화 (시작화면용)
    init_pair(7, COLOR_CYAN, COLOR_BLACK);     // 하늘색 (시작화면 아스키 아트)
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);   // 노란색 (깜빡이는 메시지)
    
    // 시작 화면 아스키 아트
    char *start_art[] = {
        " _____                          _                        _ ",
        "|  __ \\                        | |                      | |",
        "| |  \\/  __ _  _ __ ___    ___ | |      __ _  _ __    __| |",
        "| | __  / _` || '_ ` _ \\  / _ \\| |     / _` || '_ \\  / _` |",
        "| |_\\ \\| (_| || | | | | ||  __/| |____| (_| || | | || (_| |",
        " \\____/ \\__,_||_| |_| |_| \\___|\\_____/ \\__,_||_| |_| \\__,_|",
        "                                                           ",
        "                                                           ",
        NULL
    };
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // 시작 화면 출력 (하늘색)
    for (int i = 0; start_art[i] != NULL; i++) {
        int art_x = (max_x - strlen(start_art[i])) / 2;
        attron(COLOR_PAIR(7)); // 하늘색
        mvprintw(max_y / 2 - 5 + i, art_x, "%s", start_art[i]);
        attroff(COLOR_PAIR(7));
        refresh();
        usleep(200000); // 0.2초 딜레이
    }
    
    // 엔터 키 대기 메시지 (반짝이는 효과)
    const char *press_enter = "< ENTER - 게임 시작하기 >";
    int msg_x = (max_x - strlen(press_enter)) / 2;
    
    // 반짝이는 효과 (노란색)
    while (1) {
        // 메시지 표시 (노란색 + 깜빡임)
        attron(COLOR_PAIR(3) | A_BLINK);
        mvprintw(max_y - 3, msg_x, "%s", press_enter);
        attroff(COLOR_PAIR(3) | A_BLINK);
        refresh();
        usleep(500000); // 0.5초 대기
        
        // 메시지 숨기기
        mvprintw(max_y - 3, msg_x, "%*s", strlen(press_enter), "");
        refresh();
        usleep(500000); // 0.5초 대기
        
        // 키 입력 확인 (non-blocking)
        nodelay(stdscr, TRUE);
        int ch = getch();
        if (ch == '\n') {
            break;
        }
        nodelay(stdscr, FALSE);
    }
    
    endwin();
    
    // 게임 선택 화면 실행
    show_game_selection();
    
    return 0;
} 