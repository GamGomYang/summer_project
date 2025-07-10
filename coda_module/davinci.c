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
 * �Լ�: compare_tiles
 * ����: �� Ÿ���� ���Ͽ� ���� ������ ����
 * �Է�: ù ��° Ÿ�ϰ� �� ��° Ÿ���� ������
 * ���: ���� ������ ��Ÿ���� ��
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
 * �Լ�: shuffle_deck
 * ����: ���� �������� ����
 * �Է�: ����
 * ���: ����
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
 * �Լ�: initialize_game
 * ����: ������ �ʱ�ȭ�ϰ� �÷��̾�� Ÿ���� �й�
 * �Է�: �÷��̾� �迭
 * ���: ����
 */
void initialize_game(Player players[]) {
    int index = 0;
    //Ÿ�� ����
    for (int i = 1; i <= MAX_TILES; i++) {
        deck[index].number = i;
        deck[index].color = 'B';
        deck[index++].revealed = 0;
        deck[index].number = i;
        deck[index].color = 'W';
        deck[index++].revealed = 0;
    }
    //�� ����
    shuffle_deck();
    int tile_index = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        players[i].num_tiles = 4;
        for (int j = 0; j < players[i].num_tiles; j++) {
            players[i].tiles[j] = deck[tile_index++];
        }
        //���� Ÿ�� ����
        qsort(players[i].tiles, players[i].num_tiles, sizeof(Tile), compare_tiles);

    }
    deck_index = tile_index;
}

/*
 * �Լ�: draw_tile
 * ����: �÷��̾ ������ ���ο� Ÿ���� ����
 * �Է�: Ÿ���� ���� �÷��̾�
 * ���: ����
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
 * �Լ�: guess_tile
 * ����: ������ Ÿ���� ����
 * �Է�: ������ ���� �÷��̾�, Ÿ�� ��ġ, Ÿ�� ����, Ÿ�� ����
 * ���: ���� ���� ����
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
 * �Լ�: check_win
 * ����: ��� Ÿ���� �����Ǿ����� Ȯ���Ͽ� �¸� ���� �Ǵ�
 * �Է�: �¸� ������ Ȯ���� ���� �÷��̾�
 * ���: �¸� ����
 */
int check_win(Player *opponent) {
    for (int i = 0; i < opponent->num_tiles; i++) {
        if (!opponent->tiles[i].revealed) {
            return 0;
        }
    }
    return 1;
}

