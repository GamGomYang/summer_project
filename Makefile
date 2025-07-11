CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LIBS = -lncursesw

# 타겟들
all: test_main

test_main: test_main.c
	$(CC) $(CFLAGS) -o test_main test_main.c $(LIBS)

clean:
	rm -f test_main

run: test_main
	./test_main

.PHONY: all clean run 