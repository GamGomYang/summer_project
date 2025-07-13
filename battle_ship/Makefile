# 배틀쉽 게임 Makefile

# 컴파일러 설정
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g

# 라이브러리 설정
LIBS = -lncursesw -lpthread

# 소스 파일
CLIENT_SRC = include/battleship.c
SERVER_SRC = server/src/server.c server/src/gameLogic.c server/src/grid.c server/src/network.c

# 헤더 파일
CLIENT_HEADERS = include/battleship.c
SERVER_HEADERS = server/include/gameLogic.h server/include/grid.h server/include/network.h server/include/ship.h server/include/tuple.h

# 실행 파일 이름
CLIENT_TARGET = battleship_client
SERVER_TARGET = battleship_server

# 기본 타겟
all: $(CLIENT_TARGET) $(SERVER_TARGET)

# 클라이언트 컴파일
$(CLIENT_TARGET): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

# 서버 컴파일
$(SERVER_TARGET): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# 클린 타겟
clean:
	rm -f $(CLIENT_TARGET) $(SERVER_TARGET)
	rm -f *.o
	rm -f server/*.o
	rm -f server/src/*.o

# 설치 타겟 (시스템에 ncurses 설치)
install-deps:
	@echo "시스템에 필요한 라이브러리를 설치합니다..."
	@if command -v apt-get >/dev/null 2>&1; then \
		sudo apt-get update && sudo apt-get install -y libncurses5-dev libncursesw5-dev; \
	elif command -v yum >/dev/null 2>&1; then \
		sudo yum install -y ncurses-devel; \
	elif command -v brew >/dev/null 2>&1; then \
		brew install ncurses; \
	else \
		echo "패키지 매니저를 찾을 수 없습니다. 수동으로 ncurses를 설치해주세요."; \
	fi

# 실행 타겟
run-client: $(CLIENT_TARGET)
	./$(CLIENT_TARGET)

run-server: $(SERVER_TARGET)
	./$(SERVER_TARGET)

# 디버그 모드
debug: CFLAGS += -DDEBUG
debug: $(CLIENT_TARGET) $(SERVER_TARGET)

# 릴리즈 모드
release: CFLAGS += -O2 -DNDEBUG
release: clean $(CLIENT_TARGET) $(SERVER_TARGET)

# 도움말
help:
	@echo "사용 가능한 타겟:"
	@echo "  all          - 클라이언트와 서버 모두 컴파일"
	@echo "  $(CLIENT_TARGET)    - 클라이언트만 컴파일"
	@echo "  $(SERVER_TARGET)    - 서버만 컴파일"
	@echo "  clean        - 컴파일된 파일들 삭제"
	@echo "  install-deps - 필요한 라이브러리 설치"
	@echo "  run-client   - 클라이언트 실행"
	@echo "  run-server   - 서버 실행"
	@echo "  debug        - 디버그 모드로 컴파일"
	@echo "  release      - 릴리즈 모드로 컴파일"
	@echo "  help         - 이 도움말 표시"

.PHONY: all clean install-deps run-client run-server debug release help 