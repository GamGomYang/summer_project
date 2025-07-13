# 전체 프로젝트 Makefile

# 컴파일러 설정
CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LIBS = -lncursesw -lpthread

# 디렉토리 설정
BATTLESHIP_DIR = battle_ship/include
TYPING_DIR = typing_game
CODA_DIR = coda_module

# 타겟 설정
TARGET = start_page
BATTLESHIP_TARGET = $(BATTLESHIP_DIR)/battleship
TYPING_CLIENT = $(TYPING_DIR)/client
CODA_CLIENT = $(CODA_DIR)/client

# 소스 파일
TEST_MAIN_SRC = start_page.c

# 기본 타겟
all: $(TARGET) $(BATTLESHIP_TARGET) $(TYPING_CLIENT) $(CODA_CLIENT)

# 메인 실행 파일 컴파일
$(TARGET): $(TEST_MAIN_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

# 배틀쉽 게임 컴파일
$(BATTLESHIP_TARGET): $(BATTLESHIP_DIR)/battleship.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

# 타이핑 게임 클라이언트 컴파일
$(TYPING_CLIENT): $(TYPING_DIR)/client.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS) -lcurl -ljson-c

# coda 게임 클라이언트 컴파일
$(CODA_CLIENT): $(CODA_DIR)/client.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

# 각 게임 디렉토리의 Makefile도 실행
battleship:
	$(MAKE) -C $(BATTLESHIP_DIR)

typing:
	$(MAKE) -C $(TYPING_DIR)

coda:
	$(MAKE) -C $(CODA_DIR)

# 정리
clean:
	rm -f $(TARGET)
	$(MAKE) -C $(BATTLESHIP_DIR) clean
	$(MAKE) -C $(TYPING_DIR) clean
	$(MAKE) -C $(CODA_DIR) clean

# 실행
run: all
	./$(TARGET)

# 도움말
help:
	@echo "사용 가능한 명령어:"
	@echo "  make        - 모든 게임을 컴파일"
	@echo "  make run    - 메인 프로그램 실행"
	@echo "  make clean  - 모든 컴파일된 파일 삭제"
	@echo "  make battleship - 배틀쉽 게임만 컴파일"
	@echo "  make typing - 타이핑 게임만 컴파일"
	@echo "  make coda   - coda 게임만 컴파일"

.PHONY: all clean run help battleship typing coda 