// server.c

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SERVER_PORT 12345
#define BUFFER_SIZE 2048
#define LOG_FILE "server.log"

// 사용 가능한 게임 모드 목록
const char *available_game_modes[] = {
    "산성비",
    "스피드모드",
    "멀티타이핑",
    "타임어택"};
const int available_game_modes_size = sizeof(available_game_modes) / sizeof(available_game_modes[0]);

// 사용자 및 방 구조체 정의
typedef struct user {
    int socket_fd;
    char name[50];
    int room_id; // 현재 참여 중인 방 ID (-1이면 참여하지 않음)
    int is_ready;
    struct user *next; // 글로벌 사용자 목록을 위한 포인터
} User;

// 방 내 사용자 목록을 위한 구조체
typedef struct room_user {
    User *user;
    struct room_user *next;
} RoomUser;

// 점수 노드 구조체
typedef struct score_node {
    User *user;
    int score;
    struct score_node *next;
} ScoreNode;

// 방 구조체 정의
typedef struct room {
    int id;
    char name[100];
    RoomUser *users;
    char game_mode[50];
    int time_limit; // 초 단위
    int ready_count;
    int game_started;
    int game_over;     // 게임 종료 여부 추가
    ScoreNode *scores; // 게임 점수 목록
    int host_fd;       // 호스트 소켓 fd
    struct room *next;
} Room;

// 전역 변수
User *user_head = NULL;
Room *room_head = NULL;
pthread_mutex_t user_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t room_mutex = PTHREAD_MUTEX_INITIALIZER;
int next_room_id = 1;

// 로그 파일 포인터
FILE *log_fp = NULL;

// 함수 선언
void *client_handler(void *arg);
void add_user(int socket_fd, const char *name);
void remove_user(int socket_fd);
User *find_user(int socket_fd);
void broadcast_message(const char *message, int room_id, int exclude_fd);
Room *create_room(const char *name, const char *host_name, int host_fd);
Room *find_room(int room_id);
void add_user_to_room(Room *room, User *user);
void remove_user_from_room(Room *room, User *user);
void send_message(int socket_fd, const char *message);
void send_help_message(int socket_fd);
void send_room_list(int socket_fd);
void send_game_list(int socket_fd);
void start_game(Room *room);
void log_event(const char *format, ...);
void cleanup_server(int signum);

// 점수 추가 함수
void add_score(Room *room, User *user, int score) {
    ScoreNode *new_score = (ScoreNode *)malloc(sizeof(ScoreNode));
    if (!new_score) {
        perror("ScoreNode 메모리 할당 실패");
        return;
    }
    new_score->user = user;
    new_score->score = score;
    new_score->next = room->scores;
    room->scores = new_score;
}

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

// 사용자 추가 함수
void add_user(int socket_fd, const char *name) {
    User *new_user = (User *)malloc(sizeof(User));
    if (!new_user) {
        perror("사용자 메모리 할당 실패");
        return;
    }
    new_user->socket_fd = socket_fd;
    strncpy(new_user->name, name, sizeof(new_user->name) - 1);
    new_user->name[sizeof(new_user->name) - 1] = '\0';
    new_user->room_id = -1;
    new_user->is_ready = 0;
    new_user->next = user_head;
    user_head = new_user;
}

// 사용자 제거 함수
void remove_user(int socket_fd) {
    User *current = user_head;
    User *prev = NULL;

    while (current != NULL) {
        if (current->socket_fd == socket_fd) {
            if (prev == NULL) {
                user_head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

// 사용자 찾기 함수
User *find_user(int socket_fd) {
    User *current = user_head;
    while (current != NULL) {
        if (current->socket_fd == socket_fd) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// 방 찾기 함수
Room *find_room(int room_id) {
    Room *current = room_head;
    while (current != NULL) {
        if (current->id == room_id) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// 방 생성 함수
Room *create_room(const char *name, const char *host_name, int host_fd) {
    Room *new_room = (Room *)malloc(sizeof(Room));
    if (!new_room) {
        perror("방 메모리 할당 실패");
        return NULL;
    }
    new_room->id = next_room_id++;
    strncpy(new_room->name, name, sizeof(new_room->name) - 1);
    new_room->name[sizeof(new_room->name) - 1] = '\0';
    new_room->users = NULL;
    strncpy(new_room->game_mode, "기본모드", sizeof(new_room->game_mode) - 1);
    new_room->game_mode[sizeof(new_room->game_mode) - 1] = '\0';
    new_room->time_limit = 90; // 기본 제한 시간
    new_room->ready_count = 0;
    new_room->game_started = 0;
    new_room->game_over = 0; // 초기화
    new_room->scores = NULL;
    new_room->host_fd = host_fd;
    new_room->next = room_head;
    room_head = new_room;

    log_event("방 생성: ID=%d, 이름=%s, 호스트=%s\n", new_room->id, new_room->name, host_name);
    return new_room;
}

// 방에 사용자 추가 함수
void add_user_to_room(Room *room, User *user) {
    // 중복 추가 방지
    RoomUser *current = room->users;
    while (current != NULL) {
        if (current->user->socket_fd == user->socket_fd) {
            return;
        }
        current = current->next;
    }

    RoomUser *new_room_user = (RoomUser *)malloc(sizeof(RoomUser));
    if (!new_room_user) {
        perror("RoomUser 메모리 할당 실패");
        return;
    }
    new_room_user->user = user;
    new_room_user->next = room->users;
    room->users = new_room_user;
}

// 방에서 사용자 제거 함수
void remove_user_from_room(Room *room, User *user) {
    RoomUser *current = room->users;
    RoomUser *prev = NULL;
    while (current != NULL) {
        if (current->user->socket_fd == user->socket_fd) {
            if (prev == NULL) {
                room->users = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

// 메시지 브로드캐스트 함수 (room_id에 속한 사용자들에게만 전송)
void broadcast_message(const char *message, int room_id, int exclude_fd) {
    pthread_mutex_lock(&room_mutex);
    if (room_id != -1) {
        Room *current_room = find_room(room_id);
        if (current_room != NULL) {
            RoomUser *current_user = current_room->users;
            while (current_user != NULL) {
                if (current_user->user->socket_fd != exclude_fd) {
                    send_message(current_user->user->socket_fd, message);
                }
                current_user = current_user->next;
            }
        }
    } else {
        // 모든 방의 모든 사용자에게 메시지 전송
        Room *room_iter = room_head;
        while (room_iter != NULL) {
            RoomUser *user_iter = room_iter->users;
            while (user_iter != NULL) {
                if (user_iter->user->socket_fd != exclude_fd) {
                    send_message(user_iter->user->socket_fd, message);
                }
                user_iter = user_iter->next;
            }
            room_iter = room_iter->next;
        }
    }
    pthread_mutex_unlock(&room_mutex);
}

// 메시지 전송 함수
void send_message(int socket_fd, const char *message) {
    if (send(socket_fd, message, strlen(message), 0) < 0) {
        perror("메시지 전송 실패");
    } else {
        // 로그 파일에 기록
        if (log_fp != NULL) {
            fprintf(log_fp, "메시지 전송: %s", message);
            fflush(log_fp);
        }
    }
}

// 도움말 메시지 전송 함수
void send_help_message(int socket_fd) {
    const char *help_msg =
        "사용 가능한 명령어:\n"
        "/create_room <방 이름>       : 방을 생성합니다.\n"
        "/join_room <방 ID>          : 방에 참여합니다.\n"
        "/list                      : 방 목록을 조회합니다.\n"
        "/chat <메시지>             : 채팅 메시지를 보냅니다.\n"
        "/set_game <모드> <시간>      : 게임 설정을 변경합니다. (방장만 가능)\n"
        "/game_list                 : 사용 가능한 게임 모드를 조회합니다.\n"
        "/ready                     : 게임 준비를 완료합니다.\n"
        "/help                      : 도움말을 표시합니다.\n";
    send_message(socket_fd, help_msg);
    log_event("HELP 메시지 전송: %s", help_msg);
}

// 게임 목록 전송 함수
void send_game_list(int socket_fd) {
    char game_list_msg[BUFFER_SIZE] = "사용 가능한 게임 모드:\n";
    for (int i = 0; i < available_game_modes_size; i++) {
        char mode_info[100];
        snprintf(mode_info, sizeof(mode_info), "%d. %s\n", i + 1, available_game_modes[i]);
        strncat(game_list_msg, mode_info, sizeof(game_list_msg) - strlen(game_list_msg) - 1);
    }
    send_message(socket_fd, game_list_msg);
    log_event("GAME_LIST 메시지 전송: %s", game_list_msg);
}

// 방 목록 전송 함수
void send_room_list(int socket_fd) {
    pthread_mutex_lock(&room_mutex);
    if (room_head == NULL) {
        send_message(socket_fd, "현재 사용 가능한 방이 없습니다.\n");
        printf("현재 사용 가능한 방이 없습니다. 메시지 전송\n");
        log_event("현재 사용 가능한 방이 없습니다. 메시지 전송\n");
    } else {
        char list_msg[BUFFER_SIZE] = "현재 방 목록:\n";
        Room *current_room = room_head;
        while (current_room != NULL) {
            char room_info[256]; // 버퍼 크기 증가
            snprintf(room_info, sizeof(room_info), "방 ID: %d, 방 이름: %s, 게임 모드: %s, 시간 제한: %d초\n",
                     current_room->id, current_room->name, current_room->game_mode, current_room->time_limit);
            strncat(list_msg, room_info, sizeof(list_msg) - strlen(list_msg) - 1);
            current_room = current_room->next;
        }
        send_message(socket_fd, list_msg);
        printf("방 목록 전송: %s", list_msg);
        log_event("방 목록 전송: %s", list_msg);
    }
    pthread_mutex_unlock(&room_mutex);
}

// 게임 시작 함수
void start_game(Room *room) {
    // 게임 시작 메시지 전송
    char msg[BUFFER_SIZE];
    snprintf(msg, sizeof(msg), "GAME_STARTED\n");
    broadcast_message(msg, room->id, -1);
    log_event("GAME_STARTED 메시지 브로드캐스트: %s", msg);

    room->game_started = 1;
    room->game_over = 0; // 게임 종료 상태 초기화

    // 게임 시작 시점에 scores 초기화
    pthread_mutex_lock(&room_mutex);
    // 이전 점수 초기화
    ScoreNode *current_score = room->scores;
    while (current_score != NULL) {
        ScoreNode *temp = current_score;
        current_score = current_score->next;
        free(temp);
    }
    room->scores = NULL;
    pthread_mutex_unlock(&room_mutex);
}

// 서버 종료 시 클린업 함수
void cleanup_server(int signum) {
    printf("\n서버 종료 시그널 수신. 클린업을 진행합니다...\n");
    log_event("서버 종료 시그널 수신. 클린업을 진행합니다...\n");

    // 모든 사용자에게 서버 종료 메시지 전송
    broadcast_message("SERVER_SHUTDOWN\n", -1, -1);

    // 모든 소켓 닫기
    pthread_mutex_lock(&user_mutex);
    User *current = user_head;
    while (current != NULL) {
        close(current->socket_fd);
        current = current->next;
    }
    pthread_mutex_unlock(&user_mutex);

    // 방 메모리 해제
    pthread_mutex_lock(&room_mutex);
    Room *room_current = room_head;
    while (room_current != NULL) {
        // 방 내 사용자 목록 해제
        RoomUser *room_user = room_current->users;
        while (room_user != NULL) {
            RoomUser *temp_room_user = room_user;
            room_user = room_user->next;
            free(temp_room_user);
        }

        // 점수 목록 해제
        ScoreNode *score_current = room_current->scores;
        while (score_current != NULL) {
            ScoreNode *temp_score = score_current;
            score_current = score_current->next;
            free(temp_score);
        }

        Room *temp_room = room_current;
        room_current = room_current->next;
        free(temp_room);
    }
    pthread_mutex_unlock(&room_mutex);

    // 사용자 메모리 해제
    pthread_mutex_lock(&user_mutex);
    User *user_current = user_head;
    while (user_current != NULL) {
        User *temp_user = user_current;
        user_current = user_current->next;
        free(temp_user);
    }
    pthread_mutex_unlock(&user_mutex);

    // 로그 파일 닫기
    if (log_fp != NULL) {
        fclose(log_fp);
    }

    printf("서버가 정상적으로 종료되었습니다.\n");
    exit(0);
}

// 클라이언트 핸들러 함수
void *client_handler(void *arg) {
    int socket_fd = (intptr_t)arg;
    char buffer[BUFFER_SIZE];
    int bytes_read;
    char name[50] = {0};
    int current_room_id = -1; // 사용자가 속한 방 ID

    // 클라이언트로부터 이름 수신
    bytes_read = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        printf("소켓 FD %d에서 이름을 수신하지 못했습니다. 연결 종료.\n", socket_fd);
        log_event("소켓 FD %d에서 이름을 수신하지 못했습니다. 연결 종료.\n", socket_fd);
        close(socket_fd);
        pthread_exit(NULL);
    }
    buffer[bytes_read] = '\0';
    strncpy(name, buffer, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';

    printf("사용자 이름 수신: %s (소켓 FD %d)\n", name, socket_fd);
    log_event("사용자 이름 수신: %s (소켓 FD %d)\n", name, socket_fd);

    // 사용자 추가
    pthread_mutex_lock(&user_mutex);
    add_user(socket_fd, name);
    pthread_mutex_unlock(&user_mutex);

    // 환영 메시지 전송 (WELCOME <name>)
    char welcome_msg[BUFFER_SIZE];
    snprintf(welcome_msg, sizeof(welcome_msg), "WELCOME %s\n", name);
    send_message(socket_fd, welcome_msg);
    printf("환영 메시지 전송: %s", welcome_msg);
    log_event("환영 메시지 전송: %s", welcome_msg);

    // 메인 루프
    while ((bytes_read = recv(socket_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        printf("받은 메시지 from %s: %s", name, buffer);
        log_event("받은 메시지 from %s: %s", name, buffer);

        // 메시지 파싱
        if (strncmp(buffer, "/create_room ", 13) == 0) {
            char room_name[100];
            sscanf(buffer + 13, "%99[^\n]", room_name);
            printf("명령어: /create_room, 방 이름: %s\n", room_name);
            log_event("명령어: /create_room, 방 이름: %s\n", room_name);

            // 방 생성
            pthread_mutex_lock(&room_mutex);
            Room *new_room = create_room(room_name, name, socket_fd);
            pthread_mutex_unlock(&room_mutex);

            if (new_room) {
                // 방 생성 메시지 전송 (ROOM_CREATED <room_id> <room_name>)
                char msg[BUFFER_SIZE];
                snprintf(msg, sizeof(msg), "ROOM_CREATED %d %s\n", new_room->id, new_room->name);
                send_message(socket_fd, msg);
                printf("방 생성 메시지 전송: %s", msg);
                log_event("방 생성 메시지 전송: %s", msg);

                // 자동으로 방장(호스트)을 방에 참여시킴
                pthread_mutex_lock(&room_mutex);
                Room *room = find_room(new_room->id);
                pthread_mutex_unlock(&room_mutex);

                if (room) {
                    pthread_mutex_lock(&user_mutex);
                    User *host_user = find_user(socket_fd);
                    pthread_mutex_unlock(&user_mutex);

                    if (host_user) {
                        pthread_mutex_lock(&room_mutex);
                        add_user_to_room(room, host_user);
                        host_user->room_id = room->id;
                        pthread_mutex_unlock(&room_mutex);

                        current_room_id = room->id;
                        printf("사용자 %s가 방 ID %d에 참여했습니다.\n", name, room->id);
                        log_event("사용자 %s가 방 ID %d에 참여했습니다.\n", name, room->id);

                        // 사용자 입장 메시지 전송 (USER_JOINED <name>)
                        char join_msg[BUFFER_SIZE];
                        snprintf(join_msg, sizeof(join_msg), "USER_JOINED %s\n", host_user->name);
                        broadcast_message(join_msg, room->id, socket_fd); // exclude_fd를 발신자 제외
                        printf("USER_JOINED 메시지 브로드캐스트: %s", join_msg);
                        log_event("USER_JOINED 메시지 브로드캐스트: %s", join_msg);
                    }
                }
            } else {
                send_message(socket_fd, "ERROR 방 생성에 실패했습니다.\n");
                printf("방 생성 실패 메시지 전송\n");
                log_event("방 생성 실패 메시지 전송\n");
            }
        } else if (strncmp(buffer, "/join_room ", 11) == 0) {
            int room_id;
            sscanf(buffer + 11, "%d", &room_id);
            printf("명령어: /join_room, 방 ID: %d\n", room_id);
            log_event("명령어: /join_room, 방 ID: %d\n", room_id);

            pthread_mutex_lock(&room_mutex);
            Room *room = find_room(room_id);
            pthread_mutex_unlock(&room_mutex);

            if (room) {
                pthread_mutex_lock(&user_mutex);
                User *user = find_user(socket_fd);
                pthread_mutex_unlock(&user_mutex);

                if (user->room_id != -1) {
                    send_message(socket_fd, "ERROR 이미 방에 참여 중입니다.\n");
                    printf("이미 방에 참여 중임을 알리는 메시지 전송\n");
                    log_event("이미 방에 참여 중임을 알리는 메시지 전송\n");
                    continue;
                }

                pthread_mutex_lock(&room_mutex);
                add_user_to_room(room, user);
                user->room_id = room->id;
                pthread_mutex_unlock(&room_mutex);

                current_room_id = room_id;
                printf("사용자 %s가 방 ID %d에 참여했습니다.\n", name, room_id);
                log_event("사용자 %s가 방 ID %d에 참여했습니다.\n", name, room_id);

                // 사용자 입장 메시지 전송 (USER_JOINED <name>)
                char msg[BUFFER_SIZE];
                snprintf(msg, sizeof(msg), "USER_JOINED %s\n", user->name);
                broadcast_message(msg, room_id, socket_fd); // exclude_fd를 발신자 제외
                printf("USER_JOINED 메시지 브로드캐스트: %s", msg);
                log_event("USER_JOINED 메시지 브로드캐스트: %s", msg);
            } else {
                send_message(socket_fd, "ERROR 존재하지 않는 방 ID입니다.\n");
                printf("존재하지 않는 방 ID 메시지 전송\n");
                log_event("존재하지 않는 방 ID 메시지 전송\n");
            }
        } else if (strncmp(buffer, "/chat ", 6) == 0) {
            if (current_room_id == -1) {
                send_message(socket_fd, "ERROR 방에 먼저 참여해야 합니다.\n");
                printf("방에 참여하지 않은 상태에서 채팅 시도\n");
                log_event("방에 참여하지 않은 상태에서 채팅 시도\n");
                continue;
            }

            char chat_msg[2000]; // 채팅 메시지 길이 제한

            // 채팅 메시지의 길이를 제한하여 버퍼 오버플로우 방지
            sscanf(buffer + 6, "%1999[^\n]", chat_msg);
            printf("명령어: /chat, 메시지: %s\n", chat_msg);
            log_event("명령어: /chat, 메시지: %s\n", chat_msg);

            // 채팅 메시지 브로드캐스트 (CHAT <name>: <message>)
            char formatted_msg[BUFFER_SIZE];
            // snprintf을 사용하여 버퍼 오버플로우 방지
            snprintf(formatted_msg, sizeof(formatted_msg), "CHAT %s: %s\n", name, chat_msg);
            broadcast_message(formatted_msg, current_room_id, -1);
            printf("CHAT 메시지 브로드캐스트: %s", formatted_msg);
            log_event("CHAT 메시지 브로드캐스트: %s", formatted_msg);
        }

        // GAME_OVER 처리 (점수 없이)
        else if (strncmp(buffer, "GAME_OVER", 9) == 0 && strlen(buffer) == 9) {
            log_event("GAME_OVER 메시지를 처리 중입니다. 발신자: 소켓 FD %d\n", socket_fd);

            // 모든 클라이언트에게 게임 종료 메시지 전송
            handle_gameover_all_clients(socket_fd);
            continue;
        }

        //***********************************************

        else if (strncmp(buffer, "GAME_OVER ", 10) == 0) {
            // GAME_OVER 처리
            if (current_room_id == -1) {
                send_message(socket_fd, "ERROR 방에 먼저 참여해야 합니다.\n");
                printf("방에 참여하지 않은 상태에서 GAME_OVER 시도\n");
                log_event("방에 참여하지 않은 상태에서 GAME_OVER 시도\n");
                continue;
            }

            int user_score;
            sscanf(buffer + 10, "%d", &user_score);
            printf("명령어: GAME_OVER, 점수: %d\n", user_score);
            log_event("명령어: GAME_OVER, 점수: %d\n", user_score);

            // 현재 사용자가 속한 방 찾기
            pthread_mutex_lock(&room_mutex);
            Room *current_room = find_room(current_room_id);
            if (current_room == NULL) {
                pthread_mutex_unlock(&room_mutex);
                send_message(socket_fd, "ERROR 방을 찾을 수 없습니다.\n");
                printf("방을 찾을 수 없음 메시지 전송\n");
                log_event("방을 찾을 수 없음 메시지 전송\n");
                continue;
            }

            // 게임이 이미 종료되었는지 확인
            if (current_room->game_over == 1) {
                pthread_mutex_unlock(&room_mutex);
                send_message(socket_fd, "ERROR 게임이 이미 종료되었습니다.\n");
                printf("게임이 이미 종료됨 메시지 전송\n");
                log_event("게임이 이미 종료됨 메시지 전송\n");
                continue;
            }

            // 점수 기록
            pthread_mutex_lock(&user_mutex);
            User *user = find_user(socket_fd);
            pthread_mutex_unlock(&user_mutex);

            if (user == NULL) {
                pthread_mutex_unlock(&room_mutex);
                send_message(socket_fd, "ERROR 사용자를 찾을 수 없습니다.\n");
                printf("사용자를 찾을 수 없음 메시지 전송\n");
                log_event("사용자를 찾을 수 없음 메시지 전송\n");
                continue;
            }

            add_score(current_room, user, user_score);

            // 게임 종료 상태로 설정
            current_room->game_over = 1;

            // 승자 결정
            ScoreNode *max_score_node = current_room->scores;
            ScoreNode *iter = current_room->scores->next;
            while (iter != NULL) {
                if (iter->score > max_score_node->score) {
                    max_score_node = iter;
                }
                iter = iter->next;
            }

            // 승자 메시지 브로드캐스트 (발신자 제외)
            char winner_msg[BUFFER_SIZE];
            snprintf(winner_msg, sizeof(winner_msg), "GAME_OVER\n승자가 결정되었습니다: %s님!\n", max_score_node->user->name);
            broadcast_message(winner_msg, current_room_id, socket_fd); // exclude_fd를 발신자 제외
            printf("GAME_OVER 메시지 브로드캐스트: %s", winner_msg);
            log_event("GAME_OVER 메시지 브로드캐스트: %s", winner_msg);

            // 게임 상태 초기화
            current_room->game_started = 0;

            // 점수 목록 초기화
            ScoreNode *temp;
            while (current_room->scores != NULL) {
                temp = current_room->scores;
                current_room->scores = current_room->scores->next;
                free(temp);
            }
            pthread_mutex_unlock(&room_mutex);
        } else if (strncmp(buffer, "/set_game ", 10) == 0) {
            if (current_room_id == -1) {
                send_message(socket_fd, "ERROR 방에 먼저 참여해야 합니다.\n");
                printf("방에 참여하지 않은 상태에서 게임 설정 시도\n");
                log_event("방에 참여하지 않은 상태에서 게임 설정 시도\n");
                continue;
            }

            char game_mode[50];
            int time_limit;
            int parsed = sscanf(buffer + 10, "%49s %d", game_mode, &time_limit);

            if (parsed < 2) {
                send_message(socket_fd, "ERROR 올바른 형식으로 입력하세요. 예: /set_game <모드> <시간>\n");
                printf("잘못된 /set_game 명령어 형식\n");
                log_event("잘못된 /set_game 명령어 형식\n");
                continue;
            }

            printf("명령어: /set_game, 모드: %s, 시간 제한: %d\n", game_mode, time_limit);
            log_event("명령어: /set_game, 모드: %s, 시간 제한: %d\n", game_mode, time_limit);

            pthread_mutex_lock(&room_mutex);
            Room *current_room = find_room(current_room_id);
            pthread_mutex_unlock(&room_mutex);

            if (current_room == NULL) {
                send_message(socket_fd, "ERROR 방을 찾을 수 없습니다.\n");
                printf("방을 찾을 수 없음 메시지 전송\n");
                log_event("방을 찾을 수 없음 메시지 전송\n");
                continue;
            }

            // 방장이 아닌 경우
            if (current_room->host_fd != socket_fd) {
                send_message(socket_fd, "ERROR 게임 설정은 방장만 할 수 있습니다.\n");
                printf("방장이 아닌 사용자가 게임 설정 시도\n");
                log_event("방장이 아닌 사용자가 게임 설정 시도\n");
                continue;
            }

            // 게임 설정 업데이트
            pthread_mutex_lock(&room_mutex);
            strncpy(current_room->game_mode, game_mode, sizeof(current_room->game_mode) - 1);
            current_room->game_mode[sizeof(current_room->game_mode) - 1] = '\0';
            current_room->time_limit = time_limit;
            current_room->ready_count = 0; // 초기화
            current_room->game_started = 0;
            pthread_mutex_unlock(&room_mutex);
            printf("게임 설정 업데이트: 모드=%s, 시간 제한=%d\n", current_room->game_mode, current_room->time_limit);
            log_event("게임 설정 업데이트: 모드=%s, 시간 제한=%d\n", current_room->game_mode, current_room->time_limit);

            // 게임 설정 완료 메시지 전송 (GAME_SETTINGS <game_mode> <time_limit>)
            char msg[BUFFER_SIZE];
            snprintf(msg, sizeof(msg), "GAME_SETTINGS %s %d\n", current_room->game_mode, current_room->time_limit);
            broadcast_message(msg, current_room_id, socket_fd); // exclude_fd를 발신자 제외
            printf("GAME_SETTINGS 메시지 브로드캐스트: %s", msg);
            log_event("GAME_SETTINGS 메시지 브로드캐스트: %s", msg);
        } else if (strncmp(buffer, "/ready", 6) == 0) {
            if (current_room_id == -1) {
                send_message(socket_fd, "ERROR 방에 먼저 참여해야 합니다.\n");
                printf("방에 참여하지 않은 상태에서 READY 시도\n");
                log_event("방에 참여하지 않은 상태에서 READY 시도\n");
                continue;
            }

            pthread_mutex_lock(&room_mutex);
            Room *current_room = find_room(current_room_id);
            if (current_room == NULL) {
                pthread_mutex_unlock(&room_mutex);
                send_message(socket_fd, "ERROR 방을 찾을 수 없습니다.\n");
                printf("방을 찾을 수 없음 메시지 전송\n");
                log_event("방을 찾을 수 없음 메시지 전송\n");
                continue;
            }

            // 사용자의 준비 상태 확인
            pthread_mutex_lock(&user_mutex);
            User *user = find_user(socket_fd);
            if (user->is_ready) {
                pthread_mutex_unlock(&user_mutex);
                send_message(socket_fd, "ERROR 이미 READY 상태입니다.\n");
                printf("이미 READY 상태임을 알리는 메시지 전송\n");
                log_event("이미 READY 상태임을 알리는 메시지 전송\n");
                pthread_mutex_unlock(&room_mutex);
                continue;
            }
            user->is_ready = 1;
            pthread_mutex_unlock(&user_mutex);

            current_room->ready_count += 1;
            int total_users = 0;
            RoomUser *user_iter = current_room->users;
            while (user_iter != NULL) {
                total_users++;
                user_iter = user_iter->next;
            }

            int ready_all = (current_room->ready_count >= total_users);
            pthread_mutex_unlock(&room_mutex);

            printf("사용자 %s가 READY 상태 (%d/%d)\n", name, current_room->ready_count, total_users);
            log_event("사용자 %s가 READY 상태 (%d/%d)\n", name, current_room->ready_count, total_users);

            if (ready_all && !current_room->game_started) {
                // 게임 시작 메시지 전송 (GAME_STARTED)
                char msg[BUFFER_SIZE];
                snprintf(msg, sizeof(msg), "GAME_STARTED\n");
                broadcast_message(msg, current_room_id, -1);
                printf("GAME_STARTED 메시지 브로드캐스트: %s", msg);
                log_event("GAME_STARTED 메시지 브로드캐스트: %s", msg);

                // 게임 시작 로직 호출
                start_game(current_room);
            } else {
                send_message(socket_fd, "레디되었습니다. 모든 플레이어가 레디를 입력하면 게임이 시작됩니다.\n");
                printf("레디 메시지 전송\n");
                log_event("레디 메시지 전송\n");
            }
        } else if (strncmp(buffer, "/game_list", 10) == 0) {
            printf("명령어: /game_list\n");
            log_event("명령어: /game_list\n");
            send_game_list(socket_fd);
            printf("GAME_LIST 메시지 전송\n");
            log_event("GAME_LIST 메시지 전송\n");
        } else if (strncmp(buffer, "/help", 5) == 0) {
            printf("명령어: /help\n");
            log_event("명령어: /help\n");
            send_help_message(socket_fd);
            printf("HELP 메시지 전송\n");
            log_event("HELP 메시지 전송\n");
        } else if (strncmp(buffer, "/list", 5) == 0) {
            printf("명령어: /list\n");
            log_event("명령어: /list\n");
            send_room_list(socket_fd);
            printf("방 목록 전송\n");
            log_event("방 목록 전송\n");
        } else if (strncmp(buffer, "SCORE ", 6) == 0) {
            if (current_room_id == -1) {
                send_message(socket_fd, "ERROR 방에 먼저 참여해야 합니다.\n");
                printf("방에 참여하지 않은 상태에서 SCORE 시도\n");
                log_event("방에 참여하지 않은 상태에서 SCORE 시도\n");
                continue;
            }

            int user_score;
            sscanf(buffer + 6, "%d", &user_score);
            printf("명령어: SCORE, 점수: %d\n", user_score);
            log_event("명령어: SCORE, 점수: %d\n", user_score);

            // 현재 사용자가 속한 방 찾기
            pthread_mutex_lock(&room_mutex);
            Room *current_room = find_room(current_room_id);
            if (current_room == NULL) {
                pthread_mutex_unlock(&room_mutex);
                send_message(socket_fd, "ERROR 방을 찾을 수 없습니다.\n");
                printf("방을 찾을 수 없음 메시지 전송\n");
                log_event("방을 찾을 수 없음 메시지 전송\n");
                continue;
            }

            // 점수 기록
            pthread_mutex_lock(&user_mutex);
            User *user = find_user(socket_fd);
            pthread_mutex_unlock(&user_mutex);

            if (user == NULL) {
                pthread_mutex_unlock(&room_mutex);
                send_message(socket_fd, "ERROR 사용자를 찾을 수 없습니다.\n");
                printf("사용자를 찾을 수 없음 메시지 전송\n");
                log_event("사용자를 찾을 수 없음 메시지 전송\n");
                continue;
            }

            add_score(current_room, user, user_score);

            // 모든 점수가 수신되었는지 확인
            int total_users = 0;
            int scores_received = 0;
            RoomUser *user_iter = current_room->users;
            while (user_iter != NULL) {
                total_users++;
                user_iter = user_iter->next;
            }

            ScoreNode *score_iter = current_room->scores;
            while (score_iter != NULL) {
                scores_received++;
                score_iter = score_iter->next;
            }

            if (scores_received >= total_users) {
                // 승자 결정
                ScoreNode *max_score_node = current_room->scores;
                ScoreNode *iter = current_room->scores->next;
                while (iter != NULL) {
                    if (iter->score > max_score_node->score) {
                        max_score_node = iter;
                    }
                    iter = iter->next;
                }

                // 승자 메시지 브로드캐스트 (발신자 제외)
                char winner_msg[BUFFER_SIZE];
                snprintf(winner_msg, sizeof(winner_msg), "GAME_OVER\n승자가 결정되었습니다: %s님!\n", max_score_node->user->name);
                broadcast_message(winner_msg, current_room_id, -1); // exclude_fd를 -1로 설정하여 모든 클라이언트에게 전송
                printf("GAME_OVER 메시지 브로드캐스트: %s", winner_msg);
                log_event("GAME_OVER 메시지 브로드캐스트: %s", winner_msg);

                // 게임 상태 초기화
                current_room->game_started = 0;

                // 점수 목록 초기화
                ScoreNode *temp;
                while (current_room->scores != NULL) {
                    temp = current_room->scores;
                    current_room->scores = current_room->scores->next;
                    free(temp);
                }
            }
            pthread_mutex_unlock(&room_mutex);
        } else {
            send_message(socket_fd, "ERROR 알 수 없는 명령어입니다.\n");
            printf("알 수 없는 명령어 메시지 전송\n");
            log_event("알 수 없는 명령어 메시지 전송\n");
        }
    }

    // 클라이언트 연결 종료 처리
    printf("사용자 %s가 연결을 종료했습니다.\n", name);
    log_event("사용자 %s가 연결을 종료했습니다.\n", name);

    // 사용자가 참여 중인 방에서 제거
    if (current_room_id != -1) {
        pthread_mutex_lock(&room_mutex);
        Room *current_room = find_room(current_room_id);
        if (current_room != NULL) {
            User *user_to_remove = find_user(socket_fd);
            if (user_to_remove != NULL) {
                remove_user_from_room(current_room, user_to_remove);
                user_to_remove->room_id = -1; // 방 참여 상태 초기화

                // 사용자 퇴장 메시지 전송 (USER_LEFT <name>)
                char left_msg[BUFFER_SIZE];
                snprintf(left_msg, sizeof(left_msg), "USER_LEFT %s\n", name);
                broadcast_message(left_msg, current_room_id, socket_fd); // exclude_fd를 발신자 제외
                printf("USER_LEFT 메시지 브로드캐스트: %s", left_msg);
                log_event("USER_LEFT 메시지 브로드캐스트: %s", left_msg);

                // 만약 방장이 퇴장했다면, 다른 사용자를 새로운 방장으로 설정
                if (current_room->host_fd == socket_fd) {
                    RoomUser *new_host = current_room->users;
                    if (new_host != NULL) {
                        current_room->host_fd = new_host->user->socket_fd;
                        // 호스트 변경 메시지 전송
                        char host_msg[BUFFER_SIZE];
                        snprintf(host_msg, sizeof(host_msg), "HOST_CHANGED %s\n", new_host->user->name);
                        broadcast_message(host_msg, current_room_id, -1); // exclude_fd를 -1로 설정하여 모든 클라이언트에게 전송
                        printf("HOST_CHANGED 메시지 브로드캐스트: %s", host_msg);
                        log_event("HOST_CHANGED 메시지 브로드캐스트: %s", host_msg);
                    } else {
                        // 방에 사용자가 없으면 방 삭제
                        // 방 삭제 로직을 추가할 수 있음
                    }
                }
            }
        }
        pthread_mutex_unlock(&room_mutex);
    }

    // 사용자 제거
    pthread_mutex_lock(&user_mutex);
    remove_user(socket_fd);
    pthread_mutex_unlock(&user_mutex);

    close(socket_fd);
    pthread_exit(NULL);
}

// GAME_OVER 처리 함수
void handle_gameover_all_clients(int sender_fd) {
    pthread_mutex_lock(&user_mutex);
    User *sender = find_user(sender_fd); // 게임 오버를 보낸 사용자 찾기
    if (sender == NULL) {
        pthread_mutex_unlock(&user_mutex);
        printf("게임 오버 메시지를 보낸 사용자를 찾을 수 없습니다.\n");
        return;
    }

    // 게임 오버 메시지 구성
    char gameover_message[BUFFER_SIZE];
    snprintf(gameover_message, sizeof(gameover_message),
             "GAME_OVER\n게임이 종료되었습니다. 최종승리자 : %s\n",
             sender->name);

    // 모든 클라이언트에게 메시지 전송
    User *current_user = user_head;
    while (current_user != NULL) {
        send_message(current_user->socket_fd, gameover_message);
        current_user = current_user->next;
    }
    pthread_mutex_unlock(&user_mutex);

    printf("모든 클라이언트에게 GAME_OVER 메시지 전송 완료: %s", gameover_message);
    log_event("모든 클라이언트에게 GAME_OVER 메시지 전송 완료: %s", gameover_message);
}

// main 함수
int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    pthread_t tid;

    // 로그 파일 열기
    log_fp = fopen(LOG_FILE, "a");
    if (log_fp == NULL) {
        perror("로그 파일 열기 실패");
        exit(EXIT_FAILURE);
    }

    // 시그널 핸들러 설정
    signal(SIGINT, cleanup_server);
    signal(SIGTERM, cleanup_server);
    signal(SIGQUIT, cleanup_server);

    // 소켓 생성
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

    // 주소 설정
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);

    // 소켓 옵션 설정
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt 실패");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 소켓 바인딩
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("바인딩 실패");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 리슨
    if (listen(server_fd, 10) < 0) {
        perror("리슨 실패");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("서버가 포트 %d에서 리슨 중입니다...\n", SERVER_PORT);
    log_event("서버가 포트 %d에서 리슨 중입니다...\n", SERVER_PORT);

    while (1) {
        // select를 사용하여 타임아웃 설정 (시그널 처리 가능하도록)
        fd_set readfds;
        struct timeval tv;

        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);

        tv.tv_sec = 1; // 1초 타임아웃
        tv.tv_usec = 0;

        int activity = select(server_fd + 1, &readfds, NULL, NULL, &tv);

        if (activity < 0) {
            if (errno == EINTR) {
                // 시그널에 의해 인터럽트됨
                continue;
            }
            perror("select 실패");
            continue;
        }

        if (activity == 0) {
            // 타임아웃 - 계속 진행
            continue;
        }

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("어셉트 실패");
            continue;
        }

        printf("새로운 연결: 소켓 FD %d\n", new_socket);
        log_event("새로운 연결: 소켓 FD %d\n", new_socket);

        // 클라이언트 핸들러 스레드 생성
        if (pthread_create(&tid, NULL, client_handler, (void *)(intptr_t)new_socket) != 0) {
            perror("스레드 생성 실패");
            close(new_socket);
        }
        pthread_detach(tid); // 스레드 분리
    }

    close(server_fd);
    return 0;
}
