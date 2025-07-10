#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT 12345
#define BUF_SIZE 2048 // 서버에서 보내는 큰 메시지를 수신하기 위해 크기를 늘림

void *receive_messages(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[BUF_SIZE];

    while (1) {
        memset(buffer, 0, BUF_SIZE);
        int len = recv(client_socket, buffer, BUF_SIZE - 1, 0);
        if (len <= 0) {
            if (len == 0) {
                printf("서버가 연결을 종료했습니다.\n");
            } else {
                perror("recv 오류");
            }
            close(client_socket);
            exit(1);
        }
        buffer[len] = '\0';           // 수신된 데이터의 끝에 널 문자 추가
        printf("서버: %s\n", buffer); // 서버 메시지 출력
    }
    return NULL;
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    pthread_t receive_thread;
    char input[BUF_SIZE];

    // 클라이언트 소켓 생성
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("소켓 생성 실패");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    // 서버 연결
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("서버 연결 실패");
        close(client_socket);
        exit(1);
    }

    printf("서버에 연결되었습니다!\n");

    // 서버 메시지 수신 스레드 생성
    if (pthread_create(&receive_thread, NULL, receive_messages, &client_socket) != 0) {
        perror("스레드 생성 실패");
        close(client_socket);
        exit(1);
    }
    pthread_detach(receive_thread);

    // 사용자 입력 처리
    while (1) {
        memset(input, 0, BUF_SIZE);
        printf("단어 입력: ");
        if (fgets(input, BUF_SIZE, stdin) == NULL) {
            printf("입력 오류 또는 EOF 발생.\n");
            break;
        }
        input[strcspn(input, "\n")] = '\0'; // 개행 문자 제거

        // 서버에 입력 전송
        if (send(client_socket, input, strlen(input), 0) == -1) {
            perror("send 오류");
            break;
        }
    }

    close(client_socket);
    return 0;
}
