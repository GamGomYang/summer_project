#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 256 // 메시지 버퍼 크기

// 서버로부터 메시지를 수신하는 쓰레드 함수
void *receive_messages(void *arg) {
    int socket = *(int *)arg; // 클라이언트 소켓 파일 디스크립터
    char buffer[BUFFER_SIZE]; // 수신된 메시지를 저장할 버퍼
    int bytes_received;       // 수신된 바이트 수

    // 서버로부터 메시지를 계속 수신
    while ((bytes_received = recv(socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0'; // null-terminate 문자열로 변환
        printf("%s", buffer);          // 수신된 메시지 출력
    }

    // 서버와의 연결이 끊긴 경우
    printf("Disconnected from server.\n");
    exit(0); // 클라이언트 종료
}

int main() {
    int client_socket;                   // 클라이언트 소켓 파일 디스크립터
    struct sockaddr_in server_addr;      // 서버 주소 구조체
    char name[32], message[BUFFER_SIZE]; // 사용자 이름과 메시지를 저장할 버퍼
    pthread_t tid;                       // 메시지 수신용 쓰레드 ID

    // 클라이언트 소켓 생성
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed"); // 소켓 생성 실패 시 에러 메시지 출력
        exit(1);
    }

    // 서버 주소 설정
    server_addr.sin_family = AF_INET;                     // IPv4 사용
    server_addr.sin_port = htons(12345);                  // 포트 번호 설정
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // 로컬 서버 주소

    // 서버에 연결 요청
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection to server failed"); // 연결 실패 시 에러 메시지 출력
        close(client_socket);                  // 소켓 닫기
        exit(1);
    }

    // 사용자 이름 입력
    printf("Enter your name: ");
    fgets(name, sizeof(name), stdin); // 사용자 이름 입력
    name[strcspn(name, "\n")] = '\0'; // 개행 문자 제거

    // 사용자 이름 서버로 전송
    send(client_socket, name, strlen(name), 0);

    // 서버로부터 메시지를 수신하는 쓰레드 생성
    pthread_create(&tid, NULL, receive_messages, &client_socket);

    // 메시지 입력 및 서버로 전송
    while (1) {
        fgets(message, sizeof(message), stdin);           // 메시지 입력
        send(client_socket, message, strlen(message), 0); // 메시지 서버로 전송
    }

    // 소켓 닫기 (사실상 실행되지 않음, 메시지 수신 쓰레드에서 종료)
    close(client_socket);
    return 0;
}
