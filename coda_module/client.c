#include <arpa/inet.h>
#include <locale.h>
#include <ncursesw/ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define PORT 8080

WINDOW *output_win, *input_win;

int main() {
    setlocale(LC_ALL, "");

    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[2048] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n소켓 생성 실패\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\n잘못된 주소/ 주소를 변환할 수 없음\n");
        return -1;
    }

    initscr();
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        endwin();
        perror("\n연결 실패");
        return -1;
    }

    // 타이틀화면
    char *title_frames[] = {
        " _____             _        ",
        "/  __ \\           | |       ",
        "| /  \\/  ___    __| |  __ _ ",
        "| |     / _ \\  / _` | / _` |",
        "| \\__/\\| (_) || (_| || (_| |",
        " \\____/ \\___/  \\__,_| \\__,_|",
        "                            ",
        "                            ",
        NULL};

    int frame_delay = 200;
    int i = 0;
    while (title_frames[i]) {
        mvprintw(LINES / 2 - 4 + i, (COLS - strlen(title_frames[i])) / 2, "%s", title_frames[i]);
        refresh();
        usleep(frame_delay * 1000);
        i++;
    }

    refresh();
    sleep(2);

    int output_win_height = LINES - 5;
    int output_win_width = COLS;
    output_win = newwin(output_win_height, output_win_width, 0, 0);

    int input_win_height = 5;
    int input_win_width = COLS;
    input_win = newwin(input_win_height, input_win_width, output_win_height, 0);

    scrollok(output_win, TRUE);
    box(output_win, 0, 0);
    box(input_win, 0, 0);
    wrefresh(output_win);
    wrefresh(input_win);

    mvprintw(LINES / 2, (COLS - strlen("다른 플레이어가 접속하기를 기다리는 중...")) / 2, "다른 플레이어가 접속하기를 기다리는 중...");
    refresh();

    while (1) {
        int valread = read(sock, buffer, sizeof(buffer) - 1);
        if (valread > 0) {
            buffer[valread] = '\0';

            // 항상 전체 메시지 출력
            wclear(output_win);
            mvwprintw(output_win, 1, 1, "%s", buffer);
            wrefresh(output_win);

            // 게임 참여 여부
            if (strstr(buffer, "게임에 참여하시겠습니까?") != NULL) {
                char choice[3];
                mvwprintw(input_win, 1, 1, "게임에 참여하시겠습니까? (y/n): ");
                wrefresh(input_win);
                echo();
                wgetnstr(input_win, choice, sizeof(choice) - 1);
                noecho();
                send(sock, choice, strlen(choice), 0);
                werase(input_win);
                box(input_win, 0, 0);
                wrefresh(input_win);
            }
            // 게임 종료 메시지
            else if (strstr(buffer, "상대 플레이어가 연결을 해제하여 게임을 종료합니다...") != NULL ||
                     strstr(buffer, "연결을 해제하셨습니다.") != NULL ||
                     strstr(buffer, "게임을 거부하셨습니다.") != NULL ||
                     strstr(buffer, "상대 플레이어가 게임을 거부하여 연결을 종료합니다...") != NULL) {
                sleep(2);
                break;
            }
            // 내 턴 안내 및 타일 정보
            else if (strstr(buffer, "당신의 차례입니다.") != NULL) {
                char input[1024];
                mvwprintw(input_win, 1, 1, "당신의 차례입니다. 좌표를 입력하세요 (예: 1 B 5): ");
                wrefresh(input_win);
                echo();
                wgetnstr(input_win, input, sizeof(input) - 1);
                noecho();
                send(sock, input, strlen(input), 0);
                werase(input_win);
                box(input_win, 0, 0);
                wrefresh(input_win);
            }
            // 다시 추측 여부
            else if (strstr(buffer, "다시 추측하시겠습니까?") != NULL) {
                char choice[3];
                mvwprintw(input_win, 1, 1, "다시 추측하시겠습니까? (y/n): ");
                wrefresh(input_win);
                echo();
                wgetnstr(input_win, choice, sizeof(choice) - 1);
                noecho();
                send(sock, choice, strlen(choice), 0);
                werase(input_win);
                box(input_win, 0, 0);
                wrefresh(input_win);
            }
            // 승리/패배 메시지
            else if (strstr(buffer, "게임 종료: 당신이 이겼습니다!") != NULL) {
                sleep(3);
                break;
            } else if (strstr(buffer, "게임 종료: 당신이 졌습니다!") != NULL) {
                sleep(3);
                break;
            }
        }
    }

    close(sock);
    endwin();
    return 0;
}