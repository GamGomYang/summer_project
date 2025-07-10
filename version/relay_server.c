// gcc -o relay_server relay_server.c -lncursesw -lpthread

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 8080
#define MAX_CLIENTS 100

typedef struct {
    int socket;
    char nickname[20];
} Client;

typedef struct {
    Client *player1;
    Client *player2;
} GameSession;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
Client *waiting_client = NULL;

void *handle_game_session(void *arg) {
    GameSession *session = (GameSession *)arg;
    Client *player1 = session->player1;
    Client *player2 = session->player2;

    char buffer[1024];
    int bytes_read;
    int game_over = 0;
    int player_turn = 1; // 1 또는 2

    // START 메시지 전송
    strcpy(buffer, "START");
    send(player1->socket, buffer, strlen(buffer), 0);
    send(player2->socket, buffer, strlen(buffer), 0);

    // 두 클라이언트로부터 READY 메시지 수신
    // 플레이어 1의 READY 수신
    memset(buffer, 0, sizeof(buffer));
    bytes_read = recv(player1->socket, buffer, sizeof(buffer), 0);
    if (bytes_read <= 0) {
        printf("플레이어1 연결 끊김\n");
        close(player1->socket);
        close(player2->socket);
        free(player1);
        free(player2);
        free(session);
        return NULL;
    }
    buffer[bytes_read] = '\0';
    if (strcmp(buffer, "READY") != 0) {
        printf("플레이어1 READY 메시지 수신 오류\n");
        close(player1->socket);
        close(player2->socket);
        free(player1);
        free(player2);
        free(session);
        return NULL;
    }

    // 플레이어 2의 READY 수신
    memset(buffer, 0, sizeof(buffer));
    bytes_read = recv(player2->socket, buffer, sizeof(buffer), 0);
    if (bytes_read <= 0) {
        printf("플레이어2 연결 끊김\n");
        close(player1->socket);
        close(player2->socket);
        free(player1);
        free(player2);
        free(session);
        return NULL;
    }
    buffer[bytes_read] = '\0';
    if (strcmp(buffer, "READY") != 0) {
        printf("플레이어2 READY 메시지 수신 오류\n");
        close(player1->socket);
        close(player2->socket);
        free(player1);
        free(player2);
        free(session);
        return NULL;
    }

    // 두 클라이언트에게 GAME_START 메시지 전송
    strcpy(buffer, "GAME_START");
    send(player1->socket, buffer, strlen(buffer), 0);
    send(player2->socket, buffer, strlen(buffer), 0);
    while (!game_over) {
        if (player_turn == 1) {
            send(player1->socket, "YOUR_TURN", strlen("YOUR_TURN"), 0);
            send(player2->socket, "WAIT", strlen("WAIT"), 0);

            memset(buffer, 0, sizeof(buffer));
            bytes_read = recv(player1->socket, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) {
                printf("플레이어1 연결 끊김\n");
                game_over = 1;
                break;
            }
            buffer[bytes_read] = '\0';

            send(player2->socket, buffer, bytes_read, 0);

            memset(buffer, 0, sizeof(buffer));
            bytes_read = recv(player2->socket, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) {
                printf("플레이어2 연결 끊김\n");
                game_over = 1;
                break;
            }
            buffer[bytes_read] = '\0';

            send(player1->socket, buffer, bytes_read, 0);

            if (strcmp(buffer, "GAME_OVER") == 0) {
                send(player2->socket, "GAME_OVER", strlen("GAME_OVER"), 0);
                game_over = 1;
            } else {
                player_turn = 2;
            }
        } else {
            send(player2->socket, "YOUR_TURN", strlen("YOUR_TURN"), 0);
            send(player1->socket, "WAIT", strlen("WAIT"), 0);

            memset(buffer, 0, sizeof(buffer));
            bytes_read = recv(player2->socket, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) {
                printf("플레이어2 연결 끊김\n");
                game_over = 1;
                break;
            }
            buffer[bytes_read] = '\0';

            send(player1->socket, buffer, bytes_read, 0);

            memset(buffer, 0, sizeof(buffer));
            bytes_read = recv(player1->socket, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) {
                printf("플레이어1 연결 끊김\n");
                game_over = 1;
                break;
            }
            buffer[bytes_read] = '\0';

            send(player2->socket, buffer, bytes_read, 0);

            if (strcmp(buffer, "GAME_OVER") == 0) {
                send(player1->socket, "GAME_OVER", strlen("GAME_OVER"), 0);
                game_over = 1;
            } else {
                player_turn = 1;
            }
        }
    }

    if (game_over) {
        close(player1->socket);
        close(player2->socket);
        free(player1);
        free(player2);
        free(session);
    }

    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t thread_id;

    // 소켓 생성
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

    // 주소와 포트 설정
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("소켓 옵션 설정 실패");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 바인딩
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("바인딩 실패");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 연결 대기
    if (listen(server_fd, 3) < 0) {
        perror("리스닝 실패");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("서버가 시작되었습니다. 클라이언트를 기다리는 중...\n");

    while (1) {
        // 클라이언트 연결 수락
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("클라이언트 연결 실패");
            continue;
        }

        // 닉네임 수신
        Client *new_client = (Client *)malloc(sizeof(Client));
        new_client->socket = new_socket;
        int bytes_read = recv(new_socket, new_client->nickname, sizeof(new_client->nickname), 0);
        if (bytes_read <= 0) {
            perror("닉네임 수신 실패");
            close(new_socket);
            free(new_client);
            continue;
        }

        printf("클라이언트 연결됨: %s\n", new_client->nickname);

        pthread_mutex_lock(&clients_mutex);
        if (waiting_client == NULL) {
            // 대기 중인 클라이언트가 없으면 현재 클라이언트를 대기열에 추가
            waiting_client = new_client;
            // 대기 메시지 전송
            char *msg = "WAITING_FOR_OPPONENT";
            send(new_client->socket, msg, strlen(msg), 0);
            pthread_mutex_unlock(&clients_mutex);
        } else {
            // 대기 중인 클라이언트와 매칭
            GameSession *session = (GameSession *)malloc(sizeof(GameSession));
            session->player1 = waiting_client;
            session->player2 = new_client;
            waiting_client = NULL;
            pthread_mutex_unlock(&clients_mutex);

            // 게임 세션 스레드 생성
            pthread_create(&thread_id, NULL, handle_game_session, (void *)session);
            pthread_detach(thread_id);
        }
    }

    close(server_fd);
    return 0;
}
