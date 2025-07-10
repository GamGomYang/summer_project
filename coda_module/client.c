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
    // ������ ����: ���α׷��� �ٱ��� ���ڸ� ����� ó���� �� �ֵ��� ��
    setlocale(LC_ALL, "");

    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    /*
     * ���� ���� �� ����
     * ����: TCP ������ �����ϰ� ���� �ּҿ� ��Ʈ�� ������
     * �Է�: ����
     * ���: ���� ���� ���� �� ��ũ���� ����, ���� �� -1 ��ȯ
     */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n���� ���� ����\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    /*
     * ���� �ּ� ��ȯ
     * ����: ���ڿ� ������ IP �ּҸ� ��Ʈ��ũ ����Ʈ ������ ��ȯ
     * �Է�: ����ȣ��Ʈ �ּ�
     * ���: ��ȯ ���� �� 0 �̻� ��ȯ, ���� �� -1 ��ȯ
     */
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\n�߸��� �ּ�/ �ּҸ� �������� ����\n");
        return -1;
    }

    // ncurses �ʱ�ȭ
    initscr();
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    /*
     * ���� ���� ��û
     * ����: ������ ���� ������ ������ �õ�
     * �Է�: ���� �ּҿ� ��Ʈ ����
     * ���: ���� ���� �� 0 ��ȯ, ���� �� -1 ��ȯ
     */
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        endwin();
        perror("\n���� ����");
        return -1;
    }
    //����ȭ�� ����
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
    //����ȭ�� ���
    while (title_frames[i]) {
        mvprintw(LINES / 2 - 4 + i, (COLS - strlen(title_frames[i])) / 2, "%s", title_frames[i]);
        refresh();
        usleep(frame_delay * 1000);
        i++;
    }

    refresh();
    sleep(2);
    //���â �� �Է�â ����
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
    //ȭ�� �߾ӿ� ��� ���� ���
    mvprintw(LINES / 2, (COLS - strlen("������ ���۵Ǳ⸦ ��ٸ��� ��...")) / 2, "������ ���۵Ǳ⸦ ��ٸ��� ��...");
    refresh();
    //�����κ��� �� ���� �� ������ ���� ó��
    /*
     * ���� ���� ó�� ����
     * ����: �����κ��� �����͸� �а� Ŭ���̾�Ʈ�� �ݿ�
     * �Է�: �����κ��� ���� �޽���
     * ���: (�ʿ��� ���) ����� �Է� ó�� �� ������ �޽��� ���� 
     */
    while (1) {
        int valread = read(sock, buffer, 1023);
        if (valread > 0) {
            buffer[valread] = '\0';
            //���� �غ� 
            if (strstr(buffer, "���ӿ� �����Ͻðڽ��ϱ�?") != NULL) {
                char choice[3];
                mvwprintw(input_win, 1, 1, "������ �����Ͻðڽ��ϱ�? (y/n): ");
                wrefresh(input_win);
                echo();
                wgetnstr(input_win, choice, sizeof(choice) - 1);
                noecho();
                send(sock, choice, strlen(choice), 0);
                werase(input_win);
                box(input_win, 0, 0);
                wrefresh(input_win);
            }
            //���� ���
            if (strstr(buffer, "�ٸ� �÷��̾ ��ٸ��� ���Դϴ�...") != NULL) {
                wclear(output_win);
                mvwprintw(output_win, 1, 1, "�ٸ� �÷��̾ ������ ��ٸ��� �ֽ��ϴ�...");
                wrefresh(output_win);
            }
            //���� ������ ���� ���� ����
            if (strstr(buffer, "��� �÷��̾ ������ �ź��Ͽ� ������ �����մϴ�...") != NULL) {
                wclear(output_win);
                mvwprintw(output_win, 1, 1, "�ٸ� �÷��̾ �����߽��ϴ�. ���� ����.");
                wrefresh(output_win);
                sleep(2);
                break;
            }
            //�÷��̾� ������ ���� ���� ����
            if (strstr(buffer, "������ �ź��ϼ̽��ϴ�.") != NULL) {
                wclear(output_win);
                mvwprintw(output_win, 1, 1, "����� ������ �����߽��ϴ�. ���� ����.");
                wrefresh(output_win);
                sleep(2);
                break;
            }
            //���ӽ���
            if (strstr(buffer, "���� ����!") != NULL) {
                wclear(output_win);
                mvwprintw(output_win, 1, 1, "%s", buffer);
                wrefresh(output_win);
            }
            //��� Ÿ�� ���� ���
            if (strstr(buffer, "����� Ÿ��:") != NULL) {
                wclear(output_win);
                mvwprintw(output_win, 1, 1, "%s", buffer);
                wrefresh(output_win);
            }
            //�÷��̾� Ÿ�� ���� ���
            if (strstr(buffer, "����� ���Դϴ�.") != NULL) {
                char input[1024];
                mvwprintw(input_win, 1, 1, "����� ���Դϴ�. �������� �Է��ϼ��� (�ε��� ���� ��ȣ): ");
                wrefresh(input_win);
                echo();
                wgetnstr(input_win, input, sizeof(input) - 1);
                noecho();
                send(sock, input, strlen(input), 0);
                werase(input_win);
                box(input_win, 0, 0);
                wrefresh(input_win);
            }
            //Ÿ���� ���� �� Ÿ�� �ٽ� ���� ����
            if (strstr(buffer, "�ٽ� �����Ͻðڽ��ϱ�?") != NULL) {
                char choice[3];
                mvwprintw(input_win, 1, 1, "�ٽ� �����Ͻðڽ��ϱ�? (y/n): ");
                wrefresh(input_win);
                echo();
                wgetnstr(input_win, choice, sizeof(choice) - 1);
                noecho();
                send(sock, choice, strlen(choice), 0);
                werase(input_win);
                box(input_win, 0, 0);
                wrefresh(input_win);
            }
            //���� ����� �¸� ���� ���
            if (strstr(buffer, "���� ����: ����� �̰���ϴ�!") != NULL) {
                wclear(output_win);
                mvwprintw(output_win, LINES / 2, (COLS - strlen("����� �̰���ϴ�!")) / 2, "����� �̰���ϴ�!");
                wrefresh(output_win);
                sleep(3);
                break;
            }
            //���� ����� �й� ���� ��� 
            if (strstr(buffer, "���� ����: ����� �����ϴ�!") != NULL) {
                wclear(output_win);
                mvwprintw(output_win, LINES / 2, (COLS - strlen("����� �����ϴ�.")) / 2, "����� �����ϴ�.");
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