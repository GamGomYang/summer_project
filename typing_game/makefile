# Compiler
CC = gcc

# Compiler flags
CFLAGS = -lpthread

# Libraries for ncurses
NCURSES_LIB = -lncursesw

# Executable names
SERVER_EXEC = server
CLIENT_EXEC = client

# Source files
SERVER_SRC = server.c
CLIENT_SRC = client.c

# Default target
all: $(SERVER_EXEC) $(CLIENT_EXEC)

# Build server
$(SERVER_EXEC): $(SERVER_SRC)
	$(CC) $(SERVER_SRC) -o $(SERVER_EXEC) $(CFLAGS)

# Build client
$(CLIENT_EXEC): $(CLIENT_SRC)
	$(CC) $(CLIENT_SRC) -o $(CLIENT_EXEC) $(CFLAGS) $(NCURSES_LIB)

# Clean build artifacts
clean:
	rm -f $(SERVER_EXEC) $(CLIENT_EXEC)

# Rebuild all
rebuild: clean all
