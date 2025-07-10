#include <arpa/inet.h> // 네트워크 소켓 관련 헤더
#include <pthread.h>   // POSIX 스레드 관련 헤더
#include <stdio.h>     // 표준 입출력 관련 헤더
#include <stdlib.h>    // 표준 라이브러리 함수 헤더
#include <string.h>    // 문자열 처리 함수 헤더
#include <unistd.h>    // UNIX 표준 함수 헤더

#define MAX_CLIENTS 10  // 최대 클라이언트 수 정의
#define BUFFER_SIZE 256 // 메시지 버퍼 크기 정의

// 클라이언트 정보를 저장하기 위한 구조체
typedef struct {
    int socket;    // 클라이언트 소켓 파일 디스크립터
    char name[32]; // 클라이언트 이름
} Client;

Client clients[MAX_CLIENTS];                             // 연결된 클라이언트 정보를 저장하는 배열
int client_count = 0;                                    // 현재 연결된 클라이언트 수
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER; // 클라이언트 리스트 접근을 동기화하기 위한 뮤텍스

// 모든 클라이언트에게 메시지를 브로드캐스트하는 함수
void broadcast_message(const char *message) {
    pthread_mutex_lock(&client_lock); // 클라이언트 리스트 접근을 동기화하기 위해 뮤텍스 잠금
    for (int i = 0; i < client_count; i++) {
        send(clients[i].socket, message, strlen(message), 0); // 각 클라이언트 소켓으로 메시지 전송
    }
    pthread_mutex_unlock(&client_lock); // 뮤텍스 잠금 해제
}

// 각 클라이언트를 처리하는 스레드 함수
void *handle_client(void *arg) {
    int client_socket = *(int *)arg; // 클라이언트 소켓 파일 디스크립터
    char buffer[BUFFER_SIZE];        // 클라이언트로부터 받은 메시지를 저장할 버퍼
    char name[32];                   // 클라이언트 이름 저장
    int bytes_received;              // 수신된 메시지의 크기

    // 클라이언트 이름 수신
    recv(client_socket, name, sizeof(name), 0);

    // 클라이언트를 리스트에 추가
    pthread_mutex_lock(&client_lock); // 리스트 수정 전 뮤텍스 잠금
    clients[client_count].socket = client_socket;
    strncpy(clients[client_count].name, name, sizeof(name) - 1); // 클라이언트 이름 저장
    client_count++;
    pthread_mutex_unlock(&client_lock); // 리스트 수정 후 뮤텍스 해제

    printf("%s joined the chat.\n", name); // 서버 콘솔에 클라이언트 입장 메시지 출력

    // 클라이언트로부터 메시지를 수신하고 브로드캐스트
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0'; // 수신된 데이터를 null-terminated 문자열로 변환

        char message[BUFFER_SIZE + 32];                             // 포맷된 메시지를 저장할 버퍼
        snprintf(message, sizeof(message), "%s: %s", name, buffer); // "클라이언트 이름: 메시지" 형식으로 포맷
        printf("%s", message);                                      // 서버 콘솔에 수신된 메시지 출력
        broadcast_message(message);                                 // 다른 모든 클라이언트에게 메시지 브로드캐스트
    }

    // 클라이언트 연결이 종료되면 리스트에서 제거
    pthread_mutex_lock(&client_lock); // 리스트 수정 전 뮤텍스 잠금
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == client_socket) {
            clients[i] = clients[client_count - 1]; // 마지막 클라이언트를 현재 위치로 이동
            client_count--;                         // 클라이언트 수 감소
            break;
        }
    }
    pthread_mutex_unlock(&client_lock); // 리스트 수정 후 뮤텍스 해제

    close(client_socket);                // 클라이언트 소켓 닫기
    printf("%s left the chat.\n", name); // 서버 콘솔에 클라이언트 종료 메시지 출력
    return NULL;
}

int main() {
    int server_socket, client_socket;            // 서버 소켓과 클라이언트 소켓
    struct sockaddr_in server_addr, client_addr; // 서버와 클라이언트 주소 정보
    socklen_t addr_size;                         // 클라이언트 주소 크기
    pthread_t tid;                               // 클라이언트 처리용 스레드 ID

    // 서버 소켓 생성
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed"); // 소켓 생성 실패 시 에러 메시지 출력
        exit(1);
    }

    // 서버 주소 설정
    server_addr.sin_family = AF_INET;         // IPv4 사용
    server_addr.sin_port = htons(12345);      // 서버 포트 번호 설정 (12345)
    server_addr.sin_addr.s_addr = INADDR_ANY; // 모든 네트워크 인터페이스에서 접속 허용

    // 소켓과 주소 바인딩
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed"); // 바인딩 실패 시 에러 메시지 출력
        close(server_socket);
        exit(1);
    }

    // 클라이언트 연결 대기열 설정
    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("Listen failed"); // 연결 대기열 설정 실패 시 에러 메시지 출력
        close(server_socket);
        exit(1);
    }

    printf("Server is listening on port 12345...\n"); // 서버 대기 메시지 출력

    // 클라이언트 연결 요청 처리
    while (1) {
        addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size); // 클라이언트 연결 수락
        if (client_socket == -1) {
            perror("Accept failed"); // 연결 수락 실패 시 에러 메시지 출력
            continue;
        }

        pthread_create(&tid, NULL, handle_client, &client_socket); // 새로운 클라이언트 처리용 스레드 생성
        pthread_detach(tid);                                       // 스레드를 분리 상태로 설정 (자동 리소스 정리)
    }

    close(server_socket); // 서버 소켓 닫기
    return 0;
}
