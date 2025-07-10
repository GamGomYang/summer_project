#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_TILES 12
#define MAX_PLAYERS 2
#define TOTAL_TILES (MAX_TILES * 2)

typedef struct {
    int number;
    char color;
    int revealed;
} Tile;

typedef struct {
    Tile tiles[MAX_TILES * 2];
    int num_tiles;
} Player;

Tile deck[TOTAL_TILES];
int deck_index = 0;
/*
 * 함수: compare_tiles
 * 설명: 두 타일을 비교하여 정렬 순서를 결정
 * 입력: 첫 번째 타일과 두 번째 타일의 포인터
 * 출력: 정렬 순서를 나타내는 값
 */
int compare_tiles(const void *a, const void *b) {
    Tile *tileA = (Tile *)a;
    Tile *tileB = (Tile *)b;

    if (tileA->number != tileB->number) {
        return tileA->number - tileB->number;
    }

    return tileA->color - tileB->color;
}
/*
 * 함수: shuffle_deck
 * 설명: 덱을 무작위로 섞음
 * 입력: 없음
 * 출력: 없음
 */
void shuffle_deck() {
    srand((unsigned int)time(NULL));
    for (int i = TOTAL_TILES - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Tile temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}
/*
 * 함수: initialize_game
 * 설명: 게임을 초기화하고 플레이어에게 타일을 분배
 * 입력: 플레이어 배열
 * 출력: 없음
 */
void initialize_game(Player players[]) {
    int index = 0;
    //타일 생성
    for (int i = 1; i <= MAX_TILES; i++) {
        deck[index].number = i;
        deck[index].color = 'B';
        deck[index++].revealed = 0;
        deck[index].number = i;
        deck[index].color = 'W';
        deck[index++].revealed = 0;
    }
    //덱 섞기
    shuffle_deck();
    int tile_index = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        players[i].num_tiles = 4;
        for (int j = 0; j < players[i].num_tiles; j++) {
            players[i].tiles[j] = deck[tile_index++];
        }
        //받은 타일 정렬
        qsort(players[i].tiles, players[i].num_tiles, sizeof(Tile), compare_tiles);

    }
    deck_index = tile_index;
}

/*
 * 함수: draw_tile
 * 설명: 플레이어가 덱에서 새로운 타일을 뽑음
 * 입력: 타일을 뽑을 플레이어
 * 출력: 없음
 */
void draw_tile(Player *player) {
    if (deck_index < TOTAL_TILES) {
        player->tiles[player->num_tiles++] = deck[deck_index++];
        qsort(player->tiles, player->num_tiles, sizeof(Tile), compare_tiles);
        printf("Drew a new tile!\n");
    } else {
        printf("No more tiles to draw.\n");
    }
}

/*
 * 함수: guess_tile
 * 설명: 상대방의 타일을 추측
 * 입력: 추측할 상대방 플레이어, 타일 위치, 타일 색상, 타일 숫자
 * 출력: 추측 성공 여부
 */
int guess_tile(Player *opponent, int index, char color, int number) {
    index--;
    if (index < 0 || index >= opponent->num_tiles) {
        printf("Invalid index. Please try again.\n");
        return 0;
    }
    if (opponent->tiles[index].color == color && opponent->tiles[index].number == number) {
        opponent->tiles[index].revealed = 1;
        printf("Correct guess!\n");
        return 1;
    } else {
        printf("Wrong guess. Drawing a new tile.\n");
        return 0;
    }
}
/*
 * 함수: check_win
 * 설명: 모든 타일이 공개되었는지 확인하여 승리 조건 판단
 * 입력: 승리 조건을 확인할 상대방 플레이어
 * 출력: 승리 여부
 */
int check_win(Player *opponent) {
    for (int i = 0; i < opponent->num_tiles; i++) {
        if (!opponent->tiles[i].revealed) {
            return 0;
        }
    }
    return 1;
}

