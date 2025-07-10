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
    // 로케일 설정: 프로그램이 다국어 문자를 제대로 처리할 수 있도록 함
    setlocale(LC_ALL, "");

    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    /*
     * 소켓 생성 및 설정
     * 설명: TCP 소켓을 생성하고 서버 주소와 포트를 설정함
     * 입력: 없음
     * 출력: 소켓 생성 성공 시 디스크립터 저장, 실패 시 -1 반환
     */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n소켓 생성 오류\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    /*
     * 서버 주소 변환
     * 설명: 문자열 형태의 IP 주소를 네트워크 바이트 순서로 변환
     * 입력: 로컬호스트 주소
     * 출력: 변환 성공 시 0 이상 반환, 실패 시 -1 반환
     */
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\n잘못된 주소/ 주소를 지원하지 않음\n");
        return -1;
    }

    // ncurses 초기화
    initscr();
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    /*
     * 서버 연결 요청
     * 설명: 소켓을 통해 서버와 연결을 시도
     * 입력: 서버 주소와 포트 정보
     * 출력: 연결 성공 시 0 반환, 실패 시 -1 반환
     */
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        endwin();
        perror("\n연결 실패");
        return -1;
    }
    //시작화면 정의
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
    //시작화면 출력
    while (title_frames[i]) {
        mvprintw(LINES / 2 - 4 + i, (COLS - strlen(title_frames[i])) / 2, "%s", title_frames[i]);
        refresh();
        usleep(frame_delay * 1000);
        i++;
    }

    refresh();
    sleep(2);
    //출력창 및 입력창 설정
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
    //화면 중앙에 대기 문구 출력
    mvprintw(LINES / 2, (COLS - strlen("서버가 시작되기를 기다리는 중...")) / 2, "서버가 시작되기를 기다리는 중...");
    refresh();
    //서버로부터 값 읽은 후 정보에 따른 처리
    /*
     * 서버 응답 처리 루프
     * 설명: 서버로부터 데이터를 읽고 클라이언트에 반영
     * 입력: 서버로부터 받은 메시지
     * 출력: (필요한 경우) 사용자 입력 처리 및 서버로 메시지 전송 
     */
    while (1) {
        int valread = read(sock, buffer, 1023);
        if (valread > 0) {
            buffer[valread] = '\0';
            //게임 준비 
            if (strstr(buffer, "게임에 참여하시겠습니까?") != NULL) {
                char choice[3];
                mvwprintw(input_win, 1, 1, "게임을 수락하시겠습니까? (y/n): ");
                wrefresh(input_win);
                echo();
                wgetnstr(input_win, choice, sizeof(choice) - 1);
                noecho();
                send(sock, choice, strlen(choice), 0);
                werase(input_win);
                box(input_win, 0, 0);
                wrefresh(input_win);
            }
            //시작 대기
            if (strstr(buffer, "다른 플레이어를 기다리는 중입니다...") != NULL) {
                wclear(output_win);
                mvwprintw(output_win, 1, 1, "다른 플레이어가 수락을 기다리고 있습니다...");
                wrefresh(output_win);
            }
            //상대방 거절에 의한 게임 종료
            if (strstr(buffer, "상대 플레이어가 게임을 거부하여 연결을 종료합니다...") != NULL) {
                wclear(output_win);
                mvwprintw(output_win, 1, 1, "다른 플레이어가 거절했습니다. 게임 종료.");
                wrefresh(output_win);
                sleep(2);
                break;
            }
            //플레이어 거절에 의한 게임 종료
            if (strstr(buffer, "게임을 거부하셨습니다.") != NULL) {
                wclear(output_win);
                mvwprintw(output_win, 1, 1, "당신이 게임을 거절했습니다. 게임 종료.");
                wrefresh(output_win);
                sleep(2);
                break;
            }
            //게임시작
            if (strstr(buffer, "게임 시작!") != NULL) {
                wclear(output_win);
                mvwprintw(output_win, 1, 1, "%s", buffer);
                wrefresh(output_win);
            }
            //상대 타일 정보 출력
            if (strstr(buffer, "상대의 타일:") != NULL) {
                wclear(output_win);
                mvwprintw(output_win, 1, 1, "%s", buffer);
                wrefresh(output_win);
            }
            //플레이어 타일 정보 출력
            if (strstr(buffer, "당신의 턴입니다.") != NULL) {
                char input[1024];
                mvwprintw(input_win, 1, 1, "당신의 턴입니다. 움직임을 입력하세요 (인덱스 색상 번호): ");
                wrefresh(input_win);
                echo();
                wgetnstr(input_win, input, sizeof(input) - 1);
                noecho();
                send(sock, input, strlen(input), 0);
                werase(input_win);
                box(input_win, 0, 0);
                wrefresh(input_win);
            }
            //타일을 맞춘 후 타일 다시 추측 여부
            if (strstr(buffer, "다시 추측하시겠습니까?") != NULL) {
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
            //게임 종료시 승리 문구 출력
            if (strstr(buffer, "게임 종료: 당신이 이겼습니다!") != NULL) {
                wclear(output_win);
                mvwprintw(output_win, LINES / 2, (COLS - strlen("당신이 이겼습니다!")) / 2, "당신이 이겼습니다!");
                wrefresh(output_win);
                sleep(3);
                break;
            }
            //게임 종료시 패배 문구 출력 
            if (strstr(buffer, "게임 종료: 당신이 졌습니다!") != NULL) {
                wclear(output_win);
                mvwprintw(output_win, LINES / 2, (COLS - strlen("당신이 졌습니다.")) / 2, "당신이 졌습니다.");
                wrefresh(output_win);
                sleep(3);
                break;
            }
        }
    }

    close(sock);
    endwin();
    return 0;
}