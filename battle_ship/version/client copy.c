#include <arpa/inet.h>
#include <errno.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// 초기화 및 배치 관련 함수
void initGrid(char grid[GRID_SIZE][GRID_SIZE]) {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            grid[i][j] = '~'; // 물
        }
    }
}

void displayGrid(char grid[GRID_SIZE][GRID_SIZE]) {
    printf("  ");
    for (int i = 0; i < GRID_SIZE; i++)
        printf("%d ", i);
    printf("\n");

    for (int i = 0; i < GRID_SIZE; i++) {
        printf("%d ", i);
        for (int j = 0; j < GRID_SIZE; j++) {
            printf("%c ", grid[i][j]);
        }
        printf("\n");
    }
}

void placeShips(char grid[GRID_SIZE][GRID_SIZE], Ship ships[SHIP_NUM]) {
    for (int i = 0; i < SHIP_NUM; i++) {
        int x, y, orientation;
        while (1) {
            displayGrid(grid);
            printf("\n배치할 배: %s (크기: %d)\n", ships[i].name, ships[i].size);
            printf("배치 시작 좌표를 입력하세요 (x y): ");
            scanf("%d %d", &x, &y);

            printf("방향을 선택하세요 (0: 가로, 1: 세로): ");
            scanf("%d", &orientation);

            int canPlace = 1;
            for (int j = 0; j < ships[i].size; j++) {
                int nx = x + (orientation == 0 ? j : 0);
                int ny = y + (orientation == 1 ? j : 0);

                if (nx >= GRID_SIZE || ny >= GRID_SIZE || grid[ny][nx] != '~') {
                    canPlace = 0;
                    break;
                }
            }

            if (canPlace) {
                for (int j = 0; j < ships[i].size; j++) {
                    int nx = x + (orientation == 0 ? j : 0);
                    int ny = y + (orientation == 1 ? j : 0);
                    grid[ny][nx] = 'S'; // 배를 배치
                }
                break;
            } else {
                printf("유효하지 않은 위치입니다. 다시 시도하세요.\n");
            }
        }
    }
}
void sendGridToServer(char grid[GRID_SIZE][GRID_SIZE]) {
    char buffer[GRID_SIZE * GRID_SIZE + 1];
    int index = 0;

    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            buffer[index++] = grid[i][j];
        }
    }
    buffer[index] = '\0'; // 문자열 종료

    int ret = send(sock, buffer, strlen(buffer), 0);
    if (ret < 0) {
        perror("배치 정보 전송 실패");
        return;
    }
    printf("배치 정보를 서버에 전송했습니다. 전송 크기: %d 바이트\n", ret);
}

// 기존 좌표 입력 및 공격 관련 로직
void gameLoop() {
    while (1) {
        char buf_read[1 << 8], buf_write[1 << 8];
        int x, y, rep;
        do {
            printf("\n좌표를 입력하세요 (x, y): ");
            rep = scanf("%d %d", &x, &y);
        } while (rep != 2);

        // 서버에 공격 좌표 전송
        sprintf(buf_write, "%d %d\n", x, y);
        int ret = write(sock, buf_write, strlen(buf_write));
        if (ret < strlen(buf_write)) {
            printf("\n전송 오류 (전송된 바이트=%d, 오류 메시지=%s)\n",
                   ret, strerror(errno));
            continue;
        }

        // 서버 응답 읽기
        ret = read(sock, buf_read, 256);
        if (ret <= 0) {
            printf("읽기 실패 (바이트 수=%d: %s)\n", ret, strerror(errno));
            break;
        }
        buf_read[ret] = '\0'; // 버퍼에 NULL 추가
        printf("\n서버 응답: %s\n", buf_read);
    }
}

int main(int argc, char **argv) {
    struct sockaddr_in server;

    // 인자와 관련된 오류 처리
    if (argc != 4) {
        fprintf(stderr, "사용법: %s id 서버주소 포트\n", argv[0]);
        exit(1);
    }
    id = argv[1];
    sport = atoi(argv[3]);

    // 소켓 생성
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "소켓 생성 오류: %s\n", strerror(errno));
        exit(1);
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(sport);
    inet_aton(argv[2], &server.sin_addr);

    // 서버에 연결
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        fprintf(stderr, "연결 실패: %s\n", strerror(errno));
        exit(1);
    }

    // 그리드 초기화 및 배치
    char grid[GRID_SIZE][GRID_SIZE];
    Ship ships[SHIP_NUM] = {
        {"항공모함", 5},
        {"전함", 4},
        {"순양함", 3},
        {"잠수함", 3},
        {"구축함", 2}};

    initGrid(grid);
    placeShips(grid, ships);

    // 배치 정보 서버에 전송
    sendGridToServer(grid);

    // 게임 루프
    gameLoop();

    close(sock);
    return 0;
}
