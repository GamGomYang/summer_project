// server.c

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PORT 12345
#define MAX_CLIENTS 2
#define BUF_SIZE 4096 // 버퍼 크기를 늘림
#define MAX_WORDS_PER_CATEGORY 50
#define NUM_CATEGORIES 3
#define NUM_GAME_MODES 2
#define MAX_PASSAGES_PER_CATEGORY 10
#define PLACEHOLDER "_____" // 단어가 제거되었을 때 사용할 자리 표시자

// 게임 모드 목록
const char *game_modes[] = {"1. 멀티플레이 광산게임", "2. 멀티플레이 긴글 게임"};

// 단어 카테고리 및 데이터
const char *categories[] = {"동물", "나라", "사자성어"};
const char *words[NUM_CATEGORIES][MAX_WORDS_PER_CATEGORY] = {
    // 동물 카테고리 단어
    {"기린", "코끼리", "사자", "얼룩말", "강아지", "고양이", "호랑이", "여우", "늑대", "곰",
     "다람쥐", "토끼", "햄스터", "사슴", "코알라", "캥거루", "하마", "코뿔소", "침팬지", "고릴라",
     "팬더", "돌고래", "상어", "고래", "오징어", "문어", "게", "가재", "새우", "복어",
     "메기", "붕어", "송어", "연어", "잉어", "독수리", "매", "참새", "비둘기", "까치",
     "까마귀", "오리", "거위", "닭", "칠면조", "부엉이", "올빼미", "앵무새", "타조", "플라밍고"},
    // 나라 카테고리 단어
    {"한국", "미국", "일본", "중국", "프랑스", "독일", "영국", "이탈리아", "스페인", "포르투갈",
     "러시아", "캐나다", "멕시코", "브라질", "아르헨티나", "호주", "뉴질랜드", "인도", "파키스탄", "이란",
     "이라크", "이집트", "남아프리카공화국", "나이지리아", "케냐", "탄자니아", "모로코", "알제리", "터키", "그리스",
     "네덜란드", "벨기에", "스웨덴", "노르웨이", "덴마크", "핀란드", "폴란드", "체코", "오스트리아", "스위스",
     "헝가리", "루마니아", "불가리아", "우크라이나", "벨라루스", "카자흐스탄", "우즈베키스탄", "몽골", "베트남", "태국"},
    // 사자성어 카테고리 단어
    {"유비무환", "천고마비", "동병상련", "일석이조", "오리무중", "구사일생", "권선징악", "고진감래", "각골난망", "감언이설",
     "격세지감", "견물생심", "과유불급", "금상첨화", "기고만장", "대기만성", "동문서답", "동상이몽", "마이동풍", "백문불여일견",
     "배은망덕", "백발백중", "사면초가", "산전수전", "새옹지마", "설상가상", "속수무책", "수수방관", "안하무인", "양자택일",
     "어부지리", "오합지졸", "우왕좌왕", "유유자적", "일사천리", "일취월장", "임기응변", "자업자득", "적반하장", "전전긍긍",
     "천진난만", "청출어람", "침소봉대", "타산지석", "탁상공론", "파죽지세", "학수고대", "호랑이도 제 말하면 온다", "화룡점정", "환골탈태"}};

// 카테고리별 단어 개수
int word_counts[NUM_CATEGORIES] = {50, 50, 50};

// 카테고리별 긴글 목록
const char *passages[NUM_CATEGORIES][MAX_PASSAGES_PER_CATEGORY] = {
    // 동물 카테고리
    {
        "기린은 아프리카 사바나의 상징적인 동물로, 긴 목과 독특한 무늬를 가지고 있습니다.",
        "코끼리는 지능이 높고 사회적인 동물로 알려져 있으며, 가족 단위로 무리를 이루어 생활합니다.",
        "사자는 '동물의 왕'으로 불리며, 무리의 리더로서 중요한 역할을 합니다.",
        "얼룩말의 줄무늬는 개체 식별뿐만 아니라 체온 조절에도 도움을 줍니다.",
        "강아지는 사람과의 유대감이 강해 오랫동안 인간의 친구로 사랑받아 왔습니다.",
        "호랑이는 야생에서 가장 강력한 포식자 중 하나로, 은밀한 사냥 기술을 자랑합니다.",
        "여우는 지능적이고 민첩한 동물로, 다양한 환경에 적응할 수 있는 능력을 가지고 있습니다.",
        "곰은 다양한 서식지에서 살아가며, 계절에 따라 행동 패턴이 크게 달라집니다.",
        "토끼는 빠른 번식력과 민첩성으로 자연에서 중요한 역할을 합니다.",
        "캥거루는 호주 고유의 동물로, 강력한 뒷다리와 꼬리를 이용해 효율적으로 이동합니다."},
    // 나라 카테고리
    {
        "한국은 동아시아에 위치한 나라로, 풍부한 역사와 문화유산을 자랑합니다.",
        "미국은 세계적인 경제와 문화의 중심지로, 다양한 인종과 문화가 공존하는 나라입니다.",
        "일본은 기술과 전통이 조화를 이루는 나라로, 독특한 문화를 발전시켜 왔습니다.",
        "중국은 오랜 역사와 광대한 영토를 가진 나라로, 세계 경제에서 중요한 역할을 합니다.",
        "프랑스는 예술과 패션의 중심지로, 낭만적인 분위기로 유명합니다.",
        "독일은 강력한 경제력을 바탕으로 유럽의 리더 역할을 하고 있습니다.",
        "영국은 역사적인 유산과 현대적인 문명이 공존하는 나라로, 다양한 문화적 영향을 미칩니다.",
        "이탈리아는 예술, 음식, 패션 등 다양한 분야에서 세계적인 영향을 미치는 나라입니다.",
        "스페인은 활기찬 축제와 풍부한 역사로 유명하며, 아름다운 해변을 자랑합니다.",
        "포르투갈은 매력적인 해안선과 독특한 건축물로 관광객들에게 인기가 많습니다."},
    // 사자성어 카테고리
    {
        "유비무환 준비를 잘 하면 근심이 없다는 의미로, 사전에 철저한 준비의 중요성을 강조합니다.",
        "천고마비 하늘이 높고 말이 비옥하다는 뜻으로, 좋은 시기를 의미합니다.",
        "동병상련 같은 병을 앓는 사람이 서로를 불쌍히 여긴다는 뜻으로, 공감의 중요성을 나타냅니다.",
        "일석이조 한 개의 돌로 두 마리의 새를 잡는다는 뜻으로, 한 가지 노력으로 두 가지 이득을 본다는 의미입니다.",
        "오리무중 펭귄이 없는 중이란 뜻으로, 상황이 불확실하고 방향을 잃었다는 의미입니다.",
        "구사일생 아홉 번 죽을 고비를 넘기고 한 번 살아난다는 뜻으로, 매우 위험한 상황에서 벗어남을 의미합니다.",
        "권선징악 선을 권장하고 악을 처벌한다는 뜻으로, 도덕적 규범의 중요성을 강조합니다.",
        "고진감래 고생 끝에 낙이 온다는 뜻으로, 어려운 시기가 지나면 좋은 일이 온다는 의미입니다.",
        "각골난망 뼈를 새길 만큼 잊기 어렵다는 뜻으로, 깊은 인상을 남긴 일을 의미합니다.",
        "감언이설 달콤한 말과 이익을 위한 설득을 의미하며, 속임수를 경계해야 함을 나타냅니다."}};

// 카테고리별 긴글 개수
int passages_count[NUM_CATEGORIES] = {10, 10, 10};

// 현재 게임의 선택된 카테고리
int selected_category = -1;
// 현재 게임의 선택된 긴글
char current_passage[BUF_SIZE] = "";

// 클라이언트 소켓 배열
int client_sockets[MAX_CLIENTS] = {0};
pthread_mutex_t lock;
int connected_clients = 0;
pthread_cond_t start_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t mode_cond = PTHREAD_COND_INITIALIZER;

// 게임 모드 선택
int game_mode_choices[MAX_CLIENTS] = {-1, -1};
int game_mode = -1;

char game_words[MAX_WORDS_PER_CATEGORY][BUF_SIZE]; // 선택된 단어 저장
int word_count = 0;
int scores[MAX_CLIENTS] = {0};

// 긴글 게임용 변수
int passage_scores[MAX_CLIENTS] = {0};
int current_passage_index = 0;
int passage_solved = 0; // 현재 구절의 해결 상태 (0: 미해결, 1: 해결)

// 브로드캐스트 함수
void broadcast_message(const char *message) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0) {
            if (send(client_sockets[i], message, strlen(message), 0) == -1) {
                perror("send 실패");
            }
        }
    }
}

// 단어 리스트와 점수 브로드캐스트 (광산게임 전용)
void broadcast_words_and_scores() {
    char buffer[BUF_SIZE * 2] = {0};

    // 남은 단어 목록을 특별한 태그로 전송
    strcat(buffer, "WORD_LIST:");
    for (int i = 0; i < word_count; i++) {
        strcat(buffer, game_words[i]);
        if (i < word_count - 1)
            strcat(buffer, " "); // 쉼표 대신 공백으로 구분
    }
    strcat(buffer, "\n");
    broadcast_message(buffer);

    // 점수 정보를 특별한 태그로 전송
    memset(buffer, 0, sizeof(buffer));
    strcat(buffer, "SCORES:");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        char temp[32];
        snprintf(temp, sizeof(temp), "클라이언트%d: %d ", i + 1, scores[i]);
        strcat(buffer, temp);
    }
    strcat(buffer, "\n");
    broadcast_message(buffer);
}

// 카테고리 선택 함수
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
    int recv_len = recv(client_sockets[client_id], buffer, BUF_SIZE - 1, 0);
    if (recv_len <= 0) {
        printf("클라이언트%d가 연결을 종료했습니다.\n", client_id + 1);
        close(client_sockets[client_id]);
        client_sockets[client_id] = 0;
        pthread_exit(NULL);
    }
    buffer[recv_len] = '\0';
    int category_index = atoi(buffer) - 1;

    if (category_index >= 0 && category_index < NUM_CATEGORIES) {
        pthread_mutex_lock(&lock);
        selected_category = category_index;
        pthread_mutex_unlock(&lock);

        snprintf(buffer, sizeof(buffer), "클라이언트%d가 '%s'를 선택했습니다. 게임을 시작합니다.\n",
                 client_id + 1, categories[category_index]);
        broadcast_message(buffer);

        if (game_mode == 0) { // 광산게임일 경우
            // 선택된 카테고리의 단어를 게임에 사용
            word_count = word_counts[category_index];
            for (int i = 0; i < word_count; i++) {
                strncpy(game_words[i], words[category_index][i], BUF_SIZE - 1);
                game_words[i][BUF_SIZE - 1] = '\0'; // 안전한 문자열 종료
            }

            // 선택된 단어 리스트 브로드캐스트
            broadcast_words_and_scores();
        }

        // 카운트다운 메시지
        for (int i = 5; i > 0; i--) {
            snprintf(buffer, sizeof(buffer), "%d\n", i);
            broadcast_message(buffer);
            sleep(1); // 1초 대기
        }
        broadcast_message("게임 시작!\n");

        // 게임 모드에 따른 추가 작업
        if (game_mode == 1) {
            // 긴글 게임일 경우, 선택된 카테고리에서 랜덤으로 긴글 선택
            srand(time(NULL) + category_index); // 시드 설정
            int passage_index = rand() % passages_count[selected_category];
            strncpy(current_passage, passages[selected_category][passage_index], BUF_SIZE - 1);
            current_passage[BUF_SIZE - 1] = '\0'; // 안전한 문자열 종료

            // 선택된 긴글을 클라이언트에게 공유
            snprintf(buffer, sizeof(buffer), "%s\n", current_passage);
            broadcast_message(buffer);
        }

    } else {
        send(client_sockets[client_id], "잘못된 선택입니다. 다시 선택하세요.\n", BUF_SIZE, 0);
        select_category(client_id); // 재귀 호출
    }
}

// 멀티플레이 광산게임 실행 함수
void play_word_mining_game(int client_id, int client_socket) {
    char buffer[BUF_SIZE];
    // 게임 진행
    while (1) {
        memset(buffer, 0, BUF_SIZE);
        int len = recv(client_socket, buffer, BUF_SIZE - 1, 0);
        if (len <= 0) {
            printf("클라이언트%d가 연결을 종료했습니다.\n", client_id + 1);
            break;
        }

        buffer[len] = '\0';
        buffer[strcspn(buffer, "\r\n")] = 0; // 개행 및 공백 제거

        int correct = 0;

        pthread_mutex_lock(&lock);
        for (int i = 0; i < word_count; i++) {
            if (strcmp(buffer, game_words[i]) == 0) {
                correct = 1;
                scores[client_id]++;

                // 단어 제거 대신 자리 표시자로 대체
                strncpy(game_words[i], PLACEHOLDER, BUF_SIZE - 1);
                game_words[i][BUF_SIZE - 1] = '\0'; // 안전한 문자열 종료

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
}

// 멀티플레이 긴글 게임 실행 함수
void play_long_text_game(int client_id, int client_socket) {
    char buffer[BUF_SIZE];
    int initial_countdown_done = 0;

    while (1) {
        pthread_mutex_lock(&lock);
        // 승리 조건 체크
        if (passage_scores[client_id] >= 3) {
            pthread_mutex_unlock(&lock);
            break;
        }
        // 모든 구절이 끝났는지 체크
        if (current_passage_index >= passages_count[selected_category]) {
            pthread_mutex_unlock(&lock);
            break;
        }
        // 현재 구절이 아직 풀리지 않았으면 전송
        if (!passage_solved) {
            if (!initial_countdown_done) {
                // 초기 카운트다운
                for (int i = 5; i > 0; i--) {
                    snprintf(buffer, sizeof(buffer), "%d\n", i);
                    broadcast_message(buffer);
                    sleep(1); // 1초 대기
                }
                snprintf(buffer, sizeof(buffer), "게임 시작!\n");
                broadcast_message(buffer);
                initial_countdown_done = 1;
            }

            // 현재 선택된 긴글 전송
            snprintf(buffer, sizeof(buffer), "%s\n", current_passage);
            broadcast_message(buffer);
            passage_solved = 1; // 구절 전송 후 기다림
        }
        pthread_mutex_unlock(&lock);

        // 클라이언트로부터 입력 수신
        memset(buffer, 0, BUF_SIZE);
        int len = recv(client_socket, buffer, BUF_SIZE - 1, 0);
        if (len <= 0) {
            printf("클라이언트%d가 연결을 종료했습니다.\n", client_id + 1);
            break;
        }
        buffer[len] = '\0';
        buffer[strcspn(buffer, "\r\n")] = 0; // 개행 및 공백 제거

        // 입력 비교
        pthread_mutex_lock(&lock);
        if (passage_solved && strcmp(buffer, current_passage) == 0) {
            passage_scores[client_id]++;
            // 모든 클라이언트에게 알림
            snprintf(buffer, sizeof(buffer), "클라이언트%d가 정답을 맞췄습니다! 점수: %d\n", client_id + 1, passage_scores[client_id]);
            broadcast_message(buffer);
            // 다음 구절로 이동
            current_passage_index++;
            passage_solved = 0; // 다음 구절로 초기화

            // 다음 구절이 남아있으면 선택된 카테고리에서 랜덤으로 긴글 선택
            if (current_passage_index < passages_count[selected_category]) {
                srand(time(NULL) + selected_category + current_passage_index); // 시드 설정
                int passage_index = rand() % passages_count[selected_category];
                strncpy(current_passage, passages[selected_category][passage_index], BUF_SIZE - 1);
                current_passage[BUF_SIZE - 1] = '\0'; // 안전한 문자열 종료
            }
        } else {
            // 오답 처리
            snprintf(buffer, sizeof(buffer), "틀렸습니다! 다시 입력해보세요.\n");
            send(client_socket, buffer, strlen(buffer), 0);
        }
        pthread_mutex_unlock(&lock);
    }

    // 게임 종료 메시지
    if (passage_scores[client_id] >= 3) {
        snprintf(buffer, sizeof(buffer), "축하합니다! 클라이언트%d가 승리했습니다!\n", client_id + 1);
        broadcast_message(buffer);
    } else {
        snprintf(buffer, sizeof(buffer), "게임 종료! 최종 점수: %d\n", passage_scores[client_id]);
        send(client_socket, buffer, strlen(buffer), 0);
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

    // 게임 모드 선택
    char buffer[BUF_SIZE];
    snprintf(buffer, sizeof(buffer), "게임 모드를 선택하세요:\n1. 멀티플레이 광산게임\n2. 멀티플레이 긴글 게임\n");
    send(client_socket, buffer, strlen(buffer), 0);

    // 클라이언트로부터 선택 수신
    memset(buffer, 0, BUF_SIZE);
    int recv_len = recv(client_socket, buffer, BUF_SIZE - 1, 0);
    if (recv_len <= 0) {
        printf("클라이언트%d가 연결을 종료했습니다.\n", client_id + 1);
        close(client_sockets[client_id]);
        client_sockets[client_id] = 0;
        pthread_exit(NULL);
    }
    buffer[recv_len] = '\0';
    int game_mode_choice = atoi(buffer) - 1; // 선택한 게임 모드 인덱스

    // 선택 저장
    pthread_mutex_lock(&lock);
    game_mode_choices[client_id] = game_mode_choice;
    pthread_cond_broadcast(&mode_cond); // 다른 스레드에 알림
    pthread_mutex_unlock(&lock);

    // 모든 클라이언트의 선택이 완료될 때까지 대기
    pthread_mutex_lock(&lock);
    while (game_mode_choices[0] == -1 || game_mode_choices[1] == -1) {
        pthread_cond_wait(&mode_cond, &lock);
    }
    pthread_mutex_unlock(&lock);

    // 게임 모드 결정
    pthread_mutex_lock(&lock);
    if (game_mode == -1) { // 이미 결정되지 않은 경우
        if (game_mode_choices[0] == game_mode_choices[1]) {
            game_mode = game_mode_choices[0];
        } else {
            // 선택이 다를 경우 기본 게임 모드로 설정
            game_mode = 0;
            snprintf(buffer, sizeof(buffer), "선택한 게임 모드가 일치하지 않습니다. 기본 게임 모드로 진행합니다.\n");
            broadcast_message(buffer);
        }
        pthread_cond_broadcast(&start_cond); // 게임 모드 결정 완료 알림
    }
    pthread_mutex_unlock(&lock);

    // 게임 모드 결정이 완료될 때까지 대기
    pthread_mutex_lock(&lock);
    while (game_mode == -1) {
        pthread_cond_wait(&start_cond, &lock);
    }
    pthread_mutex_unlock(&lock);

    // 게임 모드에 따른 게임 시작
    if (game_mode == 0) {
        // 멀티플레이 광산게임 시작
        snprintf(buffer, sizeof(buffer), "멀티플레이 광산게임을 시작합니다!\n");
        send(client_socket, buffer, strlen(buffer), 0);

        // 첫 번째 클라이언트가 카테고리를 선택
        if (client_id == 0) {
            select_category(client_id);
            // 광산게임일 경우 단어 목록 브로드캐스트는 select_category 내에서 처리됨
        } else {
            // 다른 클라이언트는 선택을 기다림
            snprintf(buffer, sizeof(buffer), "클라이언트1이 카테고리를 선택하고 있습니다...\n");
            send(client_socket, buffer, strlen(buffer), 0);

            // 선택 완료 대기
            pthread_mutex_lock(&lock);
            while (selected_category == -1) {
                pthread_cond_wait(&start_cond, &lock);
            }
            pthread_mutex_unlock(&lock);

            // 단어와 점수 정보 수신
            // 광산게임에서는 select_category에서 이미 단어 목록과 점수 정보를 전송함
        }

        // 게임 시작 메시지
        send(client_socket, "게임 시작!\n", strlen("게임 시작!\n"), 0);

        play_word_mining_game(client_id, client_socket);
    } else if (game_mode == 1) {
        // 멀티플레이 긴글 게임 시작
        snprintf(buffer, sizeof(buffer), "멀티플레이 긴글 게임을 시작합니다!\n");
        broadcast_message(buffer);

        // 카테고리 선택을 통해 긴글 선택
        if (client_id == 0) {
            select_category(client_id);
            // 긴글 게임일 경우, 선택된 카테고리에서 이미 긴글이 선택되고 공유됨
        } else {
            // 다른 클라이언트는 카테고리 선택 완료를 기다림
            snprintf(buffer, sizeof(buffer), "클라이언트1이 카테고리를 선택하고 있습니다...\n");
            send(client_socket, buffer, strlen(buffer), 0);

            // 선택 완료 대기
            pthread_mutex_lock(&lock);
            while (selected_category == -1) {
                pthread_cond_wait(&start_cond, &lock);
            }
            pthread_mutex_unlock(&lock);

            // 클라이언트에게 선택된 긴글 공유
            snprintf(buffer, sizeof(buffer), "%s\n", current_passage);
            send(client_socket, buffer, strlen(buffer), 0);
        }

        // 게임 시작 메시지
        send(client_socket, "게임 시작!\n", strlen("게임 시작!\n"), 0);

        play_long_text_game(client_id, client_socket);
    }

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

    // 랜덤 시드 초기화
    srand(time(NULL));

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("소켓 생성 실패");
        exit(1);
    }

    // SO_REUSEADDR 옵션 설정 (빠른 재시작을 위해)
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt 실패");
        close(server_socket);
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

        // 클라이언트 연결 정보 출력 (IP와 포트)
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        printf("클라이언트 연결: %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        int *arg = malloc(sizeof(int));
        if (arg == NULL) {
            perror("메모리 할당 실패");
            close(client_socket);
            continue;
        }
        *arg = client_socket;

        if (pthread_create(&tid, NULL, handle_client, arg) != 0) {
            perror("스레드 생성 실패");
            free(arg);
            close(client_socket);
            continue;
        }
        pthread_detach(tid);
    }

    close(server_socket);
    pthread_mutex_destroy(&lock);

    return 0;
}
