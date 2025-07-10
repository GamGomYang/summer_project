#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 12345
#define MAX_CLIENTS 2
#define BUF_SIZE 1024
#define MAX_WORDS_PER_CATEGORY 50
#define NUM_CATEGORIES 3

// 단어 카테고리 및 데이터
const char *categories[] = {"동물", "나라", "사자성어"};
const char *words[NUM_CATEGORIES][MAX_WORDS_PER_CATEGORY] = {
    // 동물 카테고리 단어 50개 (예시로 임의의 동물 이름을 사용했습니다)
    {
        "기린", "코끼리", "사자", "얼룩말", "강아지", "고양이", "호랑이", "여우", "늑대", "곰",
        "다람쥐", "토끼", "햄스터", "사슴", "코알라", "캥거루", "하마", "코뿔소", "침팬지", "고릴라",
        "팬더", "돌고래", "상어", "고래", "오징어", "문어", "게", "가재", "새우", "복어",
        "메기", "붕어", "송어", "연어", "잉어", "독수리", "매", "참새", "비둘기", "까치",
        "까마귀", "오리", "거위", "닭", "칠면조", "부엉이", "올빼미", "앵무새", "타조", "플라밍고"},
    // 나라 카테고리 단어 50개 (예시로 임의의 나라 이름을 사용했습니다)
    {
        "한국", "미국", "일본", "중국", "프랑스", "독일", "영국", "이탈리아", "스페인", "포르투갈",
        "러시아", "캐나다", "멕시코", "브라질", "아르헨티나", "호주", "뉴질랜드", "인도", "파키스탄", "이란",
        "이라크", "이집트", "남아프리카공화국", "나이지리아", "케냐", "탄자니아", "모로코", "알제리", "터키", "그리스",
        "네덜란드", "벨기에", "스웨덴", "노르웨이", "덴마크", "핀란드", "폴란드", "체코", "오스트리아", "스위스",
        "헝가리", "루마니아", "불가리아", "우크라이나", "벨라루스", "카자흐스탄", "우즈베키스탄", "몽골", "베트남", "태국"},
    // 사자성어 카테고리 단어 50개 (예시로 임의의 사자성어를 사용했습니다)
    {
        "유비무환", "천고마비", "동병상련", "일석이조", "오리무중", "구사일생", "권선징악", "고진감래", "각골난망", "감언이설",
        "격세지감", "견물생심", "과유불급", "금상첨화", "기고만장", "대기만성", "동문서답", "동상이몽", "마이동풍", "백문불여일견",
        "배은망덕", "백발백중", "사면초가", "산전수전", "새옹지마", "설상가상", "속수무책", "수수방관", "안하무인", "양자택일",
        "어부지리", "오합지졸", "우왕좌왕", "유유자적", "일사천리", "일취월장", "임기응변", "자업자득", "적반하장", "전전긍긍",
        "천진난만", "청출어람", "침소봉대", "타산지석", "탁상공론", "파죽지세", "학수고대", "호랑이도 제 말하면 온다", "화룡점정", "환골탈태"}};

int word_counts[NUM_CATEGORIES] = {50, 50, 50}; // 각 카테고리의 단어 개수

int client_sockets[MAX_CLIENTS] = {0};
pthread_mutex_t lock;
int connected_clients = 0;
pthread_cond_t start_cond = PTHREAD_COND_INITIALIZER;

char game_words[MAX_WORDS_PER_CATEGORY][BUF_SIZE]; // 선택된 단어 저장
int word_count = 0;
int scores[MAX_CLIENTS] = {0};

// 브로드캐스트 함수
void broadcast(const char *message) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0) {
            send(client_sockets[i], message, strlen(message), 0);
        }
    }
}

// 단어 리스트와 점수 브로드캐스트
void broadcast_words_and_scores() {
    char buffer[BUF_SIZE * 2] = {0};
    snprintf(buffer, sizeof(buffer), "남은 단어: ");
    for (int i = 0; i < word_count; i++) {
        strcat(buffer, game_words[i]);
        if (i < word_count - 1)
            strcat(buffer, " ");
    }
    strcat(buffer, "\n점수: ");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        char temp[32];
        snprintf(temp, sizeof(temp), "클라이언트%d: %d ", i + 1, scores[i]);
        strcat(buffer, temp);
    }
    strcat(buffer, "\n");
    broadcast(buffer);
}

void select_category(int client_id) {
    char buffer[BUF_SIZE] = {0};
    snprintf(buffer, sizeof(buffer), "카테고리를 선택하세요:\n");
    for (int i = 0; i < NUM_CATEGORIES; i++) {
        char line[BUF_SIZE];
        snprintf(line, sizeof(line), "%d. %s\n", i + 1, categories[i]);
        strcat(buffer, line);
    }
    send(client_sockets[client_id], buffer, strlen(buffer), 0);

    // 선택받기
    memset(buffer, 0, BUF_SIZE);
    recv(client_sockets[client_id], buffer, BUF_SIZE, 0);
    int category_index = atoi(buffer) - 1;

    if (category_index >= 0 && category_index < NUM_CATEGORIES) {
        snprintf(buffer, sizeof(buffer), "클라이언트%d가 '%s'를 선택했습니다. 게임을 시작합니다.\n",
                 client_id + 1, categories[category_index]);
        broadcast(buffer);

        // 선택된 카테고리의 단어를 게임에 사용
        word_count = word_counts[category_index];
        for (int i = 0; i < word_count; i++) {
            strncpy(game_words[i], words[category_index][i], BUF_SIZE);
        }

        // 선택된 단어 리스트 브로드캐스트
        snprintf(buffer, sizeof(buffer), "선택된 단어: ");
        for (int i = 0; i < word_count; i++) {
            strcat(buffer, game_words[i]);
            if (i < word_count - 1)
                strcat(buffer, ", ");
        }
        strcat(buffer, "\n");
        broadcast(buffer);

        // 카운트다운 메시지
        for (int i = 5; i > 0; i--) {
            snprintf(buffer, sizeof(buffer), "%d\n", i);
            broadcast(buffer);
            sleep(1); // 1초 대기
        }
        broadcast("게임 시작!\n");

    } else {
        send(client_sockets[client_id], "잘못된 선택입니다. 다시 선택하세요.\n", BUF_SIZE, 0);
        select_category(client_id); // 재귀 호출로 다시 선택
    }
}

// 클라이언트 핸들러
void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    int client_id = -1;

    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == 0) {
            client_sockets[i] = client_socket;
            client_id = i;
            connected_clients++;
            break;
        }
    }

    if (connected_clients == MAX_CLIENTS) {
        pthread_cond_broadcast(&start_cond);
    } else {
        char wait_message[] = "다른 플레이어를 기다리는 중...\n";
        send(client_socket, wait_message, strlen(wait_message), 0);
    }
    pthread_mutex_unlock(&lock);

    pthread_mutex_lock(&lock);
    while (connected_clients < MAX_CLIENTS) {
        pthread_cond_wait(&start_cond, &lock);
    }
    pthread_mutex_unlock(&lock);

    printf("클라이언트 %d 연결\n", client_id + 1);

    // 첫 번째 클라이언트가 카테고리를 선택
    if (client_id == 0) {
        select_category(client_id);
        pthread_cond_broadcast(&start_cond); // 카테고리 선택 완료 알림
    } else {
        // 다른 클라이언트는 선택을 기다림
        char buffer[BUF_SIZE];
        snprintf(buffer, sizeof(buffer), "클라이언트1이 카테고리를 선택하고 있습니다...\n");
        send(client_socket, buffer, BUF_SIZE, 0);

        // 선택 완료 대기
        pthread_mutex_lock(&lock);
        pthread_cond_wait(&start_cond, &lock);
        pthread_mutex_unlock(&lock);
    }

    // 게임 시작 메시지
    send(client_socket, "게임 시작!\n", BUF_SIZE, 0);

    // 게임 진행
    char buffer[BUF_SIZE];
    while (word_count > 0) {
        memset(buffer, 0, BUF_SIZE);
        int len = recv(client_socket, buffer, BUF_SIZE - 1, 0);
        if (len <= 0)
            break;

        buffer[len] = '\0';
        buffer[strcspn(buffer, "\r\n")] = 0; // 개행 및 공백 제거

        int correct = 0;

        pthread_mutex_lock(&lock);
        for (int i = 0; i < word_count; i++) {
            if (strcmp(buffer, game_words[i]) == 0) {
                correct = 1;
                scores[client_id]++;

                // 단어 제거
                for (int j = i; j < word_count - 1; j++) {
                    strncpy(game_words[j], game_words[j + 1], BUF_SIZE);
                }
                word_count--;

                // 단어와 점수 갱신
                broadcast_words_and_scores();
                break;
            }
        }
        pthread_mutex_unlock(&lock);

        if (correct) {
            snprintf(buffer, sizeof(buffer), "정답!\n");
        } else {
            snprintf(buffer, sizeof(buffer), "오답! 다시 입력하세요.\n");
        }
        send(client_socket, buffer, strlen(buffer), 0);
    }

    // 게임 종료 메시지
    snprintf(buffer, sizeof(buffer), "게임 종료! 최종 점수: %d\n", scores[client_id]);
    send(client_socket, buffer, strlen(buffer), 0);

    close(client_socket);
    pthread_mutex_lock(&lock);
    client_sockets[client_id] = 0;
    connected_clients--;
    pthread_mutex_unlock(&lock);

    printf("클라이언트 %d 연결 종료\n", client_id + 1);
    pthread_exit(NULL);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size;
    pthread_t tid;

    pthread_mutex_init(&lock, NULL);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("소켓 생성 실패");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("바인딩 실패");
        close(server_socket);
        exit(1);
    }

    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("리슨 실패");
        close(server_socket);
        exit(1);
    }

    printf("서버가 포트 %d에서 실행 중...\n", PORT);

    while (1) {
        client_addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_socket == -1) {
            perror("클라이언트 연결 실패");
            continue;
        }

        int *arg = malloc(sizeof(int));
        *arg = client_socket;

        pthread_create(&tid, NULL, handle_client, arg);
        pthread_detach(tid);
    }

    close(server_socket);
    pthread_mutex_destroy(&lock);

    return 0;
}
